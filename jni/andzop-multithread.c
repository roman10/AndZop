/**
this is the wrapper of the native functions that provided for AndZop
it also glues the decoders
This part of the logic is similar to ffplay
1. parser thread parse the video to retrieve video packets and put them into packet queue
2. video thread takes a packet from packet queue, decode the video and put them into a picture/frame queue
3. render thread/main thread can retrieve a frame from the frame queue upon request and when there's at least one frame available

current implementation only works for video without sound
**/
/*android specific headers*/
#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
/*multi threads*/
#include <pthread.h>
/*standard library*/
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>
/*ffmpeg headers*/
#include <libavutil/avstring.h>
//#include <libavutil/colorspace.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>

#include <libavformat/avformat.h>

#include <libswscale/swscale.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/opt.h>
#include <libavcodec/avfft.h>

/*for android logs*/
#define LOG_TAG "libandzop"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int gsState;   //gs=>global static
char *gFileName;	//the file name of the video
pthread_t *gParseThread;
pthread_t *gVideoDecodeThread;
AVStream *gVideoStream;

/*packet queue*/
#define MAX_VIDEO_QUEUE_SIZE (5*1024*1024)i
/*structure for video packet*/
typedef struct PacketQueue {
     AVPacketList *first_pkt, *last_pkt;
     int nb_packets;
     int size;
     int abort_request;
     pthread_mutex_t *mutex;
     pthread_cond_t *cond;
} PacketQueue;
static AVPacket gFlushPkt;
PacketQueue gVideoPacketQueue;
/*structure for decoded video frame*/
typedef struct VideoPicture {
    double pts;
    double delay;
    int width, height;
    AVPicture data;
} VideoPicture;
//set the picture queue size as 1, so that we don't decode packet in advance
//because the ROI might change
#define VIDEO_PICTURE_QUEUE_SIZE 1
VideoPicture gVideoPictureQueue[VIDEO_PICTURE_QUEUE_SIZE];
int gVideoPictureQueueSize;
int gVideoPictureQueueStartIndex;
int gVideoPictureQueueEndIndex;
pthread_mutex_t *gVideoPictureQueueMutex;
pthread_cond_t *gVideoPictureQueueCond;

static int packet_queue_put(PacketQueue *pQueue, AVPacket *pPkt);
static void packet_queue_init(PacketQueue *pQueue) {
    memset(pQueue, 0, sizeof(PacketQueue));
    //pQueue->mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(pQueue->mutex, NULL);
    //pQueue->cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_init(pQueue->cond, NULL);
    packet_queue_put(pQueue, &gFlushPkt);
}
static void packet_queue_flush(PacketQueue *pQueue) {
    AVPacketList *lPktList, *lPktList1;
    pthread_mutex_lock(pQueue->mutex);
    for (lPktList = pQueue->first_pkt; lPktList != NULL; lPktList = lPktList1) {
        lPktList1 = lPktList->next;
        av_free_packet(&lPktList->pkt);
        av_freep(&lPktList);
    }
    pQueue->last_pkt = NULL;
    pQueue->first_pkt = NULL;
    pQueue->nb_packets = 0;
    pQueue->size = 0;
    pthread_mutex_unlock(pQueue->mutex);
}
static void packet_queue_end(PacketQueue *pQueue) {
    packet_queue_flush(pQueue);
    pthread_mutex_destroy(pQueue->mutex);
    pthread_cond_destroy(pQueue->cond);
}
static int packet_queue_put(PacketQueue *pQueue, AVPacket *pPkt) {
    AVPacketList *lPktList;
    if (pPkt !=  &gFlushPkt && av_dup_packet(pPkt) < 0) {
        return -1;
    }
    lPktList = av_malloc(sizeof(AVPacketList));
    if (!lPktList) {
        return -1;
    }
    lPktList->pkt = *pPkt;
    lPktList->next = NULL;
    pthread_mutex_lock(pQueue->mutex);
    if (!pQueue->last_pkt) {
        pQueue->first_pkt = lPktList;
    } else { 
        pQueue->last_pkt->next = lPktList;
    }
    pQueue->last_pkt = lPktList;
    ++pQueue->nb_packets;
    pQueue->size += lPktList->pkt.size + sizeof(*lPktList);
    pthread_cond_signal(pQueue->cond);
    pthread_mutex_unlock(pQueue->mutex);
    return 0;
}
static void packet_queue_abort(PacketQueue *pQueue) {
    pthread_mutex_lock(pQueue->mutex);
    pQueue->abort_request = 1;
    pthread_cond_signal(pQueue->cond);
    pthread_mutex_unlock(pQueue->mutex);
}
static int packet_queue_get(PacketQueue *pQueue, AVPacket *pPkt, int pBlock) {
    AVPacketList *lPktList;
    int lRet;
    pthread_mutex_lock(pQueue->mutex);
    for (;;) {
        if (pQueue->abort_request) {
            lRet = -1;
            break;
        }
        lPktList = pQueue->first_pkt;
        if (lPktList) {
            pQueue->first_pkt = lPktList->next;
            if (!pQueue->first_pkt) {
                pQueue->last_pkt = NULL;
            }
            --pQueue->nb_packets;
            pQueue->size -= lPktList->pkt.size + sizeof(*lPktList);
            *pPkt = lPktList->pkt;
            av_free(lPktList);
            lRet = 1;
            break;
        } else if (!pBlock) {
            lRet = 0;
            break;
        } else {
            pthread_cond_wait(pQueue->cond, pQueue->mutex);
        }
    }
    pthread_mutex_unlock(pQueue->mutex);
    return lRet;
}

/*decode the video into actual picture frames, done by video_decode_thread*/
static void *video_decode_thread_function(void *arg) {
   AVFrame *lVideoFrame = avcodec_alloc_frame();
   VideoPicture lVideoPicture;
   AVCodecContext *lVideoCodecCtx;
   int64_t lPtsInt;
   double lPtsDouble;
   int lRet;
   int lNumOfDecodedFrames;
   struct SwsContext *lImgConvertCtx;

   for (;;) {
       AVPacket lPkt;
       /*get a video packet from the packet queue*/       
       //use block get to ensure synchronization
       if (packet_queue_get(&gVideoPacketQueue, &lPkt, 1) < 0) {
           LOGE("Error getting a video packet from the packet queue"); 
           goto end;
       }
       //lNumOfDecodedFrames should be 1 if a video packet is successfully decoded
       avcodec_decode_video2(gVideoStream->codec, lVideoFrame, &lNumOfDecodedFrames, &lPkt);
       if (lNumOfDecodedFrames) {
           //[TODO] it's not necessary to lock the mutex all the time
           //[TODO] pts can get more complicated, suppose pts is included in pkt
           lPtsInt = lVideoFrame->pkt_pts;
           lPtsDouble = lPtsInt*av_q2d(gVideoStream->time_base);
           /*put the frame into frame/picture queue*/
           pthread_mutex_lock(gVideoPictureQueueMutex);
           //if the queue is full, then wait 
           while (gVideoPictureQueueSize >= VIDEO_PICTURE_QUEUE_SIZE) {
	       pthread_cond_wait(gVideoPictureQueueCond, gVideoPictureQueueMutex);
           }           
           //allocate the memory space for a new VideoPicture
           lVideoCodecCtx = gVideoStream->codec;
           avpicture_alloc(&lVideoPicture.data, PIX_FMT_RGB32, lVideoCodecCtx->width, lVideoCodecCtx->height);
           lVideoPicture.width = lVideoCodecCtx->width;
           lVideoPicture.height = lVideoCodecCtx->height;
           //convert the frame to RGB formati
           lImgConvertCtx = sws_getCachedContext(lImgConvertCtx, lVideoPicture.width, lVideoPicture.height, lVideoCodecCtx->pix_fmt, lVideoPicture.width, lVideoPicture.height, PIX_FMT_RGB32, 0, NULL, NULL, NULL);           
           if (lImgConvertCtx == NULL) {
               LOGE("Error initialize the video frame conversion context");
           }
           sws_scale(lImgConvertCtx, lVideoFrame->data, lVideoFrame->linesize, 0, lVideoCodecCtx->height, lVideoPicture.data.data, lVideoPicture.data.linesize);
           if (++gVideoPictureQueueEndIndex == VIDEO_PICTURE_QUEUE_SIZE) {
               gVideoPictureQueueEndIndex = 0;
           }
           gVideoPictureQueue[gVideoPictureQueueEndIndex] = lVideoPicture;
           ++gVideoPictureQueueSize;
           pthread_mutex_unlock(gVideoPictureQueueMutex);
           /*free the packet*/
           av_free_packet(&lPkt);
       }
   } 
end:
   av_free(lVideoFrame); 
}

/*parsing the video file, done by parse thread*/
static void *parse_thread_function(void *arg) {
    AVFormatContext *lFormatCtx;
    AVCodec *lVideoCodec;
    AVCodecContext *lVideoCodecCtx;
    AVPacket lVideoPacket;
    int lError;
    int lVideoStreamIndex;
    /*some global variables initialization*/
    LOGI("parse_thread_function starts!");
    pthread_mutex_init(gVideoPictureQueueMutex, NULL);
    pthread_cond_init(gVideoPictureQueueCond, NULL);
    /*register the codec, demux, and protocol*/
    extern AVCodec ff_h263_decoder;
    avcodec_register(&ff_h263_decoder);
    extern AVInputFormat ff_mov_demuxer;
    av_register_input_format(&ff_mov_demuxer);
    extern URLProtocol ff_file_protocol;
    av_register_protocol2(&ff_file_protocol, sizeof(ff_file_protocol));
    /*open the video file*/
    if ((lError = av_open_input_file(&lFormatCtx, gFileName, NULL, 0, NULL)) !=0 ) {
        LOGE("Error open video file: %d", lError);
        return;	//open file failed
    }
    /*retrieve stream information*/
    if ((lError = av_find_stream_info(lFormatCtx)) < 0) {
        LOGE("Error find stream information: %d", lError);
        return;
    }
    /*find the video stream and its decoder*/
    lVideoStreamIndex = av_find_best_stream(lFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &lVideoCodec, 0);
    if (lVideoStreamIndex == AVERROR_STREAM_NOT_FOUND) {
        LOGE("Error: cannot find a video stream");
        return;
    } 
    if (lVideoStreamIndex == AVERROR_DECODER_NOT_FOUND) {
        LOGE("Error: video stream found, but no decoder is found!");
        return;
    }   
    gVideoStream = lFormatCtx->streams[lVideoStreamIndex]; 
    /*open the codec*/
    lVideoCodecCtx = lFormatCtx->streams[lVideoStreamIndex]->codec;
    if (avcodec_open(lVideoCodecCtx, lVideoCodec) < 0) {
	LOGE("Error: cannot open the video codec!");
        return;
    }
    /*initialize the video packet queue*/    
    packet_queue_init(&gVideoPacketQueue);
    /*start another thread for decoding of the packet*/
    pthread_create(gVideoDecodeThread, NULL, video_decode_thread_function, NULL);
    /*continuously parsing the video stream for video packet*/
    while (av_read_frame(lFormatCtx, &lVideoPacket) >= 0) {
        //check if this packet is from the video stream
        if (lVideoPacket.stream_index == lVideoStreamIndex) {
	    //queue the video packet
            packet_queue_put(&gVideoPacketQueue, &lVideoPacket);
        } else {
	    //if it's not video packet, free memory
            av_free_packet(&lVideoPacket);
        }
    }
    /*close the video codec*/
    avcodec_close(lVideoCodecCtx);
    /*close the video file*/
    av_close_input_file(lFormatCtx);
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naClose(JNIEnv *pEnv, jobject pObj, jstring pFilename) {
    /**/
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
    /*get the video file name*/
    gFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    if (gFileName == NULL) {
        LOGE("Error: cannot get the video file name!");
        return;
    } 
    LOGI("video file name is %s", gFileName);
    /*create another thread for parsing video*/
    if (pthread_create(gParseThread, NULL, parse_thread_function, NULL)) {
        LOGE("Error: failed to create a native thread for parsing video file");
    }
    //*parse_thread_function(NULL);
    LOGE("initialization done");
}

/*fill in data for a bitmap*/
JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naRenderAFrame(JNIEnv * pEnv, jobject pObj, jobject pBitmap) {
    AndroidBitmapInfo lInfo;
    void* lPixels;
    int lRet;
    VideoPicture lVideoPicture;    
    int li, lj, lk;
    int lPos;
    char* ltmp;

    if (!gsState) {
        //[TODO]if not initialized, initialize 
    }
    //1. retrieve information about the bitmap
    if ((lRet = AndroidBitmap_getInfo(pEnv, pBitmap, &lInfo)) < 0) {
        LOGE("AndroidBitmap_getInfo failed! error = %d", lRet);
        return;
    }
    if (lInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGB_8888!");
        return;
    }
    //2. lock the pixel buffer and retrieve a pointer to it
    if ((lRet = AndroidBitmap_lockPixels(pEnv, pBitmap, &lPixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed! error = %d", lRet);
    }
    //3. [TODO]modify the pixel buffer
    //take a VideoPicture from the queue and read the data into lPixels
    lVideoPicture = gVideoPictureQueue[gVideoPictureQueueStartIndex++];
    --gVideoPictureQueueSize;
    for (li = 0; li < lVideoPicture.height; ++li) {
        //copy the data to lPixels line by line
        for (lj = 0; lj < lVideoPicture.width; ++lj) {
            for (lk = 0; lk < 4; ++lk) {
                lPos = li*lVideoPicture.width + lj;
                ltmp = (char *) lPixels;
                *ltmp = lVideoPicture.data.data[lk][lPos];
	        lPixels++;
            }
        }
    }
    //4. unlock pixel buffer
    AndroidBitmap_unlockPixels(pEnv, pBitmap);
}



