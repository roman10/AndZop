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

/*for logs*/
//#define LOG_ANDROID
#define LOG_LEVEL 10
#ifdef LOG_ANDROID
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

#define SELECTIVE_DECODING

#define MAX_NUM_OF_GOP 50
#define MAX_FRAME_NUM_IN_GOP 50
#define MAX_MB_H 50
#define MAX_MB_W 50
#define MAX_DEP_MB 4

#define DUMP_PACKET
#define DUMP_DEP
#define DUMP_BUF_POS
#ifdef DUMP_BUF_POS
    FILE* bufposF;
#endif
#define DUMP_PACKET_TYPE
#ifdef DUMP_PACKET_TYPE
    FILE *packetTypeFile;
#endif

/*structure for decoded video frame*/
typedef struct VideoPicture {
    double pts;
    double delay;
    int width, height;    //the requested size/the bitmap resolution
    AVPicture data;
} VideoPicture;

AVCodecContext *gVideoCodecCtx;

VideoPicture gVideoPicture;

struct SwsContext *gImgConvertCtx;   //[TODO]: check out why declear as global, probably for caching reason
AVFormatContext *gFormatCtx;
char *gFileName;	  //the file name of the video
int gVideoStreamIndex;    //video stream index
int gStFrame;
int gVideoPacketNum;

AVPacket gVideoPacket;    //the original video packet
AVPacket gVideoPacket2;   //the composed video packet

int gRoiSh, gRoiSw, gRoiEh, gRoiEw;

/*the file names for the dependency relationship*/
FILE *g_mbPosF;
FILE *g_intraDepF;
FILE *g_interDepF;
FILE *g_dcPredF;

int gGopStart[MAX_NUM_OF_GOP];
int gGopEnd[MAX_NUM_OF_GOP];
int gNumOfGop;

void get_video_info(char *prFilename);
void allocate_selected_decoding_fields(int _mbHeight, int _mbWidth);
void free_selected_decoding_fields(int _mbHeight);
void dump_frame_to_file(int _frameNum);
void decode_a_video_packet(int _roiStH, int _roiStW, int _roiEdH, int _roiEdW);
void load_gop_info(void);
void prepare_decode_of_gop(int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw);


