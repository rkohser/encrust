#include "CVideoManager.h"
#include "EncrustDefines.h"

using namespace std;

CVideoManager::CVideoManager(const std::string &sInputFile, const std::string &sOutputFile)
{
    m_pInputThread = 0;
    m_pOutputThread = 0;

    m_sInputFile = sInputFile;
    m_sOutputFile = sOutputFile;

    m_AVic = 0;
    m_AVoc = 0;
    m_AVFrame = 0;
    m_AVaudio_st = 0;
    m_AVvideo_st = 0;

    m_AVInputaudio_st = 0;
    m_AVInputvideo_st = 0;
    m_AVOutFmt = 0;

    m_AVWaitingPacket = 0;

    m_bIsAlive = false;
}

CVideoManager::~CVideoManager()
{

}

bool CVideoManager::init()
{
    av_register_all();

    packet_queue_init(&m_qInput);
    packet_queue_init(&m_qOutput);

    //Input management

    m_AVic = avformat_alloc_context();
    avformat_open_input(&m_AVic, m_sInputFile.c_str(), NULL, NULL);
    avformat_find_stream_info(m_AVic, NULL);

    for (uint i =0; i<m_AVic->nb_streams; i++) {
        AVStream *st= m_AVic->streams[i];
        AVCodecContext *avctx = st->codec;
        AVCodec *codec = avcodec_find_decoder(avctx->codec_id);
        if (!codec || avcodec_open2(avctx, codec, NULL) < 0)
            fprintf(stderr, "cannot find codecs for %s\n",
                    (avctx->codec_type == AVMEDIA_TYPE_AUDIO)? "Audio" : "Video");
        if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) m_AVInputaudio_st = st;
        if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) m_AVInputvideo_st = st;
    }

    av_dump_format(m_AVic, 0, m_sInputFile.c_str(), 0);

    if(m_AVInputvideo_st->codec->codec_id != CODEC_ID_RAWVIDEO)
    {
        cerr << "Error : input stream is not rawvideo !" << endl;
        return false;
    }

    if(m_AVInputvideo_st->codec->pix_fmt != PIX_FMT_RGB24 && m_AVInputvideo_st->codec->pix_fmt != PIX_FMT_BGR24)
    {
        cerr << "Error : input stream fmt is not BGR24 nor RGB24 !" << endl;
        return false;
    }

    //Output management
    m_AVOutFmt = av_guess_format(m_AVic->iformat->name,NULL,NULL);
    m_AVoc = avformat_alloc_context();
    m_AVoc->oformat = m_AVOutFmt;

    snprintf(m_AVoc->filename, sizeof(m_AVoc->filename), "%s", m_sOutputFile.c_str());

    m_AVOutFmt->video_codec = m_AVInputvideo_st->codec->codec_id;
    m_AVOutFmt->audio_codec = m_AVInputaudio_st->codec->codec_id;

    m_AVvideo_st = add_video_stream(m_AVoc);
    m_AVaudio_st = add_audio_stream(m_AVoc);
    av_dump_format(m_AVoc, 0, m_sOutputFile.c_str(), 1);

    /* open the output file, if needed */
    if (!(m_AVOutFmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_AVoc->pb, m_AVoc->filename, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", m_AVoc->filename);
            return false;
        }
    }

    return true;
}

bool CVideoManager::start()
{
    m_bIsAlive = true;

    m_pInputThread = new boost::thread(boost::bind(&CVideoManager::inputLoop,this));

    if(!m_pInputThread)
    {
        cerr << "Error when creating input thread !" << endl;
        return false;
    }

    m_pOutputThread = new boost::thread(boost::bind(&CVideoManager::outputLoop,this));

    if(!m_pOutputThread)
    {
        cerr << "Error when creating output thread !" << endl;
        return false;
    }

    return true;
}

void CVideoManager::stop()
{
    m_bIsAlive = false;

    pthread_cond_signal(&(m_qInput.cond));
    pthread_cond_signal(&(m_qOutput.cond));

    if(m_pInputThread)m_pInputThread->join();
}

void CVideoManager::pushFrame(IplImage * cvImage)
{
    pushPacket(iplImageToAvPacket(cvImage));
}

void CVideoManager::pushPacket(AVPacket *pkt)
{
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    if(pkt->stream_index == m_AVInputvideo_st->index)
    {
        pkt->stream_index = m_AVvideo_st->index;
        m_AVvideo_st->codec->frame_number++;
    }
    else if(pkt->stream_index == m_AVInputaudio_st->index)
    {
        pkt->stream_index = m_AVaudio_st->index;
        m_AVaudio_st->codec->frame_number++;
    }
    else
        return;

    packet_queue_put(&m_qOutput,pkt);
}

IplImage* CVideoManager::popFrame()
{
    AVPacket* pkt = new AVPacket;
    int iret = packet_queue_get(&m_qInput, pkt,1);

    while (iret && pkt->stream_index != m_AVInputvideo_st->index)
    {
        pushPacket(pkt);
        pkt = new AVPacket;
        iret = packet_queue_get(&m_qInput, pkt,1);
    }

    if(!iret)
    {
        return 0;
    }
    else
    {
        return avPacketToIplImage(pkt);
    }
}

IplImage* CVideoManager::avPacketToIplImage(AVPacket *pkt)
{
    AVFrame* avframe = avcodec_alloc_frame();
    int got_picture;

    IplImage* cvImage = cvCreateImage(cvSize(m_AVvideo_st->codec->width,m_AVvideo_st->codec->height),
                                            IPL_DEPTH_8U,
                                            3);

    int size = avcodec_decode_video2(m_AVInputvideo_st->codec,avframe,&got_picture,pkt);

    if(size > 0 && got_picture)
    {
        memcpy(cvImage->imageData,avframe->data[0],size);
        cvImage->widthStep = avframe->linesize[0];
        m_AVWaitingPacket = pkt;
    }
    else
    {
        cvReleaseImage(&cvImage);
        cvImage = 0;
        m_AVWaitingPacket = 0;
    }

    av_free(avframe);
    return cvImage;
}

AVPacket* CVideoManager::iplImageToAvPacket(IplImage * cvImage)
{
    AVPacket* pkt = m_AVWaitingPacket;
    m_AVWaitingPacket = 0;

    AVFrame* avFrame = avcodec_alloc_frame();

    int size = avpicture_fill((AVPicture*)avFrame, (u_int8_t*)cvImage->imageData,m_AVvideo_st->codec->pix_fmt,cvImage->width, cvImage->height);

    memcpy(pkt->data,(u_int8_t*)avFrame->data[0],size);

    av_free(avFrame);

    return pkt;
}

AVPacket* CVideoManager::popOutputQueue()
{
    AVPacket* pkt = new AVPacket;
    packet_queue_get(&m_qOutput, pkt,1);
    return pkt;
}

void CVideoManager::pushInputQueue(AVPacket *pkt)
{
    packet_queue_put(&m_qInput,pkt);
}

void CVideoManager::inputLoop()
{
    AVPacket pkt;

    while (m_bIsAlive)
    {
        int err = av_read_frame(m_AVic, &pkt);

        if (err)
        {
            stopRequest(0);
            break;
        }

       pushInputQueue(&pkt);
       boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }
}

void CVideoManager::outputLoop()
{
    avformat_write_header(m_AVoc,0);

    AVPacket* pkt;

    while (m_bIsAlive)
    {
        pkt = popOutputQueue();

        if(pkt)
        {
            pkt->destruct=NULL;
//            av_interleaved_write_frame(m_AVoc, pkt);
            av_write_frame(m_AVoc, pkt);
            av_destruct_packet(pkt);
            av_free_packet(pkt);
            pkt = 0;
        }

       boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    av_write_trailer(m_AVoc);
}

void CVideoManager::packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void CVideoManager::packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    pthread_mutex_lock(&q->mutex);
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
}

void CVideoManager::packet_queue_end(PacketQueue *q)
{
    packet_queue_flush(q);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

int CVideoManager::packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    /* duplicate the packet */
    if (av_dup_packet(pkt) < 0)
        return -1;

    pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    pthread_mutex_lock(&q->mutex);

    if (!q->last_pkt)

        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);

    pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int CVideoManager::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    pthread_mutex_lock(&q->mutex);

    for(;;) {
        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block || !m_bIsAlive) {
            ret = 0;
            break;
        } else {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

AVStream* CVideoManager::add_video_stream(AVFormatContext *oc)
{
    AVCodecContext *c;
    AVCodec *codec;
    AVStream *st;

    st = avformat_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    c = st->codec;
    c->codec_id = m_AVInputvideo_st->codec->codec_id;
    c->codec_type = m_AVInputvideo_st->codec->codec_type;

    /* put sample parameters */
//    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = m_AVInputvideo_st->codec->width;
    c->height = m_AVInputvideo_st->codec->height;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1.*/
    c->time_base.den = m_AVInputvideo_st->codec->time_base.den;
    c->time_base.num = m_AVInputvideo_st->codec->time_base.num;
    c->pix_fmt = m_AVInputvideo_st->codec->pix_fmt;

    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    /* open the codec */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }
//    picture = avcodec_alloc_frame();

    return st;
}

AVStream* CVideoManager::add_audio_stream(AVFormatContext *oc)
{
    AVCodecContext *c;
    AVCodec *codec;
    AVStream *st;

    st = avformat_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    c = st->codec;
    c->codec_id = m_AVInputaudio_st->codec->codec_id;
    c->codec_type = m_AVInputaudio_st->codec->codec_type;

    /* put sample parameters */
    c->sample_fmt = m_AVInputaudio_st->codec->sample_fmt;
//    c->bit_rate = 64000;
    c->sample_rate = m_AVInputaudio_st->codec->sample_rate;
    c->channels = m_AVInputaudio_st->codec->channels;
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }

    return st;
}
