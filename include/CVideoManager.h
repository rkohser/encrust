#ifndef CVIDEOMANAGER_H
#define CVIDEOMANAGER_H

#include <cv.h>
#include <iostream>
#include <queue>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>

#ifdef __cplusplus

#define __STDC_CONSTANT_MACROS

#ifdef _STDINT_H

#undef _STDINT_H

#endif

# include <stdint.h>

#endif

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} PacketQueue;

class CVideoManager
{
public:

    CVideoManager(const std::string& sInputFile, const std::string& sOutputFile);
    ~CVideoManager();

    bool init();

    bool start();

    void stop();

    IplImage*   popFrame();
    void        pushFrame(IplImage*);

    void        packet_queue_init(PacketQueue *q);
    void        packet_queue_flush(PacketQueue *q);
    void        packet_queue_end(PacketQueue *q);
    int         packet_queue_put(PacketQueue *q, AVPacket *pkt);
    int         packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);

private:

    IplImage*   avPacketToIplImage(AVPacket*);
    AVPacket*   iplImageToAvPacket(IplImage*);
    void        pushPacket( AVPacket*);

    void        pushInputQueue(AVPacket*);
    AVPacket*   popOutputQueue();

    void        inputLoop();
    void        outputLoop();

    AVStream*   add_audio_stream(AVFormatContext *oc);
    AVStream*   add_video_stream(AVFormatContext *oc);

    bool                m_bIsAlive;

    AVOutputFormat*     m_AVOutFmt;
    AVFormatContext*    m_AVic;
    AVFormatContext*    m_AVoc;
    AVFrame*            m_AVFrame;
    AVStream*           m_AVaudio_st;
    AVStream*           m_AVvideo_st;
    AVStream*           m_AVInputvideo_st;
    AVStream*           m_AVInputaudio_st;

    AVPacket*           m_AVWaitingPacket;

    std::string         m_sInputFile;
    std::string         m_sOutputFile;

    boost::thread*      m_pInputThread;
    boost::thread*      m_pOutputThread;

    PacketQueue         m_qInput;
    PacketQueue         m_qOutput;

};

#endif // CVIDEOMANAGER_H
