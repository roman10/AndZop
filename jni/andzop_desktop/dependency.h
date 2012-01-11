#ifndef DEPENDENCY_H
#define DEPENDENCY_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
/*our header files*/
#include "queue.h"
#include "utility.h"
#include "packetqueue.h"

/*for logs*/
//#define ANDROID_BUILD
#define LOG_LEVEL 10
#ifdef ANDROID_BUILD
	/*for android logs*/
	/*android specific headers*/
	#include <android/log.h>
	#define LOG_TAG "libandzop"
	#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
	#define LOGE(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}
#else
	/*for normal desktop app logs*/
	#define LOGI(level, ...) if (level <= LOG_LEVEL) {printf(__VA_ARGS__); printf("\n");}
	#define LOGE(level, ...) if (level <= LOG_LEVEL) {printf(__VA_ARGS__); printf("\n");}
#endif

#define SELECTIVE_DECODING			//commented: run as normal decoding mode;  uncommented: run as selective decoding mode

#define DECODE_VIDEO_THREAD		//commented: disable decoding, only dump the dependencies with BG_DUMP_THREAD ON
//[TODO]: the two flags below may not be fully compatible now??? dump and preload may conflict
//#define BG_DUMP_THREAD			//commented: no background thread running to dump or check
#define PRE_LOAD_DEP				//uncommented: enable a separate thread to pre-load the dependency files

//#define NORM_DECODE_DEBUG			//uncommented: dump dependency for normal decoding mode; should be commented at 						//selective decoding mode
//#define DUMP_SELECTED_MB_MASK			//enabled: dump the mask for the mb needed;
//#define DUMP_VIDEO_FRAME_BYTES			//enabled: dump the bytes to a binary file
//#define DUMP_SELECTIVE_DEP			//enabled: dump the relationship in memory to files

#ifdef ANDROID_BUILD
	#define MAX_FRAME_NUM_IN_GOP 50
	#define MAX_MB_H 100
	#define MAX_MB_W 100
	#define MAX_INTER_DEP_MB 4
	#define MAX_INTRA_DEP_MB 3
#else
	#define MAX_FRAME_NUM_IN_GOP 50
	#define MAX_MB_H 100
	#define MAX_MB_W 100
	#define MAX_INTER_DEP_MB 4
	#define MAX_INTRA_DEP_MB 3
#endif

#define DUMP_PACKET

//#define CLEAR_DEP_BEFORE_START

/*structure for decoded video frame*/
typedef struct VideoPicture {
    double pts;
    double delay;
    int width, height;    //the requested size/the bitmap resolution
    AVPicture data;
} VideoPicture;

AVCodecContext **gVideoCodecCtxDepList;
AVCodecContext **gVideoCodecCtxList;

VideoPicture gVideoPicture;
void* gBitmap;

struct SwsContext *gImgConvertCtx;   //[TODO]: check out why declear as global, probably for caching reason
AVFormatContext **gFormatCtxList;
AVFormatContext **gFormatCtxDepList;
int gNumOfVideoFiles;
int gCurrentDecodingVideoFileIndex;
char **gVideoFileNameList;	   //the list of video file names
int *gVideoStreamIndexList;    //video stream index
int gStFrame;

int gVideoPacketNum;         //the current frame number
//int g_dep_videoPacketNum;    //the current frame number when dumping dependency

AVPacket *gVideoPacketDepList; //the video packet for dumping dependency
AVPacket gVideoPacket;    //the original video packet
AVPacket gVideoPacket2;   //the composed video packet

int gRoiSh, gRoiSw, gRoiEh, gRoiEw;

/*the file names for the dependency relationship*/
/*FILE *g_mbPosF;
FILE *g_intraDepF;
FILE *g_interDepF;
FILE *g_dcPredF;
FILE *g_gopF;*/

int gGopStart;
int gGopEnd;

int *gZoomLevelToVideoIndex;

void get_video_info(int p_debug);
void allocate_selected_decoding_fields(int p_videoFileIndex, int _mbHeight, int _mbWidth);
void free_selected_decoding_fields(int p_videoFileIndex, int _mbHeight);
void dump_frame_to_file(int _frameNum);
int decode_a_video_packet(int p_videoFileIndex, int _roiStH, int _roiStW, int _roiEdH, int _roiEdW);
int dep_decode_a_video_packet(int p_videoFileIndex);
int load_gop_info(FILE* p_gopRecFile, int *p_startF, int *p_endF);
int if_dependency_complete(int p_videoFileIndex, int p_gopNum);
void prepare_decode_of_gop(int p_videoFileIndex, int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw);
#ifdef PRE_LOAD_DEP
void preload_pre_computation_result(int pVideoFileIndex, int pGopNum);
#endif
void load_frame_mb_stindex(int p_videoFileIndex, int pGopNum, int ifPreload);
void load_frame_mb_edindex(int p_videoFileIndex, int pGopNum, int ifPreload);
void unload_frame_mb_stindex(void);
void unload_frame_mb_edindex(void);
void unload_frame_dc_pred_direction(void);
void unload_inter_frame_mb_dependency(void);
void unload_intra_frame_mb_dependency(void);

#endif
