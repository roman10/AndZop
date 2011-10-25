#include <stdlib.h>

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
#define ANDROID_BUILD
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

//#define NORM_DECODE_DEBUG			//uncommented: dump dependency for normal decoding mode; should be commented at 						//selective decoding mode
//#define DUMP_SELECTED_MB_MASK			//enabled: dump the mask for the mb needed;
//#define DUMP_VIDEO_FRAME_BYTES			//enabled: dump the bytes to a binary file
//#define DUMP_SELECTIVE_DEP			//enabled: dump the relationship in memory to files

#ifdef ANDROID_BUILD
	#define MAX_FRAME_NUM_IN_GOP 50
	#define MAX_MB_H 100
	#define MAX_MB_W 100
	#define MAX_DEP_MB 4
#else
	#define MAX_FRAME_NUM_IN_GOP 50
	#define MAX_MB_H 100
	#define MAX_MB_W 100
	#define MAX_DEP_MB 4
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
void decode_a_video_packet(int p_videoFileIndex, int _roiStH, int _roiStW, int _roiEdH, int _roiEdW);
void dep_decode_a_video_packet(int p_videoFileIndex);
int load_gop_info(FILE* p_gopRecFile, int *p_startF, int *p_endF);
int if_dependency_complete(int p_videoFileIndex, int p_gopNum);
void prepare_decode_of_gop(int p_videoFileIndex, int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw);


