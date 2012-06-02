#include "CFrameEncrustor.h"
#include "CSettingsReader.h"

#include <cv.h>
#include <highgui.h>
#include <iostream>

#define GETVERPIX(a)              (uint)((a) * pDest->height) / 100
#define GETHORPIX(a)              (uint)((a) * pDest->width) / 100


using namespace std;

CFrameEncrustor::CFrameEncrustor(CVideoManager *pGrabber, CImageBuilder *pBuilder)
{
    m_bIsAlive = false;
    m_pThread = 0;

    m_pGrabber = pGrabber;
    m_pBuilder = pBuilder;

    m_dvRelTeamTop = 0.0;
    m_dvRelInterTeam = 0.0;
    m_dvRelLogoTop = 1.0;
    m_dhRelLogoRight = 1.0;
    m_dvRelBottom = 1.0;
    m_dvRelAdMessage = 1.0;
}

CFrameEncrustor::~CFrameEncrustor()
{

}

bool CFrameEncrustor::start()
{
    m_bIsAlive = true;

    m_pThread = new boost::thread(boost::bind(&CFrameEncrustor::run, this));

    return true;
}

void CFrameEncrustor::stop()
{
    m_bIsAlive = false;

    if(m_pThread)m_pThread->join();
}

void CFrameEncrustor::run()
{
    IplImage* pFrame = 0;

    while(m_bIsAlive)
    {
        //Grab frame
        pFrame = m_pGrabber->popFrame();
        if(!pFrame)
        {
            continue;
        }

        //Check if new images to encrust
        updateFrames();

        //Encrust frame
        encrust(pFrame);

        m_pGrabber->pushFrame(pFrame);

        cvReleaseImage(&pFrame);
    }
}

void CFrameEncrustor::updateFrames()
{
    if(m_pBuilder->toUpdate())
    {
        while(!m_mToEncrust.empty())
        {
            delete m_mToEncrust.begin()->second;
             m_mToEncrust.erase(m_mToEncrust.begin());
        }

        m_mToEncrust = m_pBuilder->popFrames();
        updatePositions();
    }
}

void CFrameEncrustor::updatePositions()
{
    CSettingsReader::Positions geo = m_pBuilder->getSettingsReader()->getPositions();
    m_dvRelTeamTop = geo.vRelTeamTop;
    m_dvRelInterTeam = geo.vRelInterTeam;
    m_dvRelLogoTop = geo.vRelLogoTop;
    m_dhRelLogoRight = geo.hRelLogoRight;
    m_dvRelBottom = geo.vRelBottom;
    m_dvRelAdMessage = geo.vRelAdMessage;
}

bool CFrameEncrustor::encrust(IplImage *pDest, CFrameInfo *pFrameInfo, CvPoint &rPoint)
{
    //Size test
    if( (rPoint.x + pFrameInfo->m_pcvFrame->width) > pDest->width ||
        (rPoint.y + pFrameInfo->m_pcvFrame->height) > pDest->height)
    {
        return false;
    }

    //ROI has to be set
    CvRect rect = cvRect(rPoint.x, rPoint.y, pFrameInfo->m_pcvFrame->width, pFrameInfo->m_pcvFrame->height);
    cvSetImageROI(pDest, rect);

    if(pFrameInfo->m_pcvAlphaNotMask3) //Means real alpha blending is enabled and 16bits channels
    {
        //Build intermediate images
        IplImage* pROI      = cvCreateImage(cvGetSize(pFrameInfo->m_pcvFrame),IPL_DEPTH_16U,3);
        IplImage* pResult   = cvCreateImage(cvGetSize(pFrameInfo->m_pcvFrame),IPL_DEPTH_16U,3);
        IplImage* pResult8  = cvCreateImage(cvGetSize(pFrameInfo->m_pcvFrame),IPL_DEPTH_8U,3);

        cvMul(pDest, pFrameInfo->m_pcvAlphaNotMask3, pROI); //Get the blended background
        cvAdd(pFrameInfo->m_pcvFrame,pROI,pResult); //Overlap foreground and background
        cvConvertScaleAbs(pResult,pResult8,(1.0/255)); //Retrieve 8bit channels

        //Overlap on frame
        cvCopy(pResult8, pDest, 0); //No mask

        cvReleaseImage(&pROI);
        cvReleaseImage(&pResult);
        cvReleaseImage(&pResult8);
    }

    cvResetImageROI(pDest);

    return true;
}

bool CFrameEncrustor::encrust(IplImage *pDest)
{
    static std::map<std::string,bool> m_mPrintedError;

    string sKey;
    CvPoint position;
    CvPoint adPosition = cvPoint(0,pDest->height);
    uint uSmooth = m_pBuilder->getSettingsReader()->getGeneration().smooth;

    CImageBuilder::FrameMap::iterator itMap;
    for(itMap = m_mToEncrust.begin(); itMap != m_mToEncrust.end(); itMap++)
    {
        sKey = itMap->first;
        CFrameInfo* pFrameInfo = itMap->second;
        if(!pFrameInfo || !pFrameInfo->m_pcvFrame)
        {
//            cerr << "Error when loading \"" << itMap->first << "\" ! " << endl;
            continue;
        }

        if(sKey == "logo")
        {
            //Set ROI
            position = cvPoint(pDest->width - pFrameInfo->m_pcvFrame->width - GETHORPIX(m_dhRelLogoRight),
                               GETVERPIX(m_dvRelLogoTop));
        }
        else if(sKey == "ad") //ad should be first in the map
        {
            position = cvPoint((pDest->width - pFrameInfo->m_pcvFrame->width)/2,
                               pDest->height - pFrameInfo->m_pcvFrame->height - GETVERPIX(m_dvRelBottom));
            adPosition = position; //Save the position of the add
        }
        else if(sKey == "message")
        {
            position = cvPoint((pDest->width - pFrameInfo->m_pcvFrame->width)/2,
                               adPosition.y - GETVERPIX(m_dvRelAdMessage) - pFrameInfo->m_pcvFrame->height);
        }
        else if(sKey.find("team") != string::npos)
        {
            int iTeam = atoi(sKey.substr(4).c_str());

            position = cvPoint(0,
                               GETVERPIX(m_dvRelTeamTop)  + (iTeam-1) * (GETVERPIX(m_dvRelInterTeam) + pFrameInfo->m_pcvFrame->height));
        }

        if(!encrust(pDest,pFrameInfo,position))
        {
            if(!m_mPrintedError[sKey])
            {
                cerr << "Not able to encrust image with key " <<sKey<< endl;
            }
            m_mPrintedError[sKey] = true;
        }
        else
        {
            m_mPrintedError[sKey] = false;
        }
    }

    return true;
}
