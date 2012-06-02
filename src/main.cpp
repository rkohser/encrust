#include <iostream>
#include <signal.h>

#include "CImageBuilder.h"
//#include "CVideoGrabber.h"
#include "CVideoManager.h"
#include "CFrameEncrustor.h"
#include "EncrustDefines.h"

using namespace std;

bool g_bStopRequested;

void stopRequest(int)
{
    if(!g_bStopRequested)
    {
        cerr << endl << "encrust : Stopping threads ..." << endl << endl << endl;
        g_bStopRequested = true;
    }
    else
    {
        cerr << endl << "encrust : Already stopping ..." << endl << endl << endl;
    }
}

int main(int argc, char *argv[])
{
    g_bStopRequested = false;

//    CVideoGrabber* pVideoGrabber = 0;
    CVideoManager* pVideoMgr = 0;
    CImageBuilder* pImageBuilder = 0;
    CFrameEncrustor* pFrameEncrustor = 0;

    //Stop management
    signal(SIGINT,stopRequest);

    //Build and init video grabber
//    if(argc == 6)
//    {
//        pVideoGrabber = new CVideoGrabber(strtol(argv[2],0,10),strtoul(argv[3],0,10),strtoul(argv[4],0,10), strtoul(argv[5],0,10));
//    }
//    else if(argc == 5)
//    {
//        pVideoGrabber = new CVideoGrabber(strtoul(argv[3],0,10),strtoul(argv[4],0,10));
//    }
//    else if(argc == 3)
//    {
//        pVideoGrabber = new CVideoGrabber(argv[1]);
//    }
//    else

    if(argc != 4)
    {
        cerr << "Usage : "<<endl;
//        cerr << "\t\tencrust <xml> <video device number> <width> <height> <fps>" << endl;
//        cerr << "\t\tencrust <xml> stdin <width> <height>" << endl;
        cerr << "\t\tencrust <xml> <input> <output> " << endl;
        return -1;
    }

    //Video manager
    pVideoMgr = new CVideoManager(argv[2], argv[3]);

    //Image builder and frame encrustor
    pImageBuilder = new CImageBuilder(argv[1]);

    if(!pVideoMgr || !pImageBuilder)
    {
        cerr << "Error when constructing objects !" << endl;
        return -1;
    }
    else if(!pVideoMgr->init() || !pImageBuilder->init())
    {
        cerr << "Error when initialising objects !" << endl;
        return -1;
    }

    //Build encrustor
    pFrameEncrustor = new CFrameEncrustor(pVideoMgr, pImageBuilder);
    if(!pFrameEncrustor)
    {
        cerr << "Error when constructing encrustor !" << endl;
        return -1;
    }

    cerr << "encrust : Starting threads ..." << endl;

    //Start the 3 threads
    pImageBuilder->start();
    pFrameEncrustor->start();
    pVideoMgr->start();

    while(!g_bStopRequested)
    {
        boost::this_thread::sleep(boost::posix_time::millisec(200));
    }

    //Stop them
    pImageBuilder->stop();
    pVideoMgr->stop();
    pFrameEncrustor->stop();

    //Free them
    delete pFrameEncrustor;
    delete pImageBuilder;
    delete pVideoMgr;

    return 0;
}
