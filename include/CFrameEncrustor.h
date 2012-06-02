#ifndef CFRAMEENCRUSTOR_H
#define CFRAMEENCRUSTOR_H

#include <boost/thread.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/thread/mutex.hpp>

#include "CVideoManager.h"
#include "CImageBuilder.h"

class CFrameEncrustor
{
public:
    CFrameEncrustor(CVideoManager* pGrabber, CImageBuilder* pBuilder);
    ~CFrameEncrustor();

    /** Starts the capture, runs run();
    */
    bool start();

    /** Requests to stop the thread.
    */
    void stop();

private:

    /** Thread Loop.
    */
    void run();

    /** Updates the frames map.
    */
    void updateFrames();

    /** Updates the geometry.
    */
    void updatePositions();

    /** Processes the global encrustations.
    */
    bool encrust(IplImage* cvDest);

    /** Processes each incrustation.
    */
    bool encrust(IplImage* pDest, CFrameInfo* pFrameInfo, CvPoint& rPoint);

    bool            m_bIsAlive;
    boost::thread*  m_pThread;

    CVideoManager*  m_pGrabber;
    CImageBuilder*  m_pBuilder;

    CImageBuilder::FrameMap m_mToEncrust;

    double          m_dvRelTeamTop;
    double          m_dvRelInterTeam;
    double          m_dvRelLogoTop;
    double          m_dhRelLogoRight;
    double          m_dvRelBottom;
    double          m_dvRelAdMessage;
};

#endif // CFRAMEENCRUSTOR_H
