#include "CImageBuilder.h"
#include "CSettingsReader.h"
#include "CVideoGrabber.h"

#include <SDL.h>
#include <SDL_Pango.h>
#include <SDL_image.h>

#include <sstream>

#define TEAM_NAME_SCORE_INTER_SPACE      50

using namespace std;

CImageBuilder::CImageBuilder(const std::string &sSettingsFile)
{
    m_pSettingsReader = 0;
    m_sSettingsFile = sSettingsFile;

    m_pFrameMutex = 0;
    m_pThread = 0;

    m_uhMargin  = 50;
    m_uvMargin  = 10;
    m_uhTeamMargin = 8;
    m_uvTeamMargin = 8;
    m_uradius = 12;
    m_usmooth = 5; // has to be unpair
    m_utextDpi = 100;
    m_dlogoZoom = 1.0;
    m_dteamZoom = 1.0;
    m_dadZoom = 1.0;

    m_iGeneralTeamFrameHeight = 0;
    m_iGeneralLogoWidth = 0;
    m_iGeneralTeamNameWidth = 0;
    m_iGeneralScoreWidth = 0;

    m_bToUpdate = false;
}

CImageBuilder::~CImageBuilder()
{

}

bool CImageBuilder::init()
{
    //Build the settings reader
    m_pSettingsReader = new CSettingsReader(m_sSettingsFile);
    if(!m_pSettingsReader)
    {
        cerr << "Error when constructing the CSettingsReader object." << endl;
        return false;
    }

    m_pFrameMutex = new boost::mutex;
    if(!m_pFrameMutex)
    {
        cerr << "Error when creating frame mutex !" << endl;
        return false;
    }

    return true;
}

bool CImageBuilder::generateFrames()
{
    if(m_pSettingsReader->hasChanged())
    {
        clearFrames();

        m_pSettingsReader->loadSettings();
        updateGeneration();

        //Generate message, ad and logo
        pushFrame("logo",   generatePictureFrame(m_pSettingsReader->getLogoPath(),  false, m_dlogoZoom));
        pushFrame("ad",     generatePictureFrame(m_pSettingsReader->getAdPath(),    false, m_dadZoom));
        pushFrame("message",generateMessageFrame(m_pSettingsReader->getMessage()));

        //Generate team info
        SDLTeamMap* pTeamMap = loadTeamFrames();
        SDLTeamMap::iterator itTeam;
        for ( itTeam = pTeamMap->begin(); itTeam != pTeamMap->end(); itTeam++)
        {
            pushFrame(itTeam->first, generateTeamFrame(itTeam->second));
            delete itTeam->second;
       }

        pTeamMap->clear();

        m_bToUpdate = true;
    }

    return true;
}

IplImage* CImageBuilder::aaRoundedBox1(uint uWidth, uint uHeight, double dAlphaBlending, bool bTeamFrame, uint uRadius, uint uSmoothWindow)
{
    IplImage* pAlpha = cvCreateImage(cvSize(uWidth,uHeight),IPL_DEPTH_8U,1);
    cvZero(pAlpha);

    if(uSmoothWindow <=1)
    {
        uSmoothWindow = 0;
    }

    //Draw the rounded box in one channel
    cvRectangle(pAlpha, cvPoint(uRadius,uSmoothWindow),             cvPoint(uWidth-uRadius,uHeight-uSmoothWindow),  cvScalar(255),CV_FILLED);
    cvRectangle(pAlpha, cvPoint(uWidth-uRadius,uRadius),            cvPoint(uWidth-uSmoothWindow,uHeight-uRadius),  cvScalar(255),CV_FILLED);

    //if teamframe, let the left part square
    if(bTeamFrame)
    {
        cvRectangle(pAlpha, cvPoint(0,uSmoothWindow),               cvPoint(uRadius,uHeight-uSmoothWindow),         cvScalar(255),CV_FILLED);
    }
    else
    {
        cvRectangle(pAlpha, cvPoint(uSmoothWindow,uRadius),         cvPoint(uRadius,uHeight-uRadius),               cvScalar(255),CV_FILLED);
        cvCircle(pAlpha,    cvPoint(uRadius,uRadius),                   uRadius-uSmoothWindow,                          cvScalar(255),CV_FILLED);
        cvCircle(pAlpha,    cvPoint(uRadius,uHeight-uRadius),           uRadius-uSmoothWindow,                          cvScalar(255),CV_FILLED);
    }

    cvCircle(pAlpha,    cvPoint(uWidth-uRadius,uRadius),            uRadius-uSmoothWindow,                          cvScalar(255),CV_FILLED);
    cvCircle(pAlpha,    cvPoint(uWidth-uRadius,uHeight-uRadius),    uRadius-uSmoothWindow,                          cvScalar(255),CV_FILLED);

    //Smooth for antialiasing
    if(uSmoothWindow > 1)
    {
        cvSmooth(pAlpha,pAlpha,CV_GAUSSIAN,uSmoothWindow);
    }

    //Scale for general alpha blending
    cvScale(pAlpha,pAlpha,dAlphaBlending);

    return pAlpha;
}

CFrameInfo* CImageBuilder::buildAlphaFrame(IplImage *pcvImage, IplImage *pAlphaFrame)
{
    IplImage* p3NotAlpha = 0;
    IplImage* pFinalImage = 0;
    if(pAlphaFrame)
    {
        //Build Image multiplicators
        IplImage* p3Alpha    = cvCreateImage(cvGetSize(pcvImage),IPL_DEPTH_8U,3);
        p3NotAlpha = cvCreateImage(cvGetSize(pcvImage),IPL_DEPTH_8U,3);

        cvMerge(pAlphaFrame, pAlphaFrame, pAlphaFrame, 0, p3Alpha);
        cvNot(p3Alpha, p3NotAlpha);

        //Get the blended image
        pFinalImage = cvCreateImage(cvGetSize(pcvImage),IPL_DEPTH_16U,3);
        cvMul(pcvImage, p3Alpha, pFinalImage);

        cvReleaseImage(&p3Alpha);
    }
    else
    {
        pFinalImage = pcvImage;
    }

    return new CFrameInfo(pFinalImage,p3NotAlpha);
}

IplImage* CImageBuilder::sdlToCv(SDL_Surface *pSdlSurface, double dScale)
{
    if(!pSdlSurface)
        return 0;

    //Copy image
    IplImage* pcvImage = cvCreateImage(cvSize(pSdlSurface->w,pSdlSurface->h),
                                       IPL_DEPTH_8U,
                                       pSdlSurface->format->BytesPerPixel);
    memcpy(pcvImage->imageData,pSdlSurface->pixels,pcvImage->imageSize);

    //Scale
    if(dScale != 1.0)
    {
        IplImage* pScaledImage = cvCreateImage(cvSize(int(pcvImage->width * dScale),
                                                      int(pcvImage->height * dScale)),
                                               IPL_DEPTH_8U,
                                               pcvImage->nChannels);
        cvResize(pcvImage,pScaledImage,CV_INTER_CUBIC);
        cvReleaseImage(&pcvImage);
        pcvImage = pScaledImage;
    }

    return pcvImage;
}

CFrameInfo* CImageBuilder::pngToCv(SDL_Surface *pSdlSurface, bool bAddBlending, double dScale)
{
    if(!pSdlSurface)
    {
        cerr << "Error when converting image !" << endl;
        return 0;
    }

    bool bHasAlpha = (pSdlSurface->format->BytesPerPixel == 4);

    //Copy image
    IplImage* pcvImage = cvCreateImage(cvSize(pSdlSurface->w,pSdlSurface->h),
                                       IPL_DEPTH_8U,
                                       pSdlSurface->format->BytesPerPixel);
    memcpy(pcvImage->imageData,pSdlSurface->pixels,pcvImage->imageSize);

    //Scale
    if(dScale != 1.0)
    {
        IplImage* pScaledImage = cvCreateImage(cvSize(int(pcvImage->width * dScale),
                                                      int(pcvImage->height * dScale)),
                                               IPL_DEPTH_8U,
                                               pcvImage->nChannels);
        cvResize(pcvImage,pScaledImage,CV_INTER_CUBIC);
        cvReleaseImage(&pcvImage);
        pcvImage = pScaledImage;
    }

    IplImage* pAlpha            = cvCreateImage(cvGetSize(pcvImage), IPL_DEPTH_8U, 1);
    IplImage* pcvImageNoAlpha   = cvCreateImage(cvGetSize(pcvImage), IPL_DEPTH_8U, 3);

    //Build alpha channel multiplicator
    if(bHasAlpha)
    {
        //Extract alpha channel
        cvSetImageCOI(pcvImage,4);
        cvCopy(pcvImage,pAlpha);
        cvResetImageROI(pcvImage);

        cvCvtColor(pcvImage,pcvImageNoAlpha,CV_RGBA2BGR);
    }
    else
    {
        cvCvtColor(pcvImage,pcvImageNoAlpha,CV_RGB2BGR);

        //Opaque
        cvSet(pAlpha,cvScalar(255));
    }

    if(bAddBlending)
    {
        cvScale(pAlpha,pAlpha,m_pSettingsReader->getColor().dTransparency);
    }

    cvReleaseImage(&pcvImage);

    return buildAlphaFrame(pcvImageNoAlpha,pAlpha);
}

CFrameInfo* CImageBuilder::generateMessageFrame(const string &sMessage)
{
    if(sMessage.empty())
    {
        return 0;
    }

    //Init Pango
    SDLPango_Context* context = SDLPango_CreateContext();

    //Set configuration
    SDLPango_SetDefaultColor(context,MATRIX_TRANSPARENT_BACK_WHITE_LETTER);
    SDLPango_SetDpi(context,m_utextDpi,m_utextDpi);
    SDLPango_SetMarkup(context,sMessage.c_str(),-1);

    //Draw text surface free context
    SDL_Surface* pSdlText = SDLPango_CreateSurfaceDraw(context);

    //Get text surface size
    int iWidth = SDLPango_GetLayoutWidth(context);
    int iHeight = SDLPango_GetLayoutHeight(context);

    //Build source surface
    SDL_Surface* pSdlSurface = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                                    iWidth  + m_uhMargin * 2,
                                                    iHeight + m_uvMargin * 2,
                                                    24,
                                                    (Uint32)(255 << (8 * 2)),
                                                    (Uint32)(255 << (8 * 1)),
                                                    (Uint32)(255 << (8 * 0)),
                                                    0);

    //Fill background color
    SDL_FillRect(pSdlSurface,0,SDL_MapRGB(pSdlSurface->format,
                                          m_pSettingsReader->getColor().uRed,
                                          m_pSettingsReader->getColor().uGreen,
                                          m_pSettingsReader->getColor().uBlue));

    //Overlap text
    SDL_Rect rect;
    rect.x = m_uhMargin;
    rect.y = m_uvMargin;
    SDL_BlitSurface(pSdlText,0,pSdlSurface,&rect);

    IplImage* pcvImage = sdlToCv(pSdlSurface);
    IplImage* pAlpha = aaRoundedBox1(pcvImage->width,
                                     pcvImage->height,
                                     m_pSettingsReader->getColor().dTransparency,
                                     false,
                                     m_uradius,m_usmooth);
    CFrameInfo* pFrameInfo = buildAlphaFrame(pcvImage, pAlpha);

    //Free
    SDLPango_FreeContext(context);
    SDL_FreeSurface(pSdlSurface);
    SDL_FreeSurface(pSdlText);

    return pFrameInfo;
}

CFrameInfo* CImageBuilder::generatePictureFrame(const std::string &sPicturePath, bool bAlphaBlending, double dScale)
{
    SDL_Surface* pSdlImage = IMG_Load(sPicturePath.c_str());

    CFrameInfo* pFrame = pngToCv(pSdlImage, bAlphaBlending, dScale);

    SDL_FreeSurface(pSdlImage);

    return pFrame;
}

void CImageBuilder::updateGeneration()
{
    CSettingsReader::Generation gene = m_pSettingsReader->getGeneration();
    m_uhMargin = gene.hMargin;
    m_uvMargin = gene.vMargin;
    m_uhTeamMargin = gene.hTeamMargin;
    m_uvTeamMargin = gene.vTeamMargin;
    m_uradius = gene.radius;
    m_utextDpi = gene.textDpi;

    m_usmooth = gene.smooth;
    m_dlogoZoom = gene.logoZoom;
    m_dteamZoom = gene.teamZoom;
    m_dadZoom = gene.adZoom;
}

CImageBuilder::SDLTeamMap* CImageBuilder::loadTeamFrames()
{
    SDLTeamMap* pTeamMap = new SDLTeamMap;

    m_iGeneralTeamFrameHeight = 0;
    m_iGeneralLogoWidth = 0;
    m_iGeneralTeamNameWidth = 0;
    m_iGeneralScoreWidth = 0;

    //Build Pango context
    SDLPango_Context* context = SDLPango_CreateContext();
    SDLPango_SetDefaultColor(context,MATRIX_TRANSPARENT_BACK_WHITE_LETTER);
    SDLPango_SetDpi(context,m_utextDpi,m_utextDpi);

    //Generate team frames
    const CSettingsReader::TeamMap& rMap = m_pSettingsReader->getTeamMap();
    CSettingsReader::TeamMap::const_iterator itTeam;
    for ( itTeam = rMap.begin(); itTeam != rMap.end(); itTeam++)
    {
        SDLTeam* pTeamFrames = new SDLTeam;

        const CSettingsReader::Team& team = itTeam->second;
        stringstream ssKeyBuffer;
        ssKeyBuffer << "team";
        ssKeyBuffer << itTeam->first;

        //Team text
        if(!team.sName.empty())
        {
            SDLPango_SetMarkup(context,team.sName.c_str(),-1);
            pTeamFrames->pSdlTeam = SDLPango_CreateSurfaceDraw(context);
        }

        //Score text
        if(!team.sScore.empty())
        {
            SDLPango_SetMarkup(context,team.sScore.c_str(),-1);
            pTeamFrames->pSdlScore = SDLPango_CreateSurfaceDraw(context);
        }

        //Load logo
        if(!team.sLogoPath.empty())
        {
            pTeamFrames->pSdlLogo = IMG_Load(team.sLogoPath.c_str());
        }

        pTeamMap->operator [](ssKeyBuffer.str()) = pTeamFrames;

        //Process sizes
        //Team Name
        if(pTeamFrames->pSdlTeam && pTeamFrames->pSdlTeam->w > m_iGeneralTeamNameWidth)
            m_iGeneralTeamNameWidth = pTeamFrames->pSdlTeam->w;

        //Team score
        if(pTeamFrames->pSdlScore && pTeamFrames->pSdlScore->w > m_iGeneralScoreWidth)
            m_iGeneralScoreWidth = pTeamFrames->pSdlScore->w;

        //Team Logo
        if(pTeamFrames->pSdlLogo && pTeamFrames->pSdlLogo->w > m_iGeneralLogoWidth)
            m_iGeneralLogoWidth = pTeamFrames->pSdlLogo->w;

        //Height
        if(pTeamFrames->pSdlTeam && pTeamFrames->pSdlTeam->h > m_iGeneralTeamFrameHeight)
            m_iGeneralTeamFrameHeight = pTeamFrames->pSdlTeam->h;
        if(pTeamFrames->pSdlLogo && pTeamFrames->pSdlLogo->h > m_iGeneralTeamFrameHeight)
            m_iGeneralTeamFrameHeight = pTeamFrames->pSdlLogo->h;
    }

    SDLPango_FreeContext(context);

    return pTeamMap;
}

CFrameInfo* CImageBuilder::generateTeamFrame(const SDLTeam *pTeam)
{
    if(!pTeam)
    {
        return 0;
    }

    if(!pTeam->pSdlLogo && !pTeam->pSdlScore && !pTeam->pSdlTeam)
    {
        return 0;
    }

    //Build source surface
    SDL_Surface* pSdlSurface = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                                    m_iGeneralLogoWidth + m_iGeneralTeamNameWidth + m_iGeneralScoreWidth + TEAM_NAME_SCORE_INTER_SPACE + 3*m_uhTeamMargin,
                                                    m_iGeneralTeamFrameHeight + 2*m_uvTeamMargin,
                                                    24,
                                                    (Uint32)(255 << (8 * 2)),
                                                    (Uint32)(255 << (8 * 1)),
                                                    (Uint32)(255 << (8 * 0)),
                                                    0);

    //Fill background color
    SDL_FillRect(pSdlSurface,0,SDL_MapRGB(pSdlSurface->format,
                                          m_pSettingsReader->getColor().uRed,
                                          m_pSettingsReader->getColor().uGreen,
                                          m_pSettingsReader->getColor().uBlue));

    //Overlap logo
    SDL_Rect rect;
    if(pTeam->pSdlLogo)
    {
        rect.x = m_uhTeamMargin + (m_iGeneralLogoWidth-pTeam->pSdlLogo->w)/2;
        rect.y = (pSdlSurface->h - pTeam->pSdlLogo->h)/2;
        SDL_BlitSurface(pTeam->pSdlLogo,0,pSdlSurface,&rect);
    }

    //Overlap Team text
    if(pTeam->pSdlTeam)
    {
        rect.x = m_iGeneralLogoWidth + 2*m_uhTeamMargin;
        rect.y = (pSdlSurface->h - pTeam->pSdlTeam->h)/2;
        SDL_BlitSurface(pTeam->pSdlTeam,0,pSdlSurface,&rect);
    }

    //Overlap Team score
    if(pTeam->pSdlScore)
    {
        rect.x = pSdlSurface->w - 2*m_uhTeamMargin - pTeam->pSdlScore->w;
        rect.y = (pSdlSurface->h - pTeam->pSdlScore->h)/2;
        SDL_BlitSurface(pTeam->pSdlScore,0,pSdlSurface,&rect);
    }

    IplImage* pcvImage = sdlToCv(pSdlSurface, m_dteamZoom);
    IplImage* pAlpha = aaRoundedBox1(pcvImage->width,
                                     pcvImage->height,
                                     m_pSettingsReader->getColor().dTransparency,
                                     true,
                                     m_uradius,m_usmooth);

    CFrameInfo* pFrameInfo = buildAlphaFrame(pcvImage,pAlpha);

    //Free
    SDL_FreeSurface(pSdlSurface);

    return pFrameInfo;
}

void CImageBuilder::pushFrame(const std::string &sKey, CFrameInfo* pFrameInfo)
{
    boost::lock_guard<boost::mutex> lock(*m_pFrameMutex);

    if(m_mFrames.count(sKey))
    {
        delete m_mFrames[sKey];
        m_mFrames.erase(sKey);
    }
    m_mFrames[sKey] = pFrameInfo;
}

void CImageBuilder::clearFrames()
{
    boost::lock_guard<boost::mutex> lock(*m_pFrameMutex);
    while(!m_mFrames.empty())
    {
        delete m_mFrames.begin()->second;
        m_mFrames.erase(m_mFrames.begin());
    }
    m_mFrames.clear();
}

CImageBuilder::FrameMap CImageBuilder::popFrames()
{
    FrameMap mFrames;
    {
        boost::lock_guard<boost::mutex> lock(*m_pFrameMutex);
        if(!m_mFrames.empty())
        {
            mFrames = m_mFrames;
            m_mFrames.clear();
        }
    }
    return mFrames;
}

bool CImageBuilder::toUpdate()
{
    if(m_bToUpdate)
    {
        m_bToUpdate = false;
        return true;
    }
    else
    {
        return false;
    }
}

bool CImageBuilder::start()
{
    m_bIsAlive = true;

    m_pThread = new boost::thread(boost::bind(&CImageBuilder::run,this));

    return true;
}

void CImageBuilder::stop()
{
    m_bIsAlive = false;

    if(m_pThread)m_pThread->join();
}

void CImageBuilder::run()
{
    while (m_bIsAlive)
    {
        generateFrames();

        boost::this_thread::sleep(boost::posix_time::millisec(200));
    }
}


