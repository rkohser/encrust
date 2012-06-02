#include "CVideoGrabber.h"
#include "EncrustDefines.h"
#include <sys/time.h>
#include <stdio.h>
#include <iostream>
#include <boost/thread.hpp>

using namespace std;

CVideoGrabber::CVideoGrabber(uint uWidth, uint uHeight)
{
    m_pCapture = 0;
    m_pFrame = 0;
    m_pThread = 0;
    m_pFrameMutex = 0;
    m_pFrameCondition = 0;

    m_bIsAlive = false;
    m_uFps = 1;
    m_usLoopTime = 0;
    m_usMeanCaptureTime = 0;

    m_uWidth = uWidth;
    m_uHeight = uHeight;

    //Read input from stdin
    m_sVideoSource = "stdin";
}

CVideoGrabber::CVideoGrabber(int iVideoIndex, uint uWidth, uint uHeight, uint uFps)
{
    m_pCapture = 0;
    m_pFrame = 0;
    m_pThread = 0;
    m_pFrameMutex = 0;
    m_pFrameCondition = 0;

    m_bIsAlive = false;
    m_uFps = uFps;
    m_usLoopTime = DELAY_IN_US(m_uFps);
    m_usMeanCaptureTime = 0;

    m_uWidth = uWidth;
    m_uHeight = uHeight;

    //Read input from webcam
    m_iVideoIndex = iVideoIndex;
    m_sVideoSource = "";
}

CVideoGrabber::CVideoGrabber(const std::string &sVideoFile)
{
    m_pCapture = 0;
    m_pFrame = 0;
    m_pThread = 0;
    m_pFrameMutex = 0;
    m_pFrameCondition = 0;

    m_iVideoIndex = 99;
    m_bIsAlive = false;
    m_uFps = 1;
    m_usMeanCaptureTime = 0;

    //Read input from file
    m_sVideoSource = sVideoFile;
}

CVideoGrabber::~CVideoGrabber()
{
    if(m_pCapture)          cvReleaseCapture(&m_pCapture);
    if(m_pFrame)            cvReleaseImage(&m_pFrame);
    if(m_pThread)           delete m_pThread;
    if(m_pFrameMutex)       delete m_pFrameMutex;
    if(m_pFrameCondition)   delete m_pFrameCondition;
}

bool CVideoGrabber::init()
{
    if(m_sVideoSource == "stdin")
    {
        //Read from stdin
    }
    else if(m_sVideoSource.empty())
    {
        //Read from webcam device
        m_pCapture = cvCreateCameraCapture(m_iVideoIndex);
        cvSetCaptureProperty(m_pCapture,CV_CAP_PROP_FRAME_WIDTH,m_uWidth);
        cvSetCaptureProperty(m_pCapture,CV_CAP_PROP_FRAME_HEIGHT,m_uHeight);
    }
    else
    {
        //Read from file
        m_pCapture = cvCreateFileCapture(m_sVideoSource.c_str());
        m_uFps = static_cast<uint>(cvGetCaptureProperty(m_pCapture,CV_CAP_PROP_FPS));
        m_usLoopTime = DELAY_IN_US(m_uFps);
    }

    if(m_sVideoSource != "stdin" && !m_pCapture)
    {
        cerr << "Error when creating video capture !" << endl;
        return false;
    }

    m_pFrameMutex = new boost::mutex;
    if(!m_pFrameMutex)
    {
        cerr << "Error when creating frame mutex !" << endl;
        return false;
    }

    m_pFrameCondition = new boost::condition_variable;
    if(!m_pFrameCondition)
    {
        cerr << "Error when creating frame condition !" << endl;
        return false;
    }

    return true;
}

bool CVideoGrabber::start()
{
    m_bIsAlive = true;

    if (m_sVideoSource == "stdin")
    {
        m_pThread = new boost::thread(boost::bind(&CVideoGrabber::runStdin,this));
    }
    else if (m_sVideoSource.empty())
    {
        m_pThread = new boost::thread(boost::bind(&CVideoGrabber::run,this));
    }
    else
    {
        m_pThread = new boost::thread(boost::bind(&CVideoGrabber::runVideoFile,this));
    }

    return true;
}

void CVideoGrabber::stop()
{
    m_bIsAlive = false;
    m_pFrameCondition->notify_one();

    if(m_pThread)m_pThread->join();
}

void CVideoGrabber::pushFrame(const IplImage *pImage)
{
    boost::mutex::scoped_lock lock(*m_pFrameMutex);
    m_pFrame = cvCloneImage(pImage);
    lock.unlock();
    m_pFrameCondition->notify_one();
}

IplImage* CVideoGrabber::popFrame()
{
    IplImage* pImage = 0;
    boost::mutex::scoped_lock lock(*m_pFrameMutex);

    while(m_pFrame == 0 && m_bIsAlive)
    {
        m_pFrameCondition->wait(lock);
    }

    if(m_bIsAlive)
    {
        pImage = m_pFrame;
        m_pFrame = 0;
    }

    lock.unlock();

    return pImage;
}

void CVideoGrabber::run()
{
    IplImage* pFrame = 0;

    u_int64_t usTime1 = 0, usTime2 = 0;

    u_int64_t usDelay = 0;
    int64_t usMeanCorrection = 0;
    bool bProcessMean = false;
    uint uBypass10First = 0;

    //temp
    m_usLoopTime += 94;

    while(m_bIsAlive)
    {
        uBypass10First++;


        usTime2 = usTime1;
        usTime1 = boost::posix_time::microsec_clock::universal_time().time_of_day().total_microseconds();
        if(uBypass10First > 10 && !bProcessMean) bProcessMean = true;
        if(bProcessMean) usMeanCorrection = processMeanCaptureTime(usTime1-usTime2);

        //Get the frame
        cvGrabFrame(m_pCapture);
        pFrame = cvRetrieveFrame(m_pCapture);
        if(!pFrame)
        {
            stopRequest(0);
            m_bIsAlive = false;
            break;
        }

        //Load the frame
        pushFrame(pFrame);

        //Loop cadencing
        usDelay = m_usLoopTime - boost::posix_time::microsec_clock::universal_time().time_of_day().total_microseconds() + usTime1 + usMeanCorrection;

        if(usDelay > 0)
        {
            boost::this_thread::sleep(boost::posix_time::microsec(usDelay));
        }
        else
        {
            cerr << "The chosen framerate is too high !" <<endl;
        }
    }
}

void CVideoGrabber::runVideoFile()
{
    IplImage* pFrame = 0;

    u_int64_t usTime1 = 0;

    u_int64_t usDelay = 0;

    while(m_bIsAlive)
    {
        usTime1 = boost::posix_time::microsec_clock::universal_time().time_of_day().total_microseconds();

        //Get the frame
        cvGrabFrame(m_pCapture);
        pFrame = cvRetrieveFrame(m_pCapture);
        if(!pFrame)
        {
            stopRequest(0);
            m_bIsAlive = false;
            break;
        }

        //Load the frame
        pushFrame(pFrame);

        //Loop cadencing
        usDelay = m_usLoopTime - boost::posix_time::microsec_clock::universal_time().time_of_day().total_microseconds() + usTime1;

        if(usDelay > 0)
        {
            boost::this_thread::sleep(boost::posix_time::microsec(usDelay));
        }
    }
}

void CVideoGrabber::runStdin()
{
    IplImage* pTempFrame = cvCreateImage(cvSize(m_uWidth,m_uHeight),IPL_DEPTH_8U,3);

    while(m_bIsAlive)
    {
        //Read from pipe
//        cin.peek();
//        if(cin.eof())
//        {
//            boost::this_thread::sleep(boost::posix_time::milliseconds(10));
//            continue;
//        }

        //Read from pipe
        cin.read(pTempFrame->imageData,pTempFrame->imageSize);

//        if(cin.gcount() < pTempFrame->imageSize)
//        {
//            stopRequest(0);
//            m_bIsAlive = false;
//            break;
//        }

        //Transfer to encrustor
        pushFrame(pTempFrame);
    }
}


