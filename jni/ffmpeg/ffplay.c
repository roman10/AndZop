/**
this is the wrapper of the native functions that provided for AndZop
it also glues the decoders
This part of the logic is similar to ffplay
1. parser thread parse the video to retrieve video packets and put them into packet queue
2. video thread takes a packet from packet queue, decode the video and put them into a picture/frame queue
3. render thread can retrieve a frame from the frame queue upon request and when there's at least one frame available

current implementation only works for video without sound
**/

#include <jni.h>
#include <time.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <pthread.h>

#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"		//this is not included in current build
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"

#include "libavformat/avformat.h"

#include "libswscale/swscale.h"

#include "libavcodec/opt.h"
#include "libavcodec/avfft.h"

#include <unistd.h>
#include <assert.h>

#define  LOG_TAG    "libffmpeg"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 5

#define FRAME_SKIP_FACTOR 0.05

static int sws_flags = SWS_BICUBIC;

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 2

typedef struct VideoPicture {
    double pts;                                  ///<presentation time stamp for this picture
    double target_clock;                         ///<av_gettime() time at which this should be displayed ideally
    int64_t pos;                                 ///<byte position in file
    SDL_Overlay *bmp;
    int width, height; /* source height & width */
    int allocated;
    enum PixelFormat pix_fmt;
} VideoPicture;

enum {
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

typedef struct VideoState {
    pthread_t *parse_tid;
    pthread_t *video_tid;
    pthread_t *refresh_tid;
    AVInputFormat *iformat;
    int no_background;
    int abort_request;
    int paused;
    int last_paused;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int dtg_active_format;

    double external_clock; /* external clock base */
    int64_t external_clock_time;

    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;

    double frame_timer;
    double frame_last_pts;
    double frame_last_delay;
    double video_clock;                          ///<pts of last decoded frame / predicted pts of next decoded frame
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double video_current_pts;                    ///<current displayed pts (different from video_clock if frame fifos are used)
    double video_current_pts_drift;              ///<video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t video_current_pos;                   ///<current displayed file pos
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    pthread_mutex_t *pictq_mutex;
    pthread_cond_t *pictq_cond;
    struct SwsContext *img_convert_ctx;

    //    QETimer *video_timer;
    char filename[1024];
    int width, height, xleft, ytop;

    float skip_frames;
    float skip_frames_index;
    int refresh;
} VideoState;


/* options specified by the user */
static AVInputFormat *file_iformat;
static const char *input_filename;
static const char *window_title;
static int fs_screen_width;
static int fs_screen_height;
static int screen_width = 0;
static int screen_height = 0;
static int frame_width = 0;
static int frame_height = 0;
static enum PixelFormat frame_pix_fmt = PIX_FMT_NONE;
static int video_disable;
static int wanted_stream[AVMEDIA_TYPE_NB]={
    [AVMEDIA_TYPE_VIDEO]=-1,
};
static int seek_by_bytes=-1;
static int display_disable;
static int show_status = 1;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int debug = 0;
static int debug_mv = 0;
static int step = 0;
static int thread_count = 1;
static int workaround_bugs = 1;
static int fast = 0;
static int genpts = 0;
static int lowres = 0;
static int idct = FF_IDCT_AUTO;
static enum AVDiscard skip_frame= AVDISCARD_DEFAULT;
static enum AVDiscard skip_idct= AVDISCARD_DEFAULT;
static enum AVDiscard skip_loop_filter= AVDISCARD_DEFAULT;
static int error_recognition = FF_ER_CAREFUL;
static int error_concealment = 3;
static int decoder_reorder_pts= -1;
static int autoexit;
static int exit_on_keydown;
static int exit_on_mousedown;
static int loop=1;
static int framedrop=1;

static int rdftspeed=20;

/* current context */
static int is_full_screen;
static VideoState *cur_stream;

static AVPacket flush_pkt;

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

static SDL_Surface *screen;

static int packet_queue_put(PacketQueue *q, AVPacket *pkt);

/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = PTHREAD_MUTEX_INITIALIZER;
    q->cond = PTHREAD_COND_INITIALIZER;
    packet_queue_put(q, &flush_pkt);
}

static void packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    pthread_mutex_lock(q->mutex);
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    pthread_mutex_unlock(q->mutex);
}

static void packet_queue_end(PacketQueue *q)
{
    packet_queue_flush(q);
    pthread_mutex_destroy(q->mutex);
    pthread_cond_destroy(q->cond);
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    /* duplicate the packet */
    if (pkt!=&flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;


    pthread_mutex_lock(q->mutex);

    if (!q->last_pkt)

        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    pthread_cond_signal(q->cond);

    pthread_mutex_unlock(q->mutex);
    return 0;
}

static void packet_queue_abort(PacketQueue *q)
{
    pthread_mutex_lock(q->mutex);

    q->abort_request = 1;

    pthread_cond_signal(q->cond);

    pthread_mutex_unlock(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    pthread_mutex_lock(q->mutex);

    for(;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

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
        } else if (!block) {
            ret = 0;
            break;
        } else {
            pthread_cond_wait(q->cond, q->mutex);
        }
    }
    pthread_mutex_unlock(q->mutex);
    return ret;
}

static inline void fill_rectangle(SDL_Surface *screen,
                                  int x, int y, int w, int h, int color)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_FillRect(screen, &rect, color);
}


#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}


#define BPP 1

static void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh)
{
    int wrap, wrap3, width2, skip2;
    int y, u, v, a, u1, v1, a1, w, h;
    uint8_t *lum, *cb, *cr;
    const uint8_t *p;
    const uint32_t *pal;
    int dstx, dsty, dstw, dsth;

    dstw = av_clip(rect->w, 0, imgw);
    dsth = av_clip(rect->h, 0, imgh);
    dstx = av_clip(rect->x, 0, imgw - dstw);
    dsty = av_clip(rect->y, 0, imgh - dsth);
    lum = dst->data[0] + dsty * dst->linesize[0];
    cb = dst->data[1] + (dsty >> 1) * dst->linesize[1];
    cr = dst->data[2] + (dsty >> 1) * dst->linesize[2];

    width2 = ((dstw + 1) >> 1) + (dstx & ~dstw & 1);
    skip2 = dstx >> 1;
    wrap = dst->linesize[0];
    wrap3 = rect->pict.linesize[0];
    p = rect->pict.data[0];
    pal = (const uint32_t *)rect->pict.data[1];  /* Now in YCrCb! */

    if (dsty & 1) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            p++;
            lum++;
        }
        p += wrap3 - dstw * BPP;
        lum += wrap - dstw - dstx;
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    for(h = dsth - (dsty & 1); h >= 2; h -= 2) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            p += wrap3;
            lum += wrap;

            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);

            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 2);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 2);

            cb++;
            cr++;
            p += -wrap3 + 2 * BPP;
            lum += -wrap + 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        p += wrap3 + (wrap3 - dstw * BPP);
        lum += wrap + (wrap - dstw - dstx);
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    /* handle odd height */
    if (h) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
        }
    }
}

static void video_image_display(VideoState *is)
{
    VideoPicture *vp;
    AVPicture pict;
    float aspect_ratio;
    int width, height, x, y;
    SDL_Rect rect;
    int i;

    vp = &is->pictq[is->pictq_rindex];
    if (vp->bmp) {

        /* XXX: use variable in the frame */
        if (is->video_st->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_st->sample_aspect_ratio);
        else if (is->video_st->codec->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio);
        else
            aspect_ratio = 0;
        if (aspect_ratio <= 0.0)
            aspect_ratio = 1.0;
        aspect_ratio *= (float)vp->width / (float)vp->height;

        /* XXX: we suppose the screen has a 1.0 pixel ratio */
        height = is->height;
        width = ((int)rint(height * aspect_ratio)) & ~1;
        if (width > is->width) {
            width = is->width;
            height = ((int)rint(width / aspect_ratio)) & ~1;
        }
        x = (is->width - width) / 2;
        y = (is->height - height) / 2;
        if (!is->no_background) {
            /* fill the background */
            //            fill_border(is, x, y, width, height, QERGB(0x00, 0x00, 0x00));
        } else {
            is->no_background = 0;
        }
        rect.x = is->xleft + x;
        rect.y = is->ytop  + y;
        rect.w = width;
        rect.h = height;
        SDL_DisplayYUVOverlay(vp->bmp, &rect);
    } else {
#if 0
        fill_rectangle(screen,
                       is->xleft, is->ytop, is->width, is->height,
                       QERGB(0x00, 0x00, 0x00));
#endif
    }
}

static inline int compute_mod(int a, int b)
{
    a = a % b;
    if (a >= 0)
        return a;
    else
        return a + b;
}

static int video_open(VideoState *is){
    int flags = SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL;
    int w,h;

    if(is_full_screen) flags |= SDL_FULLSCREEN;
    else               flags |= SDL_RESIZABLE;

    if (is_full_screen && fs_screen_width) {
        w = fs_screen_width;
        h = fs_screen_height;
    } else if(!is_full_screen && screen_width){
        w = screen_width;
        h = screen_height;
    }else if (is->video_st && is->video_st->codec->width){
        w = is->video_st->codec->width;
        h = is->video_st->codec->height;
    } else {
        w = 640;
        h = 480;
    }
    if(screen && is->width == screen->w && screen->w == w
       && is->height== screen->h && screen->h == h)
        return 0;

    screen = SDL_SetVideoMode(w, h, 0, flags);
    if (!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        return -1;
    }
    if (!window_title)
        window_title = input_filename;
    SDL_WM_SetCaption(window_title, window_title);

    is->width = screen->w;
    is->height = screen->h;

    return 0;
}

/* display the current picture, if any */
static void video_display(VideoState *is)
{
    if(!screen)
        video_open(cur_stream);
    if (is->video_st)
        video_image_display(is);
}

static int refresh_thread(void *opaque)
{
    VideoState *is= opaque;
    while(!is->abort_request){
        SDL_Event event;
        event.type = FF_REFRESH_EVENT;
        event.user.data1 = opaque;
        if(!is->refresh){
            is->refresh=1;
            SDL_PushEvent(&event);
        }
    }
    return 0;
}

/* get the current video clock value */
static double get_video_clock(VideoState *is)
{
    if (is->paused) {
        return is->video_current_pts;
    } else {
        return is->video_current_pts_drift + av_gettime() / 1000000.0;
    }
}

/* get the current external clock value */
static double get_external_clock(VideoState *is)
{
    int64_t ti;
    ti = av_gettime();
    return is->external_clock + ((ti - is->external_clock_time) * 1e-6);
}

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
    }
}

/* pause or resume the video */
static void stream_pause(VideoState *is)
{
    if (is->paused) {
        is->frame_timer += av_gettime() / 1000000.0 + is->video_current_pts_drift - is->video_current_pts;
        if(is->read_pause_return != AVERROR(ENOSYS)){
            is->video_current_pts = is->video_current_pts_drift + av_gettime() / 1000000.0;
        }
        is->video_current_pts_drift = is->video_current_pts - av_gettime() / 1000000.0;
    }
    is->paused = !is->paused;
}

static double compute_target_time(double frame_current_pts, VideoState *is)
{
    double delay, sync_threshold, diff;

    /* compute nominal delay */
    delay = frame_current_pts - is->frame_last_pts;
    if (delay <= 0 || delay >= 10.0) {
        /* if incorrect delay, use previous one */
        delay = is->frame_last_delay;
    } else {
        is->frame_last_delay = delay;
    }
    is->frame_last_pts = frame_current_pts;
    is->frame_timer += delay;
#if defined(DEBUG_SYNC)
    printf("video: delay=%0.3f actual_delay=%0.3f pts=%0.3f A-V=%f\n",
            delay, actual_delay, frame_current_pts, -diff);
#endif

    return is->frame_timer;
}

/* called to display each frame */
static void video_refresh_timer(void *opaque)
{
    VideoState *is = opaque;
    VideoPicture *vp;

    if (is->video_st) {
retry:
        if (is->pictq_size == 0) {
            //nothing to do, no picture to display in the que
        } else {
            double time= av_gettime()/1000000.0;
            double next_target;
            /* dequeue the picture */
            vp = &is->pictq[is->pictq_rindex];

            if(time < vp->target_clock)
                return;
            /* update current video pts */
            is->video_current_pts = vp->pts;
            is->video_current_pts_drift = is->video_current_pts - time;
            is->video_current_pos = vp->pos;
            if(is->pictq_size > 1){
                VideoPicture *nextvp= &is->pictq[(is->pictq_rindex+1)%VIDEO_PICTURE_QUEUE_SIZE];
                assert(nextvp->target_clock >= vp->target_clock);
                next_target= nextvp->target_clock;
            }else{
                next_target= vp->target_clock + is->video_clock - vp->pts; //FIXME pass durations cleanly
            }
            if(framedrop && time > next_target){
                is->skip_frames *= 1.0 + FRAME_SKIP_FACTOR;
                if(is->pictq_size > 1 || time > next_target + 0.5){
                    /* update queue size and signal for next picture */
                    if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
                        is->pictq_rindex = 0;

                    pthread_mutex_lock(is->pictq_mutex);
                    is->pictq_size--;
                    pthread_cond_signal(is->pictq_cond);
                    pthread_mutex_unlock(is->pictq_mutex);
                    goto retry;
                }
            }

            /* display picture */
            if (!display_disable)
                video_display(is);

            /* update queue size and signal for next picture */
            if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
                is->pictq_rindex = 0;

            pthread_mutex_lock(is->pictq_mutex);
            is->pictq_size--;
            pthread_cond_signal(is->pictq_cond);
            pthread_mutex_unlock(is->pictq_mutex);
        }
    } 
    if (show_status) {
        static int64_t last_time;
        int64_t cur_time;
        int vqsize;
        double av_diff;

        cur_time = av_gettime();
        if (!last_time || (cur_time - last_time) >= 30000) {
            vqsize = 0;
            if (is->video_st)
                vqsize = is->videoq.size;
            av_diff = 0;
            printf("A-V:%7.3f s:%3.1f aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
                   av_diff,
                   FFMAX(is->skip_frames-1, 0),
                   aqsize / 1024,
                   vqsize / 1024,
                   sqsize,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0);
            fflush(stdout);
            last_time = cur_time;
        }
    }
}

static void stream_close(VideoState *is)
{
    VideoPicture *vp;
    int i;
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;
    pthread_join(is->parse_tid, NULL);
    pthread_join(is->refresh_tid, NULL);

    /* free all pictures */
    for(i=0;i<VIDEO_PICTURE_QUEUE_SIZE; i++) {
        vp = &is->pictq[i];
        if (vp->bmp) {
            SDL_FreeYUVOverlay(vp->bmp);
            vp->bmp = NULL;
        }
    }
    pthread_mutex_destroy(is->pictq_mutex);
    pthread_cond_destroy(is->pictq_cond);
    pthread_mutex_destroy(is->subpq_mutex);
    pthread_cond_destroy(is->subpq_cond);
    if (is->img_convert_ctx)
        sws_freeContext(is->img_convert_ctx);
    av_free(is);
}

static void do_exit(void)
{
    if (cur_stream) {
        stream_close(cur_stream);
        cur_stream = NULL;
    }
    uninit_opts();
    if (show_status)
        printf("\n");
    SDL_Quit();
    av_log(NULL, AV_LOG_QUIET, "");
    exit(0);
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(void *opaque)
{
    VideoState *is = opaque;
    VideoPicture *vp;

    vp = &is->pictq[is->pictq_windex];

    if (vp->bmp)
        SDL_FreeYUVOverlay(vp->bmp);
    vp->width   = is->video_st->codec->width;
    vp->height  = is->video_st->codec->height;
    vp->pix_fmt = is->video_st->codec->pix_fmt;

    vp->bmp = SDL_CreateYUVOverlay(vp->width, vp->height,
                                   SDL_YV12_OVERLAY,
                                   screen);
    if (!vp->bmp || vp->bmp->pitches[0] < vp->width) {
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        fprintf(stderr, "Error: the video system does not support an image\n"
                        "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                        "to reduce the image size.\n", vp->width, vp->height );
        do_exit();
    }

    pthread_mutex_lock(is->pictq_mutex);
    vp->allocated = 1;
    pthread_cond_signal(is->pictq_cond);
    pthread_mutex_unlock(is->pictq_mutex);
}

/**
 *
 * @param pts the dts of the pkt / pts of the frame and guessed if not known
 */
static int queue_picture(VideoState *is, AVFrame *src_frame, double pts, int64_t pos)
{
    VideoPicture *vp;
    int dst_pix_fmt;
    /* wait until we have space to put a new picture */
    pthread_mutex_lock(is->pictq_mutex);

    if(is->pictq_size>=VIDEO_PICTURE_QUEUE_SIZE && !is->refresh)
        is->skip_frames= FFMAX(1.0 - FRAME_SKIP_FACTOR, is->skip_frames * (1.0-FRAME_SKIP_FACTOR));

    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&
           !is->videoq.abort_request) {
        pthread_cond_wait(is->pictq_cond, is->pictq_mutex);
    }
    pthread_mutex_unlock(is->pictq_mutex);

    if (is->videoq.abort_request)
        return -1;

    vp = &is->pictq[is->pictq_windex];

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp ||
        vp->width != is->video_st->codec->width ||
        vp->height != is->video_st->codec->height) {
        SDL_Event event;

        vp->allocated = 0;

        /* the allocation must be done in the main thread to avoid
           locking problems */
        event.type = FF_ALLOC_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);

        /* wait until the picture is allocated */
        pthread_mutex_lock(is->pictq_mutex);
        while (!vp->allocated && !is->videoq.abort_request) {
            pthread_cond_wait(is->pictq_cond, is->pictq_mutex);
        }
        pthread_mutex_unlock(is->pictq_mutex);

        if (is->videoq.abort_request)
            return -1;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        AVPicture pict;

        /* get a pointer on the bitmap */
        SDL_LockYUVOverlay (vp->bmp);

        dst_pix_fmt = PIX_FMT_YUV420P;
        memset(&pict,0,sizeof(AVPicture));
        pict.data[0] = vp->bmp->pixels[0];
        pict.data[1] = vp->bmp->pixels[2];
        pict.data[2] = vp->bmp->pixels[1];

        pict.linesize[0] = vp->bmp->pitches[0];
        pict.linesize[1] = vp->bmp->pitches[2];
        pict.linesize[2] = vp->bmp->pitches[1];

        sws_flags = av_get_int(sws_opts, "sws_flags", NULL);
        is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
            vp->width, vp->height, vp->pix_fmt, vp->width, vp->height,
            dst_pix_fmt, sws_flags, NULL, NULL, NULL);
        if (is->img_convert_ctx == NULL) {
            fprintf(stderr, "Cannot initialize the conversion context\n");
            exit(1);
        }
        sws_scale(is->img_convert_ctx, src_frame->data, src_frame->linesize,
                  0, vp->height, pict.data, pict.linesize);
        /* update the bitmap content */
        SDL_UnlockYUVOverlay(vp->bmp);

        vp->pts = pts;
        vp->pos = pos;

        /* now we can update the picture count */
        if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
            is->pictq_windex = 0;
        pthread_mutex_lock(is->pictq_mutex);
        vp->target_clock= compute_target_time(vp->pts, is);

        is->pictq_size++;
        pthread_mutex_unlock(is->pictq_mutex);
    }
    return 0;
}

/**
 * compute the exact PTS for the picture if it is omitted in the stream
 * @param pts1 the dts of the pkt / pts of the frame
 */
static int output_picture2(VideoState *is, AVFrame *src_frame, double pts1, int64_t pos)
{
    double frame_delay, pts;

    pts = pts1;

    if (pts != 0) {
        /* update video clock with pts, if present */
        is->video_clock = pts;
    } else {
        pts = is->video_clock;
    }
    /* update video clock for next frame */
    frame_delay = av_q2d(is->video_st->codec->time_base);
    /* for MPEG2, the frame can be repeated, so we update the
       clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    is->video_clock += frame_delay;

#if defined(DEBUG_SYNC) && 0
    printf("frame_type=%c clock=%0.3f pts=%0.3f\n",
           av_get_pict_type_char(src_frame->pict_type), pts, pts1);
#endif
    return queue_picture(is, src_frame, pts, pos);
}

static int get_video_frame(VideoState *is, AVFrame *frame, int64_t *pts, AVPacket *pkt)
{
    int len1, got_picture, i;

    if (packet_queue_get(&is->videoq, pkt, 1) < 0)
        return -1;

    if (pkt->data == flush_pkt.data) {
        avcodec_flush_buffers(is->video_st->codec);

        pthread_mutex_lock(is->pictq_mutex);
        //Make sure there are no long delay timers (ideally we should just flush the que but thats harder)
        for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++) {
            is->pictq[i].target_clock= 0;
        }
        while (is->pictq_size && !is->videoq.abort_request) {
            pthread_cond_wait(is->pictq_cond, is->pictq_mutex);
        }
        is->video_current_pos = -1;
        pthread_mutex_unlock(is->pictq_mutex);

        is->frame_last_pts = AV_NOPTS_VALUE;
        is->frame_last_delay = 0;
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->skip_frames = 1;
        is->skip_frames_index = 0;
        return 0;
    }

    len1 = avcodec_decode_video2(is->video_st->codec,
                                 frame, &got_picture,
                                 pkt);

    if (got_picture) {
        if (decoder_reorder_pts == -1) {
            *pts = frame->best_effort_timestamp;
        } else if (decoder_reorder_pts) {
            *pts = frame->pkt_pts;
        } else {
            *pts = frame->pkt_dts;
        }

        if (*pts == AV_NOPTS_VALUE) {
            *pts = 0;
        }

        is->skip_frames_index += 1;
        if(is->skip_frames_index >= is->skip_frames){
            is->skip_frames_index -= FFMAX(is->skip_frames, 1.0);
            return 1;
        }

    }
    return 0;
}

static int video_thread(void *arg)
{
    VideoState *is = arg;
    AVFrame *frame= avcodec_alloc_frame();
    int64_t pts_int;
    double pts;
    int ret;

    for(;;) {
        AVPacket pkt;
        while (is->paused && !is->videoq.abort_request)
            SDL_Delay(10);
        ret = get_video_frame(is, frame, &pts_int, &pkt);

        if (ret < 0) goto the_end;

        if (!ret)
            continue;

        pts = pts_int*av_q2d(is->video_st->time_base);

        ret = output_picture2(is, frame, pts,  pkt.pos);
        av_free_packet(&pkt);
        if (ret < 0)
            goto the_end;

        if (step)
            if (cur_stream)
                stream_pause(cur_stream);
    }
 the_end:
    av_free(frame);
    return 0;
}

/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = ic->streams[stream_index]->codec;

    codec = avcodec_find_decoder(avctx->codec_id);
    avctx->debug_mv = debug_mv;
    avctx->debug = debug;
    avctx->workaround_bugs = workaround_bugs;
    avctx->lowres = lowres;
    if(lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
    avctx->idct_algo= idct;
    if(fast) avctx->flags2 |= CODEC_FLAG2_FAST;
    avctx->skip_frame= skip_frame;
    avctx->skip_idct= skip_idct;
    avctx->skip_loop_filter= skip_loop_filter;
    avctx->error_recognition= error_recognition;
    avctx->error_concealment= error_concealment;
    avctx->thread_count= thread_count;

    set_context_opts(avctx, avcodec_opts[avctx->codec_type], 0, codec);

    if (!codec ||
        avcodec_open(avctx, codec) < 0)
        return -1;

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

//        is->video_current_pts_time = av_gettime();

        packet_queue_init(&is->videoq);
        pthread_create(is->video_tid, NULL,video_thread, is);
        break;
    default:
        break;
    }
    return 0;
}

static void stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        packet_queue_abort(&is->videoq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        pthread_mutex_lock(is->pictq_mutex);
        pthread_cond_signal(is->pictq_cond);
        pthread_mutex_unlock(is->pictq_mutex);

        pthread_join(is->video_tid, NULL);

        packet_queue_end(&is->videoq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    default:
        break;
    }
}

/* since we have only one decoding thread, we can use a global
   variable instead of a thread local variable */
static VideoState *global_video_state;

static int decode_interrupt_cb(void)
{
    return (global_video_state && global_video_state->abort_request);
}

/* this thread gets the stream from the disk or the network */
static int decode_thread(void *arg)
{
    VideoState *is = arg;
    AVFormatContext *ic;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    AVFormatParameters params, *ap = &params;
    int eof=0;
    int pkt_in_play_range = 0;

    ic = avformat_alloc_context();

    memset(st_index, -1, sizeof(st_index));
    is->video_stream = -1;

    global_video_state = is;
    url_set_interrupt_cb(decode_interrupt_cb);

    memset(ap, 0, sizeof(*ap));

    ap->prealloced_context = 1;
    ap->width = frame_width;
    ap->height= frame_height;
    ap->time_base= (AVRational){1, 25};
    ap->pix_fmt = frame_pix_fmt;

    set_context_opts(ic, avformat_opts, AV_OPT_FLAG_DECODING_PARAM, NULL);

    err = av_open_input_file(&ic, is->filename, is->iformat, 0, ap);
    if (err < 0) {
        print_error(is->filename, err);
        ret = -1;
        goto fail;
    }
    is->ic = ic;

    if(genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    err = av_find_stream_info(ic);
    if (err < 0) {
        fprintf(stderr, "%s: could not find codec parameters\n", is->filename);
        ret = -1;
        goto fail;
    }
    if(ic->pb)
        ic->pb->eof_reached= 0; //FIXME hack, ffplay maybe should not use url_feof() to test for the end

    if(seek_by_bytes<0)
        seek_by_bytes= !!(ic->iformat->flags & AVFMT_TS_DISCONT);

    /* if seeking requested, we execute it */
    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            fprintf(stderr, "%s: could not seek to position %0.3f\n",
                    is->filename, (double)timestamp / AV_TIME_BASE);
        }
    }

    for (i = 0; i < ic->nb_streams; i++)
        ic->streams[i]->discard = AVDISCARD_ALL;
    if (!video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (show_status) {
        av_dump_format(ic, 0, is->filename, 0);
    }


    ret=-1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret= stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    pthread_create(is->refresh_tid, NULL, refresh_thread, is);

    if (is->video_stream < 0) {
        fprintf(stderr, "%s: could not open codecs\n", is->filename);
        ret = -1;
        goto fail;
    }

    for(;;) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return= av_read_pause(ic);
            else
                av_read_play(ic);
        }
        if (is->seek_req) {
            int64_t seek_target= is->seek_pos;
            int64_t seek_min= is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            int64_t seek_max= is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
//FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                fprintf(stderr, "%s: error while seeking\n", is->ic->filename);
            }else{
                if (is->video_stream >= 0) {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
            }
            is->seek_req = 0;
            eof= 0;
        }

        /* if the queue are full, no need to read more */
        if ( is->videoq.size > MAX_QUEUE_SIZE
            || (is->videoq   .nb_packets > MIN_FRAMES || is->video_stream<0)) {
            /* wait 10 ms */
            SDL_Delay(10);
            continue;
        }
        if(eof) {
            if(is->video_stream >= 0){
                av_init_packet(pkt);
                pkt->data=NULL;
                pkt->size=0;
                pkt->stream_index= is->video_stream;
                packet_queue_put(&is->videoq, pkt);
            }
            SDL_Delay(10);
            if(is->videoq.size ==0){
                if(loop!=1 && (!loop || --loop)){
                    stream_seek(cur_stream, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
                }else if(autoexit){
                    ret=AVERROR_EOF;
                    goto fail;
                }
            }
            eof=0;
            continue;
        }
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if (ret == AVERROR_EOF || url_feof(ic->pb))
                eof=1;
            if (ic->pb && ic->pb->error)
                break;
            SDL_Delay(100); /* wait for user event */
            continue;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                (pkt->pts - ic->streams[pkt->stream_index]->start_time) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(start_time != AV_NOPTS_VALUE ? start_time : 0)/1000000
                <= ((double)duration/1000000);
        if (pkt->stream_index == is->video_stream && pkt_in_play_range) {
            packet_queue_put(&is->videoq, pkt);
        } else {
            av_free_packet(pkt);
        }
    }
    /* wait until the end */
    while (!is->abort_request) {
        SDL_Delay(100);
    }

    ret = 0;
 fail:
    /* disable interrupting */
    global_video_state = NULL;

    /* close each stream */
    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->ic) {
        av_close_input_file(is->ic);
        is->ic = NULL; /* safety */
    }
    url_set_interrupt_cb(NULL);

    if (ret != 0) {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    return 0;
}

static VideoState *stream_open(const char *filename, AVInputFormat *iformat)
{
    VideoState *is;

    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    av_strlcpy(is->filename, filename, sizeof(is->filename));
    is->iformat = iformat;
    is->ytop = 0;
    is->xleft = 0;

    /* start video display */
    is->pictq_mutex = PTHREAD_MUTEX_INITIALIZER;
    is->pictq_cond = PTHREAD_COND_INITIALIZER;

    is->subpq_mutex = PTHREAD_MUTEX_INITIALIZER;
    is->subpq_cond = PTHREAD_COND_INITIALIZER;

    pthread_create(is->parse_tid, NULL, decode_thread, is);
    if (!is->parse_tid) {
        av_free(is);
        return NULL;
    }
    return is;
}

static void stream_cycle_channel(VideoState *is, int codec_type)
{
    AVFormatContext *ic = is->ic;
    int start_index, stream_index;
    AVStream *st;

    if (codec_type == AVMEDIA_TYPE_VIDEO)
        start_index = is->video_stream;
    if (start_index < (codec_type == AVMEDIA_TYPE_SUBTITLE ? -1 : 0))
        return;
    stream_index = start_index;
    for(;;) {
        if (++stream_index >= is->ic->nb_streams)
        {
            stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = ic->streams[stream_index];
        if (st->codec->codec_type == codec_type) {
            /* check that parameters are OK */
            switch(codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                goto the_end;
            default:
                break;
            }
        }
    }
 the_end:
    stream_component_close(is, start_index);
    stream_component_open(is, stream_index);
}

static void step_to_next_frame(void)
{
    if (cur_stream) {
        /* if the stream is paused unpause it, then step */
        if (cur_stream->paused)
            stream_pause(cur_stream);
    }
    step = 1;
}

/* handle an event sent by the GUI */
static void event_loop(void)
{
    SDL_Event event;
    double incr, pos, frac;

    for(;;) {
        double x;
        SDL_WaitEvent(&event);
        switch(event.type) {
        case SDL_KEYDOWN:
            if (exit_on_keydown) {
                do_exit();
                break;
            }
            switch(event.key.keysym.sym) {
            case SDLK_ESCAPE:
            case SDLK_q:
                do_exit();
                break;
            case SDLK_s: //S: Step to next frame
                step_to_next_frame();
                break;
            case SDLK_v:
                if (cur_stream)
                    stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_LEFT:
                incr = -10.0;
                goto do_seek;
            case SDLK_RIGHT:
                incr = 10.0;
                goto do_seek;
            case SDLK_UP:
                incr = 60.0;
                goto do_seek;
            default:
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (exit_on_mousedown) {
                do_exit();
                break;
            }
        case SDL_MOUSEMOTION:
            if(event.type ==SDL_MOUSEBUTTONDOWN){
                x= event.button.x;
            }else{
                if(event.motion.state != SDL_PRESSED)
                    break;
                x= event.motion.x;
            }
            if (cur_stream) {
                if(seek_by_bytes || cur_stream->ic->duration<=0){
                    uint64_t size=  avio_size(cur_stream->ic->pb);
                    stream_seek(cur_stream, size*x/cur_stream->width, 0, 1);
                }else{
                    int64_t ts;
                    int ns, hh, mm, ss;
                    int tns, thh, tmm, tss;
                    tns = cur_stream->ic->duration/1000000LL;
                    thh = tns/3600;
                    tmm = (tns%3600)/60;
                    tss = (tns%60);
                    frac = x/cur_stream->width;
                    ns = frac*tns;
                    hh = ns/3600;
                    mm = (ns%3600)/60;
                    ss = (ns%60);
                    fprintf(stderr, "Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)       \n", frac*100,
                            hh, mm, ss, thh, tmm, tss);
                    ts = frac*cur_stream->ic->duration;
                    if (cur_stream->ic->start_time != AV_NOPTS_VALUE)
                        ts += cur_stream->ic->start_time;
                    stream_seek(cur_stream, ts, 0, 0);
                }
            }
            break;
        case SDL_VIDEORESIZE:
            if (cur_stream) {
                screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
                                          SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL);
                screen_width = cur_stream->width = event.resize.w;
                screen_height= cur_stream->height= event.resize.h;
            }
            break;
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            do_exit();
            break;
        case FF_ALLOC_EVENT:
            video_open(event.user.data1);
            alloc_picture(event.user.data1);
            break;
        case FF_REFRESH_EVENT:
            video_refresh_timer(event.user.data1);
            cur_stream->refresh=0;
            break;
        default:
            break;
        }
    }
}

/* initialize the video decoding*/
JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naInit(JNIEnv * env, jobject  obj) {
     int flags;

    av_log_set_flags(AV_LOG_SKIP_REPEATED);

    /* register all codecs, demux and protocols */
    //avcodec_register_all();
    //h264 for video and aac for audio
    extern AVCodec ff_h263_decoder;
    avcodec_register(&ff_h263_decoder);
    //extern AVCodec ff_aac_decoder;
    //avcodec_register(&ff_aac_decoder);
    //demux and protocols: mov and file respectively
    //av_register_all();
    extern AVInputFormat ff_mov_demuxer;
    av_register_input_format(&ff_mov_demuxer);
    extern URLProtocol ff_file_protocol;
    av_register_protocol2(&ff_file_protocol, sizeof(ff_file_protocol));

    init_opts();

    if (!input_filename) {
        exit(1);
    }

    if (display_disable) {
        video_disable = 1;
    }

    av_init_packet(&flush_pkt);
    flush_pkt.data= "FLUSH";

    cur_stream = stream_open(input_filename, file_iformat);

    return;
}

/* fill in data for a bitmap */
JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naRenderAFrame(JNIEnv * env, jobject  obj, jobject pBitmap) {
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;
    static Stats       stats;
    static int         init;

    if (!init) {
        init_tables();
        stats_init(&stats);
        init = 1;
    }
    //1. retrieve information about bitmap
    if ((ret = AndroidBitmap_getInfo(env, pBitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        LOGE("Bitmap format is not RGB_565 !");
        return;
    }
    //2. lock pixel buffer and retrieve a pointer to it
    if ((ret = AndroidBitmap_lockPixels(env, pBitmap, &pixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }
    //3. modify the pixel buffer
    fill_plasma(&info, pixels, time_ms);
    //4. unlock the buffer
    AndroidBitmap_unlockPixels(env, pBitmap);
}
