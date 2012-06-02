#ifndef CVIDEOGRABBER_H
#define CVIDEOGRABBER_H

#include "highgui.h"
#include <sys/time.h>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>
#include <queue>

#define DELAY_IN_US(a)  (1000000/(a))

class CVideoGrabber
{

public:

    CVideoGrabber(const std::string& sVideoFile);
    CVideoGrabber(uint uWidth, uint uHeight);
    CVideoGrabber(int iVideoIndex, uint uWidth, uint uHeight, uint uFps);
    ~CVideoGrabber();

    /** Creates the video capture.
    */
    bool init();

    /** Starts the capture, runs run();
    */
    bool start();

    /** Requests to stop the thread.
    */
    void stop();

    /** Pops a frame from the frame queue.
    */
    IplImage* popFrame();

    /** Accessors.
    */
    uint getFramerate() const { return m_uFps;}

private:

    /** Thread Loop.
    */
    void run();

    /** Thread Loop.
    */
    void runVideoFile();

    /** Thread Loop.
    */
    void runStdin();


    /** Pushes a frame in the queue.
    */
    void pushFrame(const IplImage* pImage);

    /** Processes the floating mean capture time in microsecs.
    */
    inline int64_t processMeanCaptureTime(u_int64_t);

    bool            m_bIsAlive;
    CvCapture*      m_pCapture;
    uint            m_uFps;
    int64_t         m_usLoopTime;
    uint            m_uWidth;
    uint            m_uHeight;
    int             m_iVideoIndex;
    std::string     m_sVideoSource;
    boost::thread*  m_pThread;

    std::queue<IplImage*>   m_qFrames;
    IplImage*               m_pFrame;
    boost::mutex*           m_pFrameMutex;
    boost::condition_variable* m_pFrameCondition;

    u_int64_t       m_usMeanCaptureTime;

};

int64_t CVideoGrabber::processMeanCaptureTime(u_int64_t usLastCaptureTime)
{
    static uint uFrameCounter = 0;
    uFrameCounter ++;

    m_usMeanCaptureTime = ( (uFrameCounter-1) * m_usMeanCaptureTime + usLastCaptureTime ) / uFrameCounter;
    return (m_usLoopTime - m_usMeanCaptureTime);
}

#endif // CVIDEOGRABBER_H
