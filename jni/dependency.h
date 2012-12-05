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

#include "compile.h"

#define LOG_LEVEL 0
/*for logs*/
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

int gPreloadGopNum;

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
int gNextGopStart;
int gNextGopEnd;

int gDumpDep;

int gRoiSh, gRoiSw, gRoiEh, gRoiEw;

int *gZoomLevelToVideoIndex;

void get_video_info(int p_debug);
void allocate_selected_decoding_fields(int p_videoFileIndex, int _mbHeight, int _mbWidth);
void free_selected_decoding_fields(int p_videoFileIndex, int _mbHeight);
void dump_frame_to_file(int _frameNum);
//int decode_a_video_packet(int pMode, int p_videoFileIndex, int _roiStH, int _roiStW, int _roiEdH, int _roiEdW, int _displayStH, int _displayStW, int _displayEdH, int _displayEdW);
int decode_a_video_packet(int pMode, int p_videoFileIndex, int _roiStH, int _roiStW, int _roiEdH, int _roiEdW, 
		int _cropStH, int _cropStW, int _cropEdH, int _cropEdW,
		int _displayStH, int _displayStW, int _displayEdH, int _displayEdW);
int dep_decode_a_video_packet(int p_videoFileIndex);
int load_gop_info(FILE* p_gopRecFile, int *p_startF, int *p_endF);
int get_gop_info_given_gop_num(int p_videoFileIndex, int pGopNum, int *pStartF, int *pEndF);
int if_dependency_complete(int p_videoFileIndex, int p_gopNum);
int prepare_decode_of_gop(int p_videoFileIndex, int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw);
#ifdef PRE_LOAD_DEP
int preload_pre_computation_result(int pVideoFileIndex, int pGopNum);
#endif
int load_frame_mb_stindex(int p_videoFileIndex, int pGopNum, int ifPreload);
int load_frame_mb_edindex(int p_videoFileIndex, int pGopNum, int ifPreload);
int load_frame_mb_len(int p_videoFileIndex, int pGopNum, int ifPreload);
void unload_frame_mb_stindex(void);
void unload_frame_mb_edindex(void);
void unload_frame_mb_len(int pVideoFileIndex);
void unload_frame_dc_pred_direction(void);
void unload_inter_frame_mb_dependency(void);
void unload_intra_frame_mb_dependency(void);
void unload_mv(int pVideoFileIndex);

#endif
