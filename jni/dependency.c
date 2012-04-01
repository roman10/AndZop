#include "dependency.h"
#ifdef ANDROID_BUILD
	#include "yuv2rgb.h"
	#include "scale.h"
	#include "yuv2rgb.neon.h"
	#include "andutils.h"
#endif

static int videoIndexCmp(const void *pa, const void *pb) {
    int *a = (int *)pa, *b = (int *)pb;
    LOGI(10, "videoIndexCmp: %d, %d, %d, %d", *a, *b, gVideoCodecCtxList[*a]->width * gVideoCodecCtxList[*a]->height, gVideoCodecCtxList[*b]->width * gVideoCodecCtxList[*b]->height);
    if ((gVideoCodecCtxList[*a]->width * gVideoCodecCtxList[*a]->height) > (gVideoCodecCtxList[*b]->width * gVideoCodecCtxList[*b]->height)) {
        return 1;
    } else if ((gVideoCodecCtxList[*a]->width * gVideoCodecCtxList[*a]->height) < (gVideoCodecCtxList[*b]->width * gVideoCodecCtxList[*b]->height)) {
        return -1;
    } else {
        return 0;
    }
}
/*parsing the video file, done by parse thread*/
void get_video_info(int p_debug) {
    AVCodec *lVideoCodec;
    int lError;
    int l_dumpDep;
    int l_i;
    /*some global variables initialization*/
    extern AVCodec ff_h263_decoder;
    extern AVCodec ff_h264_decoder;
    extern AVCodec ff_mpeg4_decoder;
    extern AVCodec ff_mjpeg_decoder;
    extern AVInputFormat ff_mov_demuxer;
    extern URLProtocol ff_file_protocol;
    LOGI(10, "get video info starts!");
    /*register the codec*/
    avcodec_register(&ff_h263_decoder);
    avcodec_register(&ff_h264_decoder);
    avcodec_register(&ff_mpeg4_decoder);
    avcodec_register(&ff_mjpeg_decoder);
    /*register parsers*/
    //extern AVCodecParser ff_h264_parser;
    //av_register_codec_parser(&ff_h264_parser);
    //extern AVCodecParser ff_mpeg4video_parser;
    //av_register_codec_parser(&ff_mpeg4video_parser);
    /*register demux*/
    av_register_input_format(&ff_mov_demuxer);
    //extern AVInputFormat ff_h264_demuxer;
    //av_register_input_format(&ff_h264_demuxer);
    /*register the protocol*/
    av_register_protocol2(&ff_file_protocol, sizeof(ff_file_protocol));
    /*initialize the lists*/
    gFormatCtxList = (AVFormatContext**)malloc(gNumOfVideoFiles*sizeof(AVFormatContext));
    gVideoStreamIndexList = (int*)malloc(gNumOfVideoFiles*sizeof(int));
    gVideoCodecCtxList = (AVCodecContext**)malloc(gNumOfVideoFiles*sizeof(AVCodecContext));
    gVideoPacketQueueList = (PacketQueue*)malloc(gNumOfVideoFiles*sizeof(PacketQueue));
    gFormatCtxDepList = (AVFormatContext**)malloc(gNumOfVideoFiles*sizeof(AVFormatContext));
    gVideoCodecCtxDepList = (AVCodecContext**)malloc(gNumOfVideoFiles*sizeof(AVCodecContext));
    gVideoPacketDepList = (AVPacket*)malloc(gNumOfVideoFiles*sizeof(AVPacket));
	for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
		packet_queue_init(&gVideoPacketQueueList[l_i]);		//initialize the packet queue
		gVideoPacketQueueList[l_i].dep_gop_num = 1;
		g_decode_gop_num = 1;
		gVideoPacketQueueList[l_i].nb_packets = 0;
	}
    for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
#if (defined SELECTIVE_DECODING) || (defined NORM_DECODE_DEBUG)
		/***
			The following section are initialization for dumping dependencies
		**/
	    /*open the video file*/
		if ((lError = av_open_input_file(&gFormatCtxDepList[l_i], gVideoFileNameList[l_i], NULL, 0, NULL)) !=0 ) {
			LOGE(1, "Error open video file: %d", lError);
			return;	//open file failed
		}
		/*retrieve stream information*/
		if ((lError = av_find_stream_info(gFormatCtxDepList[l_i])) < 0) {
			LOGE(1, "Error find stream information: %d", lError);
			return;
		} 
		/*find the video stream and its decoder*/
		gVideoStreamIndexList[l_i] = av_find_best_stream(gFormatCtxDepList[l_i], AVMEDIA_TYPE_VIDEO, -1, -1, &lVideoCodec, 0);
		if (gVideoStreamIndexList[l_i] == AVERROR_STREAM_NOT_FOUND) {
			LOGE(1, "Error: cannot find a video stream");
			return;
		} else {
			LOGI(10, "video codec: %s; stream index: %d", lVideoCodec->name, gVideoStreamIndexList[l_i]);
		}
		if (gVideoStreamIndexList[l_i] == AVERROR_DECODER_NOT_FOUND) {
			LOGE(1, "Error: video stream found, but no decoder is found!");
			return;
		}   
		/*open the codec*/
		gVideoCodecCtxDepList[l_i] = gFormatCtxDepList[l_i]->streams[gVideoStreamIndexList[l_i]]->codec;
		LOGI(10, "open codec for dumping dep: (%d, %d)", gVideoCodecCtxDepList[l_i]->height, gVideoCodecCtxDepList[l_i]->width);
		if (avcodec_open(gVideoCodecCtxDepList[l_i], lVideoCodec) < 0) {
			LOGE(1, "Error: cannot open the video codec!");
			return;
		}
#endif
		/***
			The following section are initialization for decoding
		**/
	    if ((lError = av_open_input_file(&gFormatCtxList[l_i], gVideoFileNameList[l_i], NULL, 0, NULL)) !=0 ) {
			LOGE(1, "Error open video file: %d", lError);
			return;	//open file failed
	    }
	    /*retrieve stream information*/
	    LOGI(1, "try find stream info");
	    if ((lError = av_find_stream_info(gFormatCtxList[l_i])) < 0) {
			LOGE(1, "Error find stream information: %d", lError);
			return;
	    } 
	    LOGI(1, "stream info retrieved");
	    /*find the video stream and its decoder*/
	    gVideoStreamIndexList[l_i] = av_find_best_stream(gFormatCtxList[l_i], AVMEDIA_TYPE_VIDEO, -1, -1, &lVideoCodec, 0);
	    if (gVideoStreamIndexList[l_i] == AVERROR_STREAM_NOT_FOUND) {
			LOGE(1, "Error: cannot find a video stream");
			return;
	    } else {
			LOGI(10, "video codec: %s; stream index: %d", lVideoCodec->name, gVideoStreamIndexList[l_i]);
	    }
	    if (gVideoStreamIndexList[l_i] == AVERROR_DECODER_NOT_FOUND) {
			LOGE(1, "Error: video stream found, but no decoder is found!");
			return;
	    }   
	    /*open the codec*/
	    gVideoCodecCtxList[l_i] = gFormatCtxList[l_i]->streams[gVideoStreamIndexList[l_i]]->codec;
	    LOGI(10, "open codec: (%d, %d)", gVideoCodecCtxList[l_i]->height, gVideoCodecCtxList[l_i]->width);
	    if (avcodec_open(gVideoCodecCtxList[l_i], lVideoCodec) < 0) {
			LOGE(1, "Error: cannot open the video codec!");
			return;
	    }
        //gVideoCodecCtxList[l_i]->error_concealment = 0;
#ifdef SELECTIVE_DECODING
	    LOGI(10, "SELECTIVE_DECODING is enabled");
	    gVideoCodecCtxList[l_i]->allow_selective_decoding = 1;
#else
	    LOGI(10, "SELECTIVE_DECODING is disabled");
	    gVideoCodecCtxList[l_i]->allow_selective_decoding = 0;
#endif
	    /*set the debug option*/
	    gVideoCodecCtxList[l_i]->debug_selective = p_debug;
	    if (gVideoCodecCtxList[l_i]->debug_selective == 1) {
			//clear the old dump file
#ifdef ANDROID_BUILD
			FILE *dctF = fopen("/sdcard/r10videocam/debug_dct.txt", "w");
#else
			FILE *dctF = fopen("./debug_dct.txt", "w");
#endif		
			fclose(dctF);
	    }
    }
	//if there's multiple inputs, we'll need to come up with a zoom level index to video file index map
	gZoomLevelToVideoIndex = (int*) malloc(gNumOfVideoFiles*sizeof(int));
	for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
		gZoomLevelToVideoIndex[l_i] = l_i;
	}
	qsort(gZoomLevelToVideoIndex, gNumOfVideoFiles, sizeof(int), videoIndexCmp);
    gCurrentDecodingVideoFileIndex = gZoomLevelToVideoIndex[0];
	for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
		LOGI(10, "zoom level to video index map %d: %d", l_i, gZoomLevelToVideoIndex[l_i]);
	}
    LOGI(10, "get video info ends");
}

//TODO: more fields to clear
#ifdef SELECTIVE_DECODING
void allocate_selected_decoding_fields(int p_videoFileIndex, int _mbHeight, int _mbWidth) {
    int l_i;
	LOGI(10, "allocate %d video selected decoding fields: %d, %d", p_videoFileIndex, _mbHeight, _mbWidth);
    gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask = (unsigned char **) malloc(_mbHeight * sizeof(unsigned char *));
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i] = (unsigned char *) malloc(_mbWidth * sizeof(unsigned char));
    }
    /*gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir = (unsigned char **) malloc(_mbHeight * sizeof(unsigned char *));
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_i] = (unsigned char *) malloc(_mbWidth * sizeof(unsigned char));
    }*/
	LOGI(10, "allocate %d video selected decoding fields: %d, %d is done", p_videoFileIndex, _mbHeight, _mbWidth);
}

//TODO: more fields to clear
void free_selected_decoding_fields(int p_videoFileIndex, int _mbHeight) {
    int l_i;
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        free(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i]);
    }
    free(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask);
    /*for (l_i = 0; l_i < _mbHeight; ++l_i) {
        free(gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_i]);
    }
    free(gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir);*/
}
#endif

int *mbStartPos;
int mapStLen;
int mbStartFd;

//if next video file index is the same as current video file index, we simply assign the next info to current one
int nextVideoFileIndex;	
int ifNextMbStartLoaded;	
int *nextMbStartPos;
int nextMapStLen;
int nextMbStartFd;

int ifNextMbEndLoaded;
int *nextMbEndPos;
int nextMapEdLen;
int nextMbEndFd;

int *mbEndPos;
int mapEdLen;
int mbEndFd;

int ifNextMbLenLoaded;
unsigned short *nextMbLen;
int nextMapLenLen;
int nextMbLenFd;

//unsigned short *mbLen;
int mapLenLen;
int mbLenFd;

//instead of using memcpy
unsigned char (*pInterDepMask)[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
unsigned char nextInterDepMaskBuf;
unsigned char interDepMaskBuf1[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
unsigned char interDepMaskBuf2[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
#ifdef MV_BASED_OPTIMIZATION
typedef struct MBR{      //define a mb region
    short tlh, tlw;      //top left height, top left width
    unsigned char ph, pw;      //pixel height, pixel width
}MBR;
MBR **mvDep;
int *mbCounts;
#endif
int nextInterDepMaskVideoIndex;
int nextInterDepMaskStH;
int nextInterDepMaskStW;
int nextInterDepMaskEdH;
int nextInterDepMaskEdW;

static void compute_mb_mask_from_inter_frame_dependency(int p_videoFileIndex, int pGopNum, int _stFrame, int _edFrame, int _stH, int _stW, int _edH, int _edW, int ifPreload);

void unload_frame_mb_stindex(void) {
	close(mbStartFd);
	munmap(mbStartPos, mapStLen);
}

void unload_frame_mb_edindex(void) {
	close(mbEndFd);
	munmap(mbEndPos, mapEdLen);
}

void unload_frame_mb_len(int pVideoFileIndex) {
	close(mbLenFd);
	//munmap(mbLen, mapLenLen);
    munmap(gVideoCodecCtxList[pVideoFileIndex]->g_mbLen, mapLenLen);
}

void load_frame_mb_len(int p_videoFileIndex, int pGopNum, int ifPreload) {
    if ((!ifPreload) && ifNextMbLenLoaded) {
        ifNextMbLenLoaded = 0;
        //mbLen = nextMbLen;
        gVideoCodecCtxList[p_videoFileIndex]->g_mbLen = nextMbLen;
	    mapLenLen = nextMapLenLen;
	    mbLenFd = nextMbLenFd;
    } else {
		char l_mbLenFileName[200];
		int *l_mbLenFd;
		unsigned short **l_mbLen;
		int *l_mapLenLen;
		struct stat sbuf;
		if (ifPreload) {
			l_mbLenFd = &nextMbLenFd;
			l_mbLen = &nextMbLen;
			l_mapLenLen = &nextMapLenLen;
		} else {
			l_mbLenFd = &mbLenFd;
			//l_mbLen = &mbLen;
            l_mbLen = &(gVideoCodecCtxList[p_videoFileIndex]->g_mbLen);
			l_mapLenLen = &mapLenLen;
		}
#ifdef ANDROID_BUILD
		sprintf(l_mbLenFileName, "%s_mblen_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#else
		sprintf(l_mbLenFileName, "./%s_mblen_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#endif
			LOGI(10, "+++++load_frame_mb_len: %s", l_mbLenFileName);
		if ((*l_mbLenFd = open(l_mbLenFileName, O_RDONLY)) == -1) {
			LOGE(1, "file open error %d: %s", errno, gVideoCodecCtxList[p_videoFileIndex]->g_mbLenFileName);
			perror("file open error: ");
			exit(1);
		}
		if (stat(l_mbLenFileName, &sbuf) == -1) {
			LOGE(1, "stat error");
			exit(1);
		}
		*l_mapLenLen = sbuf.st_size;
		LOGI(10, "file size: %u", *l_mapLenLen);
		*l_mbLen = mmap(0, *l_mapLenLen, PROT_READ, MAP_PRIVATE, *l_mbLenFd, 0)	;
		if (*l_mbLen == MAP_FAILED) {
			LOGE(1, "mmap error");
			perror("mmap error: ");
			exit(1);
		}
		if (ifPreload) {
		    nextVideoFileIndex = p_videoFileIndex;
			ifNextMbLenLoaded = 1;
		}
    }
    LOGI(10, "+++++load_frame_mb_len finished, exit the function");
}

void load_frame_mb_stindex(int p_videoFileIndex, int pGopNum, int ifPreload) {
    if ((!ifPreload) && ifNextMbStartLoaded) {
        ifNextMbStartLoaded = 0;
        mbStartPos = nextMbStartPos;
	mapStLen = nextMapStLen;
	mbStartFd = nextMbStartFd;
    } else {
	char l_mbStPosFileName[200];
	int *l_mbStartFd;
	int **l_mbStartPos;
	int *l_mapStLen;
//#ifdef ANDROID_BUILD
//        FILE *lf;  
//#else
	struct stat sbuf;
//#endif
	if (ifPreload) {
	    l_mbStartFd = &nextMbStartFd;
	    l_mbStartPos = &nextMbStartPos;
	    l_mapStLen = &nextMapStLen;
	} else {
	    l_mbStartFd = &mbStartFd;
	    l_mbStartPos = &mbStartPos;
	    l_mapStLen = &mapStLen;
	}
#ifdef ANDROID_BUILD
	sprintf(l_mbStPosFileName, "%s_mbstpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#else
	sprintf(l_mbStPosFileName, "./%s_mbstpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#endif
    	LOGI(10, "+++++load_frame_mb_stindex: %s", l_mbStPosFileName);
	if ((*l_mbStartFd = open(l_mbStPosFileName, O_RDONLY)) == -1) {
		LOGE(1, "file open error %d: %s", errno, gVideoCodecCtxList[p_videoFileIndex]->g_mbStPosFileName);
		perror("file open error: ");
		exit(1);
	}
/*#ifdef ANDROID_BUILD
        lf = fopen(l_mbStPosFileName, "r");
        fseek(lf, 0, SEEK_END);
        *l_mapStLen = ftell(lf);
        fclose(lf);
#else*/
	if (stat(l_mbStPosFileName, &sbuf) == -1) {
		LOGE(1, "stat error");
		exit(1);
	}
        *l_mapStLen = sbuf.st_size;
//#endif
	LOGI(10, "file size: %u", *l_mapStLen);
	*l_mbStartPos = mmap(0, *l_mapStLen, PROT_READ, MAP_PRIVATE, *l_mbStartFd, 0)	;
	if (*l_mbStartPos == MAP_FAILED) {
		LOGE(1, "mmap error");
		perror("mmap error: ");
		exit(1);
	}
	if (ifPreload) {
            nextVideoFileIndex = p_videoFileIndex;
	    ifNextMbStartLoaded = 1;
	}
	//TODO: preload the data
    }
    LOGI(10, "+++++load_frame_mb_stindex finished, exit the function");
}

void load_frame_mb_edindex(int p_videoFileIndex, int pGopNum, int ifPreload) {
    if ((!ifPreload) && ifNextMbEndLoaded) {
        ifNextMbEndLoaded = 0;
        mbEndPos = nextMbEndPos;
        mapEdLen = nextMapEdLen;
        mbEndFd = nextMbEndFd;
    } else {
        char l_mbEdPosFileName[200];
        int *l_mbEndFd;
        int **l_mbEndPos;
        int *l_mapEdLen;
//#ifdef ANDROID_BUILD
//        FILE *lf;  
//#else
        struct stat sbuf;
//#endif
        if (ifPreload) {
            l_mbEndFd = &nextMbEndFd;
            l_mbEndPos = &nextMbEndPos;
            l_mapEdLen = &nextMapEdLen;
        } else {
            l_mbEndFd = &mbEndFd;
            l_mbEndPos = &mbEndPos;
            l_mapEdLen = &mapEdLen;
        }
#ifdef ANDROID_BUILD
        sprintf(l_mbEdPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#else
        sprintf(l_mbEdPosFileName, "./%s_mbedpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#endif
        LOGI(10, "+++++load_frame_mb_edindex, file: %s", l_mbEdPosFileName);
	if ((*l_mbEndFd = open(l_mbEdPosFileName, O_RDONLY)) == -1) {
            LOGE(1, "file open error %d: %s", errno, l_mbEdPosFileName);
            exit(1);
	}
/*#ifdef ANDROID_BUILD
        lf = fopen(l_mbEdPosFileName, "r");
        fseek(lf, 0, SEEK_END);
        *l_mapEdLen = ftell(lf);
        fclose(lf);
#else*/
        if (stat(l_mbEdPosFileName, &sbuf) == -1) {
	    LOGE(1, "stat error");
	    exit(1);
        }
        *l_mapEdLen = sbuf.st_size;
//#endif
        LOGI(10, "file size: %ld", *l_mapEdLen);
        *l_mbEndPos = mmap(0, *l_mapEdLen, PROT_READ, MAP_PRIVATE, *l_mbEndFd, 0);
        if (*l_mbEndPos == MAP_FAILED) {
            LOGE(1, "mmap error");
            perror("mmap error: ");
            exit(1);
        }
        if (ifPreload) {
            ifNextMbEndLoaded = 1;
        }
    //TODO: preload the data
    }
    LOGI(10, "+++++load_frame_mb_edindex finished, exit the function");
}

unsigned char *intraDepMap;
unsigned int intraDepMapLen;
int intraDepFd;

int ifNextIntraDepLoaded;
unsigned char *nextIntraDepMap;
unsigned int nextIntraDepMapLen;
int nextIntraDepFd;

void unload_intra_frame_mb_dependency(void) {
    LOGI(10, "unload_intra_frame_mb_dependency: %d", intraDepMapLen);
	close(intraDepFd);
	if (munmap(intraDepMap, intraDepMapLen)!=0) {
        LOGE(1, "munmap error!");
        exit(0);
    }
}
static void load_intra_frame_mb_dependency(int p_videoFileIndex, int pGopNumber, int ifPreload) {
    if ((!ifPreload) && ifNextIntraDepLoaded) {
        ifNextIntraDepLoaded = 0;
        intraDepMap = nextIntraDepMap;
        intraDepMapLen = nextIntraDepMapLen;
        intraDepFd = nextIntraDepFd;
    } else {
	char l_depIntraFileName[100];
        unsigned char **l_intraDepMap;
        int *l_intraDepFd;
        unsigned int *l_intraDepMapLen;
	struct stat sbuf;
        if (ifPreload) {
            l_intraDepFd = &nextIntraDepFd;
            l_intraDepMap = &nextIntraDepMap;
            l_intraDepMapLen = &nextIntraDepMapLen;
        } else {
            l_intraDepFd = &intraDepFd;
            l_intraDepMap = &intraDepMap;
            l_intraDepMapLen = &intraDepMapLen;
        }
#ifdef ANDROID_BUILD
    sprintf(l_depIntraFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNumber);
#else
    sprintf(l_depIntraFileName, "./%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNumber);
#endif
    LOGI(10, "+++++load_intra_frame_mb_dependency, file: %s", l_depIntraFileName);
	if ((*l_intraDepFd = open(l_depIntraFileName, O_RDONLY)) == -1) {
		LOGE(1, "file open error %d: %s", errno, l_depIntraFileName);
		exit(1);
	}
	if (stat(l_depIntraFileName, &sbuf) == -1) {
		LOGE(1, "stat error");
		exit(1);
	}
	*l_intraDepMapLen = sbuf.st_size;
	LOGI(10, "file size: %ld", *l_intraDepMapLen);
	*l_intraDepMap = mmap(0, *l_intraDepMapLen, PROT_READ, MAP_PRIVATE, *l_intraDepFd, 0);
	if (*l_intraDepMap == MAP_FAILED) {
		LOGE(1, "mmap error");
		perror("mmap error: ");
		exit(1);
	}
        if (ifPreload) {
            ifNextIntraDepLoaded = 1;
        }
    }
    LOGI(10, "+++++load_intra_frame_mb_dependency finished, exit the function");
}

#ifdef MV_BASED_DEPENDENCY
short *nextMvMap;
unsigned int mvMapLen;
int mvFd;

int ifNextMvLoaded;
unsigned int nextMvMapLen;
int nextMvFd;

void unload_mv(int pVideoFileIndex) {
    close(mvFd);
    munmap(gVideoCodecCtxList[pVideoFileIndex]->g_mv, mvMapLen);
}

static void load_mv(int pVideoFileIndex, int pGopNumber, int ifPreload) {
    if ((!ifPreload) && ifNextMvLoaded) {
        ifNextMvLoaded = 0;
        gVideoCodecCtxList[pVideoFileIndex]->g_mv = nextMvMap;
        mvMapLen = nextMvMapLen;
        mvFd = nextMvFd;
    } else {
        char lMvFileName[100];
        short **lMvMap;
        int *lMvFd;
        unsigned int *lMvMapLen;
        struct stat sbuf;
        if (ifPreload) {
            lMvFd = &nextMvFd;
            lMvMap = &nextMvMap;
            lMvMapLen = &nextMvMapLen;
        } else {
            lMvFd = &mvFd;
            lMvMap = &gVideoCodecCtxList[pVideoFileIndex]->g_mv;
            lMvMapLen = &mvMapLen;
        }
#ifdef ANDROID_BUILD
        sprintf(lMvFileName, "%s_mv_gop%d.txt", gVideoFileNameList[pVideoFileIndex], pGopNumber);
#else
        sprintf(lMvFileName, "./%s_mv_gop%d.txt", gVideoFileNameList[pVideoFileIndex], pGopNumber);
#endif
        LOGI(10, "+++++load_mv, file: %s", lMvFileName);
        if ((*lMvFd = open(lMvFileName, O_RDONLY)) == -1) {
            LOGE(1, "file open error %d: %s", errno, lMvFileName);
            exit(1);
        }
        if (stat(lMvFileName, &sbuf) == -1) {
            LOGE(1, "stat error");
            exit(1);
        }
        *lMvMapLen = sbuf.st_size;
        LOGI(10, "file size: %ld", *lMvMapLen);
        *lMvMap = mmap(0, *lMvMapLen, PROT_READ, MAP_PRIVATE, *lMvFd, 0);
        if (*lMvMap == MAP_FAILED) {
            LOGE(1, "mmap error");
            perror("mmap error: ");
            exit(1);
        }
        if (ifPreload) {
            ifNextMvLoaded = 1;
        }
    }
    LOGI(10, "+++++load_mv finished, exit the function");
}
#endif

unsigned char *interDepMap, *interDepMapMove;
unsigned char *nextInterDepMap;
unsigned int interDepMapLen;
int interDepFd;

int ifNextInterDepLoaded;
unsigned int nextInterDepMapLen;
int nextInterDepFd;

void unload_inter_frame_mb_dependency(void) {
    close(interDepFd);
    munmap(interDepMap, interDepMapLen);
}

static void load_inter_frame_mb_dependency(int p_videoFileIndex, int pGopNumber, int ifPreload) {
    if ((!ifPreload) && ifNextInterDepLoaded) {
        ifNextInterDepLoaded = 0;
        interDepMap = nextInterDepMap;
        interDepMapLen = nextInterDepMapLen;
        interDepFd = nextInterDepFd;
    } else {
        char l_depInterFileName[100];
        unsigned char **l_interDepMap;
        int *l_interDepFd;
        unsigned int *l_interDepMapLen;
        struct stat sbuf;
        if (ifPreload) {
            l_interDepFd = &nextInterDepFd;
            l_interDepMap = &nextInterDepMap;
            l_interDepMapLen = &nextInterDepMapLen;
        } else {
            l_interDepFd = &interDepFd;
            l_interDepMap = &interDepMap;
            l_interDepMapLen = &interDepMapLen;
        }
#ifdef ANDROID_BUILD
        sprintf(l_depInterFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNumber);
#else
        sprintf(l_depInterFileName, "./%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNumber);
#endif
        LOGI(10, "+++++load_inter_frame_mb_dependency, file: %s", l_depInterFileName);
        if ((*l_interDepFd = open(l_depInterFileName, O_RDONLY)) == -1) {
            LOGE(1, "file open error %d: %s", errno, l_depInterFileName);
            exit(1);
        }
        if (stat(l_depInterFileName, &sbuf) == -1) {
            LOGE(1, "stat error");
            exit(1);
        }
        *l_interDepMapLen = sbuf.st_size;
        LOGI(10, "file size: %ld", *l_interDepMapLen);
        *l_interDepMap = mmap(0, *l_interDepMapLen, PROT_READ, MAP_PRIVATE, *l_interDepFd, 0);
        if (*l_interDepMap == MAP_FAILED) {
            LOGE(1, "mmap error");
            perror("mmap error: ");
            exit(1);
        }
        if (ifPreload) {
            ifNextInterDepLoaded = 1;
        }
    }
    LOGI(10, "+++++load_inter_frame_mb_dependency finished, exit the function");
}

unsigned int dcpMapLen;
unsigned char *dcpPos, *dcpPosMove;
int dcpFd;

unsigned int ifNextDcpLoaded;
unsigned int nextDcpMapLen;
unsigned char *nextDcpPos, *nextDcpPosMove;
int nextDcpFd;

void unload_frame_dc_pred_direction(void) {
	close(dcpFd);
	munmap(dcpPos, dcpMapLen);
}

static void load_gop_dc_pred_direction(int p_videoFileIndex, int pGopNumber, int ifPreload) {
    if ((!ifPreload) && ifNextDcpLoaded) {
        ifNextDcpLoaded = 0;
        dcpPos = nextDcpPos;
        dcpPosMove = nextDcpPosMove;
        dcpFd = nextDcpFd;
    } else {
	char l_dcPredFileName[100];
        unsigned int *l_dcpMapLen;
        unsigned char **l_dcpPos, **l_dcpPosMove;
        int *l_dcpFd;
	struct stat sbuf;
        if (ifPreload) {
            l_dcpMapLen = &nextDcpMapLen;
            l_dcpPos = &nextDcpPos;
            l_dcpPosMove = &nextDcpPosMove;
            l_dcpFd = &nextDcpFd;
        } else {
            l_dcpMapLen = &dcpMapLen;
            l_dcpPos = &dcpPos;
            l_dcpPosMove = &dcpPosMove;
            l_dcpFd = &dcpFd;
        }
#ifdef ANDROID_BUILD
        sprintf(l_dcPredFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNumber);
#else
        sprintf(l_dcPredFileName, "./%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNumber);
#endif
        LOGI(10, "load_gop_dc_pred_direction: %s\n", l_dcPredFileName);
        if ((*l_dcpFd = open(l_dcPredFileName, O_RDONLY)) == -1) {
		LOGE(1, "file open error %d: %s", errno, l_dcPredFileName);
		perror("file open error: ");
		exit(1);
	}
	if (stat(l_dcPredFileName, &sbuf) == -1) {
		LOGE(1, "stat error");
		exit(1);
	}
	*l_dcpMapLen = sbuf.st_size;
	LOGI(9, "file size: %ld", *l_dcpMapLen);
	*l_dcpPos = mmap(0, *l_dcpMapLen, PROT_READ, MAP_PRIVATE, *l_dcpFd, 0);
	*l_dcpPosMove = *l_dcpPos;
	if (*l_dcpPos == MAP_FAILED) {
	    LOGE(1, "map error");
	    perror("mmap error:");
	    exit(1);
	}
        if (ifPreload) {
            ifNextDcpLoaded = 1;
        }
    }
    LOGI(10, "load_gop_dc_pred_direction done\n");
}

static void load_frame_dc_pred_direction(int p_videoFileIndex, int _height, int _width) {
    int i, j;
    LOGI(10, "load_frame_dc_pred_direction\n");
    gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir = dcpPosMove;
    dcpPosMove += _height*_width;
    LOGI(10, "load_frame_dc_pred_direction done\n");
}

#ifdef PRE_LOAD_DEP
//done on GOP basis
void preload_pre_computation_result(int pVideoFileIndex, int pGopNum) {
    LOGI(10, "preload_pre_computation_result");
#ifdef COMPOSE_PACKET_OR_SKIP
    load_frame_mb_stindex(pVideoFileIndex, pGopNum, 1);              //the mb index position
    load_frame_mb_edindex(pVideoFileIndex, pGopNum, 1);              //the mb index position
#else
    load_frame_mb_edindex(pVideoFileIndex, pGopNum, 1);              //the mb index position
    load_frame_mb_len(pVideoFileIndex, pGopNum, 1);                  //the mb length
#endif
    load_intra_frame_mb_dependency(pVideoFileIndex, pGopNum, 1);
#ifdef MV_BASED_DEPENDENCY
    load_mv(pVideoFileIndex, pGopNum, 1); 
#endif
    compute_mb_mask_from_inter_frame_dependency(pVideoFileIndex, pGopNum, gNextGopStart, gNextGopEnd, gRoiSh, gRoiSw, gRoiEh, gRoiEw, 1);
    //load_inter_frame_mb_dependency(pVideoFileIndex, pGopNum, 1);   //we're going to use inter frame dependency file to compute mask, no need to preload
    load_gop_dc_pred_direction(pVideoFileIndex, pGopNum, 1);
}
#endif

/*done on a GOP basis*/
static void load_pre_computation_result(int pVideoFileIndex) {
#ifdef COMPOSE_PACKET_OR_SKIP
    load_frame_mb_stindex(pVideoFileIndex, g_decode_gop_num, 0);              //the mb index position
    load_frame_mb_edindex(pVideoFileIndex, g_decode_gop_num, 0);              //the mb index position
#else
    load_frame_mb_edindex(pVideoFileIndex, g_decode_gop_num, 0);              //the mb index position
    load_frame_mb_len(pVideoFileIndex, g_decode_gop_num, 0);                  //the mb length
#endif
    load_intra_frame_mb_dependency(pVideoFileIndex, g_decode_gop_num, 0);   //the intra-frame dependency
#ifdef MV_BASED_DEPENDENCY
    load_mv(pVideoFileIndex, g_decode_gop_num, 0); 
#endif
#ifndef PRE_LOAD_DEP								     //if we don't preload (pre-compute, then we'll need to load inter frame files
    load_inter_frame_mb_dependency(pVideoFileIndex, g_decode_gop_num, 0);   	//the inter-frame dependency
#endif
    load_gop_dc_pred_direction(pVideoFileIndex, g_decode_gop_num, 0);		    //the dc prediction direction 
}

void dump_frame_to_file(int _frameNum) {
    FILE *l_pFile;
    char l_filename[32];
    int y, k;
    LOGI(10, "dump frame to file");
    //open file
#ifdef ANDROID_BUILD
    sprintf(l_filename, "/sdcard/r10videocam/frame_%d.ppm", _frameNum);
#else
    sprintf(l_filename, "frame_%d.ppm", _frameNum);
#endif
    l_pFile = fopen(l_filename, "wb");
    if (l_pFile == NULL) 
        return;
    //write header
    fprintf(l_pFile, "P6\n%d %d\n255\n", gVideoPicture.width, gVideoPicture.height);
    //write pixel data
#ifndef ANDROID_BUILD
    for (y = 0; y < gVideoPicture.height; ++y) {
        for (k = 0; k < gVideoPicture.width; ++k) {
            fwrite(gVideoPicture.data.data[0] + y * gVideoPicture.data.linesize[0] + k*4, 1, 3, l_pFile);
        }
    }
#endif
    fclose(l_pFile);
    LOGI(10, "frame dumped to file");
}

//copy count bits starting from startPos from data to buf starting at bufPos
static int copy_bits(unsigned char *data, unsigned char *buf, int startPos, int count, int bufPos) {
    unsigned char value;
    int length;
    int bitsCopied;
    int i;
    int numOfAlignedBytes;
    bitsCopied = 0;
    //1. get the starting bits that are not align with byte boundary
    //[TODO] this part is a bit tricky to consider all situations, better to build a unit test for it
    if (startPos % 8 != 0) {
        length = (8 - startPos % 8) < count ? (8 - startPos % 8):count; //use count if count cannot fill to the byte boundary
        value = (*(data + startPos / 8)) & (0xFF >> (startPos % 8)) & (0xFF << (8 - startPos % 8 - length));
	//LOGI("**** value: %x, %d\n", value, length);
	if (8 - startPos % 8 <= 8 - bufPos % 8) {
	    //LOGI("0!!!!%d, %d", bufPos / 8, startPos % 8 - bufPos % 8);
            //LOGI("0!!!!%x", *(buf + bufPos / 8));
            //LOGI("0!!!!%x", *(buf + bufPos / 8));
	    //the current byte of buf can contain all bits from data
	    *(buf + bufPos / 8) |= (value << (startPos % 8 - bufPos % 8));
	    //LOGI("0!!!!%x", *(buf + bufPos / 8));
	} else {
	    //the current byte of buf cannot contain all bits from data, split into two bytes
	    //((8 - startPos % 8) - (8 - bufPos % 8)): the bits cannot be contained in current buf byte
	    *(buf + bufPos / 8) |= (unsigned char)(value >> ((8 - startPos % 8) - (8 - bufPos % 8)));
	    *(buf + bufPos / 8 + 1) |= (unsigned char)(value << (8 - ((8 - startPos % 8) - (8 - bufPos % 8))));
	    //LOGI("0!!!!%x\n", *(buf + bufPos / 8));
	    //LOGI("0!!!!%x\n", *(buf + bufPos / 8 + 1));
	}
	bufPos += length;
	bitsCopied += length;
	startPos += length;
    } 
    //2. copy the bytes from data to buf
    numOfAlignedBytes = (count - bitsCopied) / 8;
    for (i = 0; i < numOfAlignedBytes; ++i) {
        value = *(data + startPos / 8);
	//LOGI("**** value: %x\n", value);
	if (8 - startPos % 8 <= 8 - bufPos % 8) {
	    //the current byte of buf can contain all bits from data
	    *(buf + bufPos / 8) |= (value << (startPos % 8 - bufPos % 8));
	    //LOGI("1!!!!%x\n", *(buf + bufPos / 8));
	} else {
	    //the current byte of buf cannot contain all bits from data, split into two bytes
	    //((8 - startPos % 8) - (8 - bufPos % 8)): the bits cannot be contained in current buf byte
	    //LOGI("%x & %x\n", *(buf + bufPos / 8), (unsigned char)(value >> ((8 - startPos % 8) - (8 - bufPos % 8))));
	    //LOGI("%x & %x\n", *(buf + bufPos / 8 + 1), (unsigned char)(value << (8 - ((8 - startPos % 8) - (8 - bufPos % 8)))));
	    *(buf + bufPos / 8) |= (unsigned char)(value >> ((8 - startPos % 8) - (8 - bufPos % 8)));
	    *(buf + bufPos / 8 + 1) |= (unsigned char)(value << (8 - ((8 - startPos % 8) - (8 - bufPos % 8))));
	    //LOGI("1!!!!%x, %d, %d, %x, %x\n", *(buf + bufPos / 8), bufPos, startPos, (unsigned char)(value >> ((8 - startPos % 8) - (8 - bufPos % 8))), (unsigned char)(value << (8 - ((8 - startPos % 8) - (8 - bufPos % 8)))));
	    //LOGI("1!!!!%x\n", *(buf + bufPos / 8 + 1));
	}
	bufPos += 8;
	bitsCopied += 8;
	startPos += 8;
    }
    //3. copy the last few bites from data to buf
    //LOGI("bitsCopied: %d, count: %d\n", bitsCopied, count);
    if (bitsCopied < count) {
	value = (*(data + startPos / 8)) & (0xFF << (8 - (count - bitsCopied)));
	//LOGI("**** value: %x\n", value);
	if (8 - startPos % 8 <= 8 - bufPos % 8) {
	    //the current byte of buf can contain all bits from data
	    *(buf + bufPos / 8) |= (value << (startPos % 8 - bufPos % 8));
	    //LOGI("2!!!!%x\n", *(buf + bufPos / 8));
	} else {
	    //the current byte of buf cannot contain all bits from data, split into two bytes
	    //((8 - startPos % 8) - (8 - bufPos % 8)): the bits cannot be contained in current buf byte
	    *(buf + bufPos / 8) |= (unsigned char)(value >> ((8 - startPos % 8) - (8 - bufPos % 8)));
	    *(buf + bufPos / 8 + 1) |= (unsigned char)(value << (8 - ((8 - startPos % 8) - (8 - bufPos % 8))));
	    //LOGI("2!!!!%x, %d, %d, %x, %x\n", *(buf + bufPos / 8), bufPos, startPos, (unsigned char)(value >> ((8 - startPos % 8) - (8 - bufPos % 8))), (unsigned char)(value << (8 - ((8 - startPos % 8) - (8 - bufPos % 8)))));
	    //LOGI("2!!!!%x\n", *(buf + bufPos / 8 + 1));
	}
	bufPos += (count - bitsCopied);
	startPos += (count - bitsCopied);
    }
    return bufPos;
}

static void compute_mb_mask_from_intra_frame_dependency_for_single_mb(int p_videoFileIndex, int _stFrame, int _frameNum, struct MBIdx _Pmb, int _height, int _width) {
    struct Queue l_q;
    struct MBIdx l_mb, l_mb2;
    //int l_i;
    unsigned char *p, *pframe;

    initQueue(&l_q);
    enqueue(&l_q, _Pmb);
    //pframe = intraDepMap + (_frameNum - _stFrame)*_height*_width*6;
    pframe = intraDepMap + (_frameNum - _stFrame)*_height*_width*4;
    while (ifEmpty(&l_q) == 0) {
        //get the front value
        l_mb = front(&l_q);
        //mark the corresponding position in the mask
        //if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb.h][l_mb.w] > 1) {
	    //it has been tracked before
        //    dequeue(&l_q);
        //    continue;
        //}
        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb.h][l_mb.w]++;
        /*for (l_i = 0; l_i < 2; ++l_i) {
	        //p = pframe + (l_mb.h*_width + l_mb.w)*6 + l_i*2;
            p = pframe + (l_mb.h*_width + l_mb.w)*4 + l_i*2;
            if ((*p !=0) || (*(p+1) != 0)) {
		        l_mb2.h = *p;
            	l_mb2.w = *(p+1);
                if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb2.h][l_mb2.w] <= 1) {
            	    enqueue(&l_q, l_mb2);
                }
	        //fprintf(testF, "enqueue: %d:%d:     ", l_mb2.h, l_mb2.w);
	        }
        }*/
        p = pframe + (l_mb.h*_width + l_mb.w)*4;
        if ((*p !=0) || (*(p+1) != 0)) {
	        l_mb2.h = *p;
        	l_mb2.w = *(p+1);
            if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb2.h][l_mb2.w] <= 1) {
        	    enqueue(&l_q, l_mb2);
            }
        }
        if ((*(p+2) !=0) || (*(p+3) != 0)) {
	        l_mb2.h = *(p+2);
        	l_mb2.w = *(p+3);
            if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb2.h][l_mb2.w] <= 1) {
        	    enqueue(&l_q, l_mb2);
            }
        }
        dequeue(&l_q);
    }
}

/*based on the start pos (_stH, _stW) and end pos (_edH, _edW), compute the mb needed to decode the roi due to inter-frame dependency
for I-frame: intra-frame dependency are due to differential encoding of DC and AC coefficients: mpeg4videodec.c
for P-frame: intra-frame dependency are due to differential encoding of motion vectors: h263.c*/
static void compute_mb_mask_from_intra_frame_dependency(int p_videoFileIndex, int _stFrame, int _frameNum, int _height, int _width) {
   int l_i, l_j;
   struct MBIdx l_mb;
   LOGI(10, "compute_mb_mask_from_intra_frame_dependency started");
   for (l_i = 0; l_i < _height; ++l_i) {
       for (l_j = 0; l_j < _width; ++l_j) {
           //dependency list traversing for a block
           //e.g. mb A has two dependencies mb B and C. We track down to B and C, mark them as needed, then do the same for B and C as we did for A.
           //basically a graph traversal problem.
           if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] == 1) {
               l_mb.h = l_i;
               l_mb.w = l_j;
               compute_mb_mask_from_intra_frame_dependency_for_single_mb(p_videoFileIndex, _stFrame, _frameNum, l_mb, _height, _width);
               //LOGI(10, "%d:%d:%d:%d:%d:%d:%d", p_videoFileIndex, _stFrame, _frameNum, l_mb.h, l_mb.w, _height, _width);
           }
       }
   } 
   LOGI(10, "compute_mb_mask_from_intra_frame_dependency ended");
}

//new approach of computing intra frame dependency: compute starting from last mb backwards, this avoids the queue structure
static void compute_mb_mask_from_intra_frame_dependency_without_queue(int p_videoFileIndex, int _stFrame, int _frameNum, int _height, int _width, unsigned int *_lastMbHeight, unsigned int *_lastMbWidth) {
   int l_i, l_j, l_k;
   unsigned char *p;
   int lFirstTime = 1;
   LOGI(10, "compute_mb_mask_from_intra_frame_dependency_without_queue started");
   //p = intraDepMap + (_frameNum - _stFrame + 1)*_height*_width*6 - 1;
   p = intraDepMap + (_frameNum - _stFrame + 1)*_height*_width*4 - 1;
   for (l_i = _height-1; l_i >= 0; --l_i) {
       for (l_j = _width-1; l_j >= 0; --l_j) {
           //dependency list traversing for a block
           //e.g. mb A has two dependencies mb B and C. We track down to B and C, mark them as needed, then do the same for B and C as we did for A.
           //basically a graph traversal problem.
           if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] == 1) {
               if (lFirstTime) {
                   *_lastMbHeight = l_i; *_lastMbWidth = l_j;
                   lFirstTime = 0;
               }
               //for (l_k = 0; l_k < 3; ++l_k) {
               for (l_k = 0; l_k < 2; ++l_k) {
				    if (((*p !=0) || (*(p-1) != 0)) && (!gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[*(p-1)][*p])) {
				    	gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[*(p-1)][*p] = 1; //mark as 2 instead of 1 to differentiate from inter-frame dep
				    }
                    p -= 2;    
			   }
           } else {
               //p -= 6;
               p -= 4;
           }
       }
   } 
   LOGI(10, "compute_mb_mask_from_intra_frame_dependency_without_queue ended");
}

#ifdef MV_BASED_OPTIMIZATION
//TODO: optimized MV-based dependency traversal: refer to mpegvideo_common.h to see how the motion estimation is performed
/*starting from the last frame of the GOP, calculate the inter-dependency backwards 
first-pass, we calculate based on pixel position for each mb (including complete/partial mb)
second-pass, we convert the pixel-based position to mb-based*/
static void compute_mb_mask_from_inter_frame_dependency(int pVideoFileIndex, int pGopNum, int _stFrame, int _edFrame, int _stH, int _stW, int _edH, int _edW, int ifPreload) {
    LOGI(9, "MV-based: start of compute_mb_mask_from_inter_frame_dependency, preload=%d, %d=%d", ifPreload, pVideoFileIndex, nextInterDepMaskVideoIndex);
    LOGI(9, "%d,%d,%d,%d=%d,%d,%d,%d", nextInterDepMaskStH, nextInterDepMaskStW, nextInterDepMaskEdH, nextInterDepMaskEdW, _stH, _stW, _edH, _edW);
    if ((!ifPreload) && nextInterDepMaskVideoIndex == pVideoFileIndex && nextInterDepMaskStH == _stH && nextInterDepMaskStW == _stW && nextInterDepMaskEdH == _edH && nextInterDepMaskEdW == _edW) {
        if (nextInterDepMaskBuf == 1) {
            pInterDepMask = &interDepMaskBuf1;
        } else {
            pInterDepMask = &interDepMaskBuf2;
        }
    } else {
        int li, lj, lk, lm;
        int l_mbHeight, l_mbWidth;
        int lRoiMbSize;
        short *lMvMap;
        if (ifPreload) {
            if (nextInterDepMaskBuf == 1) {    //1 is in use, we should use 2 for preload
                pInterDepMask = &interDepMaskBuf2;
                nextInterDepMaskBuf = 2;
            } else {
                pInterDepMask = &interDepMaskBuf1;
                nextInterDepMaskBuf = 1;
            }
        } else {
            //if the code reaches here, it means the ROI has changed, the preload has become invalid, we'll need to recompute it
            if (nextInterDepMaskBuf == 1) {    //1 contains the preload data, we should override it
                pInterDepMask = &interDepMaskBuf1;
                nextInterDepMaskBuf = 1;
            } else {
                pInterDepMask = &interDepMaskBuf2;
                nextInterDepMaskBuf = 2;
            }
        }
        l_mbHeight = (gVideoCodecCtxList[pVideoFileIndex]->height + 15) / 16;
        l_mbWidth = (gVideoCodecCtxList[pVideoFileIndex]->width + 15) / 16;
        LOGI(10, "start of compute_mb_mask_from_inter_frame_dependency: %d, %d, [%d:%d] (%d, %d) (%d, %d)", _stFrame, _edFrame, l_mbHeight, l_mbWidth, _stH, _stW, _edH, _edW);
        LOGI(10, "test: %d", (*pInterDepMask)[0][0][0]);
        memset(*pInterDepMask, 0, sizeof((*pInterDepMask)[0][0][0])*MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
        LOGI(10, "memset done, start traversal: %d", _edFrame - _stFrame + 1);
        //optimized MV-based dependency: assume all mb has only MV first
        mvDep = (MBR**)malloc(sizeof(MBR*)*(_edFrame - _stFrame + 1));
        mbCounts = (int*)malloc(sizeof(int)*(_edFrame - _stFrame + 1));
        lMvMap = nextMvMap;
        lRoiMbSize = (_edH - _stH + 1) * (_edW - _stW + 1);   //number of mbs in ROI
        LOGI(10, "number of mbs in roi: %d", lRoiMbSize);
        int frameIdx = _edFrame - _stFrame;
        mvDep[frameIdx] = (MBR*)malloc(sizeof(MBR)*lRoiMbSize);
        mbCounts[frameIdx] = 0;
        //record the ROI for the last frame, there's no inter-frame dependency on it
        for (lj = _stH; lj <= _edH; ++lj) {
            for (lk = _stW; lk <= _edW; ++lk) {
                 mvDep[frameIdx][mbCounts[frameIdx]].tlh = lj*16;  mvDep[frameIdx][mbCounts[frameIdx]].ph = 16;
                 mvDep[frameIdx][mbCounts[frameIdx]].tlw = lk*16;  mvDep[frameIdx][mbCounts[frameIdx]].pw = 16;
                 ++mbCounts[frameIdx];
            }
        }
        LOGI(10, "ROI on last frame is marked");
        //trace dependency for frame N => N-1
        for (li = _edFrame - 1; li >= _stFrame; --li) {      //the _stFrame is I-frame, no MV
            frameIdx = li - _stFrame;            //frame index for N-1 frame
            lRoiMbSize = mbCounts[frameIdx+1]*4; //one mb can be divided to at most 4 patial mbs, so if frame N has x mbs, frame N-1 have at most 4x mbs.
            //LOGI(10, "frame %d: mbs: %d: alloc size: %d, %d", frameIdx, lRoiMbSize, sizeof(MBR)*lRoiMbSize, mvDep[frameIdx]);
            mvDep[frameIdx] = (MBR*)malloc(sizeof(MBR)*lRoiMbSize);
            /*if (mvDep[frameIdx] == NULL) {
                LOGI(10, "malloc failed");
            }*/
            //LOGI(10, "frame %d mem allocated", frameIdx);
            //first record all mbs in ROI
            mbCounts[frameIdx] = 0;
            for (lj = _stH; lj <= _edH; ++lj) {
		        for (lk = _stW; lk <= _edW; ++lk) {
		             mvDep[frameIdx][mbCounts[frameIdx]].tlh = lj*16;  mvDep[frameIdx][mbCounts[frameIdx]].ph = 16;
		             mvDep[frameIdx][mbCounts[frameIdx]].tlw = lk*16;  mvDep[frameIdx][mbCounts[frameIdx]].pw = 16;
		             ++mbCounts[frameIdx];
		        }
		    }
            //LOGI(10, "frame %d: roi set", frameIdx);
            //trace the dependency
            for (lj = 0; lj < mbCounts[frameIdx+1]; ++lj) {
                int mbh = mvDep[frameIdx+1][lj].tlh/16;
                int mbw = mvDep[frameIdx+1][lj].tlw/16;
                lMvMap = nextMvMap + (frameIdx*l_mbHeight*l_mbWidth + mbh*l_mbWidth + mbw)*9;  //move the pointer to first roi mb of the row
                //motion estimation
                short mvx = *lMvMap; 
                short mvy = *(lMvMap + 1);
                //LOGI(10, "%d ouf of %d, %d:%d:%d:%d:", lj, mbCounts[frameIdx+1], mbw, mbh, mvx, mvy);
                int refx = mbw*16 + (mvx >> 1);
                int refy = mbh*16 + (mvy >> 1);
                refx = (refx > 0)?refx:0;
                refy = (refy > 0)?refy:0;
                int phl, pwl;
                //LOGI(10, "%d: %d:%d %d:%d", mbCounts[frameIdx], refx, refy, mvDep[frameIdx+1][lj].pw, mvDep[frameIdx+1][lj].ph);
                //LOGI(10, "%d:%d:%d:%d", _stH, _stW, _edH, _edW);
                if ((refx/16 == (refx+mvDep[frameIdx+1][lj].pw-1)/16) && (refy/16 == (refy+mvDep[frameIdx+1][lj].ph-1)/16)) { //1 mb
                    if (!((refy/16 >= _stH) && (refy/16 <= _edH) && (refx/16 >= _stW) && (refx/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].tlh = refy;
		                mvDep[frameIdx][mbCounts[frameIdx]].tlw = refx;
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = mvDep[frameIdx+1][lj].ph;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = mvDep[frameIdx+1][lj].pw;
		                ++mbCounts[frameIdx];
                    }
                } else if (refx/16 == (refx+mvDep[frameIdx+1][lj].pw-1)/16) {              //2 mbs, divide y/h
                    if (!((refy/16 >= _stH) && (refy/16 <= _edH) && (refx/16 >= _stW) && (refx/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].tlh = refy;
		                mvDep[frameIdx][mbCounts[frameIdx]].tlw = refx;
		                phl = refy/16*16 + 15 - refy;
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = phl;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = mvDep[frameIdx+1][lj].pw;
		                ++mbCounts[frameIdx];
                    }
                    mvDep[frameIdx][mbCounts[frameIdx]].tlh = (refy/16 + 1)*16;
                    mvDep[frameIdx][mbCounts[frameIdx]].tlw = refx;
                    if (!((mvDep[frameIdx][mbCounts[frameIdx]].tlh/16 >= _stH) && (mvDep[frameIdx][mbCounts[frameIdx]].tlh/16 <= _edH) && (refx/16 >= _stW) && (refx/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = mvDep[frameIdx+1][lj].ph - phl;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = mvDep[frameIdx+1][lj].pw;
		                ++mbCounts[frameIdx];
                    }
                } else if (refy/16 == (refy+mvDep[frameIdx+1][lj].ph-1)/16) {              //2 mbs, divide x/w
                    if (!((refy/16 >= _stH) && (refy/16 <= _edH) && (refx/16 >= _stW) && (refx/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].tlh = refy;
		                mvDep[frameIdx][mbCounts[frameIdx]].tlw = refx;
		                pwl = refx/16*16 + 15 - refx;
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = mvDep[frameIdx+1][lj].ph;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = pwl;
		                ++mbCounts[frameIdx];
                    }
	                mvDep[frameIdx][mbCounts[frameIdx]].tlh = refy;
	                mvDep[frameIdx][mbCounts[frameIdx]].tlw = (refx/16 + 1)*16;
                    if (!((refy/16 >= _stH) && (refy/16 <= _edH) && (mvDep[frameIdx][mbCounts[frameIdx]].tlw/16 >= _stW) && (mvDep[frameIdx][mbCounts[frameIdx]].tlw/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = mvDep[frameIdx+1][lj].ph;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = mvDep[frameIdx+1][lj].pw - pwl;
		                ++mbCounts[frameIdx];
                    }
                } else {                                //4 mbs
                    pwl = refx/16*16+15 - refx;
                    phl = refy/16*16+15 - refy;
                    if (!((refy/16 >= _stH) && (refy/16 <= _edH) && (refx/16 >= _stW) && (refx/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].tlh = refy;
		                mvDep[frameIdx][mbCounts[frameIdx]].tlw = refx;
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = phl;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = pwl;
		                ++mbCounts[frameIdx];
                    }
                    mvDep[frameIdx][mbCounts[frameIdx]].tlh = refy;
                    mvDep[frameIdx][mbCounts[frameIdx]].tlw = (refx/16+1)*16;
                    if (!((refy/16 >= _stH) && (refy/16 <= _edH) && (mvDep[frameIdx][mbCounts[frameIdx]].tlw/16 >= _stW) && (mvDep[frameIdx][mbCounts[frameIdx]].tlw/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = phl;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = mvDep[frameIdx+1][lj].pw - pwl;
		                ++mbCounts[frameIdx];
                    }
                    mvDep[frameIdx][mbCounts[frameIdx]].tlh = (refy/16+1)*16;
                    mvDep[frameIdx][mbCounts[frameIdx]].tlw = refx;
                    if (!((mvDep[frameIdx][mbCounts[frameIdx]].tlh/16 >= _stH) && (mvDep[frameIdx][mbCounts[frameIdx]].tlh/16 <= _edH) && (refx/16 >= _stW) && (refx/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = mvDep[frameIdx+1][lj].ph - phl;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = pwl;
		                ++mbCounts[frameIdx];
                    }
                    mvDep[frameIdx][mbCounts[frameIdx]].tlh = (refy/16+1)*16;
                    mvDep[frameIdx][mbCounts[frameIdx]].tlw = (refx/16+1)*16;
                    if (!((mvDep[frameIdx][mbCounts[frameIdx]].tlh/16 >= _stH) && (mvDep[frameIdx][mbCounts[frameIdx]].tlh/16 <= _edH) && (mvDep[frameIdx][mbCounts[frameIdx]].tlw/16 >= _stW) && (mvDep[frameIdx][mbCounts[frameIdx]].tlw/16 <= _edW))) {
		                mvDep[frameIdx][mbCounts[frameIdx]].ph = mvDep[frameIdx+1][lj].ph - phl;
		                mvDep[frameIdx][mbCounts[frameIdx]].pw = mvDep[frameIdx+1][lj].pw - pwl;
		                ++mbCounts[frameIdx];
                    }
                }
            }
            LOGI(10, "frame %d: mb count: %d", frameIdx, mbCounts[frameIdx]);
        }
        LOGI(10, "MV-based computation at pixel level start second pass");
        //second pass, mark the bitmap mask
        for (li = _stFrame; li <= _edFrame; ++li) {
            frameIdx = li - _stFrame;
			//LOGI(10, "**************%d", mbCounts[frameIdx]);
            for (lj = 0; lj < mbCounts[frameIdx]; ++lj) {
				//LOGI(10, "%d:%d:%d", lj, mvDep[frameIdx][lj].tlh, mvDep[frameIdx][lj].tlw);
                (*pInterDepMask)[frameIdx][mvDep[frameIdx][lj].tlh/16][mvDep[frameIdx][lj].tlw/16] = 1;
            }
        }
        LOGI(10, "MV-based computation at pixel level second pass done, start free memory: %d %d", _stFrame, _edFrame);
        for (li = _stFrame; li <= _edFrame; ++li) {
            //LOGI(10, "free %d\n", (li-_stFrame));
			free(mvDep[li - _stFrame]);
        }
        free(mvDep);
        free(mbCounts);
        //TODO: for debug, print the bitmap out
        if (ifPreload) {
            nextInterDepMaskVideoIndex = pVideoFileIndex;
            nextInterDepMaskStH = _stH;
            nextInterDepMaskStW = _stW;
            nextInterDepMaskEdH = _edH;
            nextInterDepMaskEdW = _edW;
        }
    }
    LOGI(10, "MV-based: end of compute_mb_mask_from_inter_frame_dependency");
}

#else
/*starting from the last frame of the GOP, calculate the inter-dependency backwards 
if the calculation is forward, then the case below might occur:
mb 3 in frame 3 depends on mb 2 on frame 2, but mb 2 is not decoded
if we know the roi for the entire GOP, we can pre-calculate the needed mbs at every frame*/
//TODO: the inter dependency list contains some negative values, we haven't figured it out yet
static void compute_mb_mask_from_inter_frame_dependency(int p_videoFileIndex, int pGopNum, int _stFrame, int _edFrame, int _stH, int _stW, int _edH, int _edW, int ifPreload) {
    LOGI(9, "start of compute_mb_mask_from_inter_frame_dependency, preload=%d, %d=%d", ifPreload, p_videoFileIndex, nextInterDepMaskVideoIndex);
    LOGI(9, "%d,%d,%d,%d=%d,%d,%d,%d", nextInterDepMaskStH, nextInterDepMaskStW, nextInterDepMaskEdH, nextInterDepMaskEdW, _stH, _stW, _edH, _edW);
    if ((!ifPreload) && nextInterDepMaskVideoIndex == p_videoFileIndex && nextInterDepMaskStH == _stH && nextInterDepMaskStW == _stW && nextInterDepMaskEdH == _edH && nextInterDepMaskEdW == _edW) {
        //LOGI(9, "get mask from preloading result: %d", sizeof(interDepMask[0][0][0])*MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
        //memcpy(&(interDepMask[0][0][0]), &(nextInterDepMask[0][0][0]), sizeof(int)*MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
        if (nextInterDepMaskBuf == 1) {
            pInterDepMask = &interDepMaskBuf1;
        } else {
            pInterDepMask = &interDepMaskBuf2;
        }
    } else {
        //int (*l_interDepMask)[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
        int l_i, l_j, l_k, l_m;
        int l_mbHeight, l_mbWidth;
        load_inter_frame_mb_dependency(p_videoFileIndex, pGopNum, 0);
        if (ifPreload) {
            if (nextInterDepMaskBuf == 1) {    //1 is in use, we should use 2 for preload
                pInterDepMask = &interDepMaskBuf2;
                nextInterDepMaskBuf = 2;
            } else {
                pInterDepMask = &interDepMaskBuf1;
                nextInterDepMaskBuf = 1;
            }
        } else {
            //if the code reaches here, it means the ROI has changed, the preload has become invalid, we'll need to recompute it
            //load the file first
            if (nextInterDepMaskBuf == 1) {    //1 contains the preload data, we should override it
                pInterDepMask = &interDepMaskBuf1;
                nextInterDepMaskBuf = 1;
            } else {
                pInterDepMask = &interDepMaskBuf2;
                nextInterDepMaskBuf = 2;
            }
        }
        l_mbHeight = (gVideoCodecCtxList[p_videoFileIndex]->height + 15) / 16;
        l_mbWidth = (gVideoCodecCtxList[p_videoFileIndex]->width + 15) / 16;
        LOGI(10, "start of compute_mb_mask_from_inter_frame_dependency: %d, %d, [%d:%d] (%d, %d) (%d, %d)", _stFrame, _edFrame, l_mbHeight, l_mbWidth, _stH, _stW, _edH, _edW);
        LOGI(10, "test: %d", (*pInterDepMask)[0][0][0]);
        memset(*pInterDepMask, 0, sizeof((*pInterDepMask)[0][0][0])*MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
        LOGI(10, "memset done, start traversal");
        //from last frame in the GOP, going backwards to the first frame of the GOP
        //1. mark the roi as needed
        for (l_i = _edFrame; l_i >= _stFrame; --l_i) {
            for (l_j = _stH; l_j <= _edH; ++l_j) {
                //for (l_k = _stW; l_k <= _edW; ++l_k) {
                    //(*pInterDepMask)[l_i - _stFrame][l_j][l_k] = 1;
                //}
                memset(&(*pInterDepMask)[l_i - _stFrame][l_j][_stW], 1, (_edW - _stW));
            }
        }
        LOGI(10, "roi marked as needed");
        //2. based on inter-dependency list, mark the needed mb
        //it's not necessary to process _stFrame, as there's no inter-dependency for it
        for (l_i = _edFrame; l_i >  _stFrame; --l_i) {
            interDepMapMove = interDepMap + (l_i - _stFrame)*l_mbHeight*l_mbWidth*8;
            //as we initialize the interDepMask to zero, we don't have a way to tell whether the upper left mb should be decoded, we always mark it as needed
            (*pInterDepMask)[l_i - 1 - _stFrame][0][0] = 1;
            for (l_j = 0; l_j < l_mbHeight; ++l_j) {
                for (l_k = 0; l_k < l_mbWidth; ++l_k) {
                    if ((*pInterDepMask)[l_i - _stFrame][l_j][l_k] == 1) {
                        for (l_m = 0; l_m < MAX_INTER_DEP_MB; ++l_m) {
                            //mark the needed mb in the previous frame
                            if (((*interDepMapMove) < 0) || (*(interDepMapMove+1) < 0)) {
			    } else if (((*interDepMapMove) == 0) && (*(interDepMapMove+1) == 0)) {
			    } else {
                                (*pInterDepMask)[l_i - 1 - _stFrame][*interDepMapMove][*(interDepMapMove+1)] = 1;
			    }
			    interDepMapMove += 2;
                        }
                    } else {
		        interDepMapMove += 8;
		    }
                }
            }
        }
        LOGI(10, "all frames inter frame dependency mask computed");
        if (ifPreload) {
            nextInterDepMaskVideoIndex = p_videoFileIndex;
            nextInterDepMaskStH = _stH;
            nextInterDepMaskStW = _stW;
            nextInterDepMaskEdH = _edH;
            nextInterDepMaskEdW = _edW;
        }
        LOGI(10, "before unload_inter_frame_mb_dependency");
        //we can unload the inter frame dependency file here
        unload_inter_frame_mb_dependency();
    }
    LOGI(10, "end of compute_mb_mask_from_inter_frame_dependency");
}
#endif

/*called by decoding thread, to check if needs to wait for dumping thread to dump dependency*/
/*or called by dumping thread, see if it needs to generate dependency files for this gop*/
/*return 1 if it's complete; otherwise, return 0*/
int if_dependency_complete(int p_videoFileIndex, int p_gopNum) {
	char l_depGopRecFileName[100], l_depIntraFileName[100], l_depInterFileName[100], l_depMbStPosFileName[100], l_depMbEdPosFileName[100], l_depDcpFileName[100];
	FILE* lGopF;
	int ret, ti, tj;
	/*check if the dependency files exist, if not, we'll need to dump the dependencies*/
#ifdef ANDROID_BUILD
	sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depIntraFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depInterFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depMbStPosFileName, "%s_mbstpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depMbEdPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depDcpFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);  
#else
	sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depIntraFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depInterFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depMbStPosFileName, "%s_mbstpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depMbEdPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depDcpFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);  
#endif
	if ((!if_file_exists(l_depGopRecFileName)) || (!if_file_exists(l_depIntraFileName)) || (!if_file_exists(l_depInterFileName)) || (!if_file_exists(l_depMbStPosFileName)) || (!if_file_exists(l_depMbEdPosFileName)) || (!if_file_exists(l_depDcpFileName))) {
		return 0;
	} else {
		//further check if the gop file contains both start frame and end frame
		lGopF = fopen(l_depGopRecFileName, "r");
		ret = load_gop_info(lGopF, &ti, &tj);
		fclose(lGopF);
		return ((ret == 0)?1:0);
	}
}

int dep_decode_a_video_packet(int p_videoFileIndex) {
    char l_depGopRecFileName[200], l_depIntraFileName[200], l_depInterFileName[200], lmvFileName[200];
    AVFrame *l_videoFrame = avcodec_alloc_frame();
    int l_numOfDecodedFrames, l_frameType;
    int ti, tj;
    FILE *tmpF, *postF;
    unsigned char interDep[8];
    //unsigned char intraDep[6];
    unsigned char intraDep[4];
    char aLine[121], *aToken, testLine[80];
    unsigned char l_depH, l_depW, l_curDepIdx;
    int l_idxF, l_idxH = 0, l_idxW = 0;
    int i, j, k, m;
    int lret, lFrameDumped;
    LOGI(10, "dep_decode_a_video_packet for video: %d", p_videoFileIndex);
    while ((lFrameDumped = av_read_frame(gFormatCtxDepList[p_videoFileIndex], &gVideoPacketDepList[p_videoFileIndex])) >= 0) {
        if (gVideoPacketDepList[p_videoFileIndex].stream_index == gVideoStreamIndexList[p_videoFileIndex]) {
            LOGI(10, "got a video packet, dump dependency: %d", p_videoFileIndex);	
            ++gVideoCodecCtxDepList[p_videoFileIndex]->dep_video_packet_num;
	    /*update the gop information if it's an I-frame
            update: for every GOP, we generate a set of dependency files. It has the following advantages:
            0. It avoids the overhead, and memory issue caused by opearting on big files
            1. It makes it easy to delete some of the files that is already used if we're having a space constraints
               or we never use the dependency files again (in live streaming)
            */
            l_frameType = (gVideoPacketDepList[p_videoFileIndex].data[4] & 0xC0);
            if (l_frameType == 0x00) {    //an I frame packet
                LOGI(10, "dump dependency: got I frame");
                if (gVideoCodecCtxDepList[p_videoFileIndex]->dep_video_packet_num == 1) {
		    //if it's first frame, no action is needed
                } else {
		    //print the end frame number of previoius gop
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency) {
                        fprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF, "%d:\n", gVideoCodecCtxDepList[p_videoFileIndex]->dep_video_packet_num - 1);
                        //close all dependency files for this GOP
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF);
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosF);
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosF);
#ifndef COMPOSE_PACKET_OR_SKIP
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenF);
#endif
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF);
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF);
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF);
#ifdef MV_BASED_DEPENDENCY
                        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_mvF); 
#endif
                        //post processing for inter frame dependency files to make them mmap compatible
                        sprintf(l_depInterFileName, "%s_inter_gop%d.txt.tmp", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                        sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                        tmpF = fopen(l_depInterFileName, "r");
			            postF = fopen(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepFileName, "w");
                        LOGI(10, "...........post processing %s to %s", l_depInterFileName, gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepFileName);
                        while (fgets(aLine, 120, tmpF) != NULL) {
                            memset(interDep, 0, 8);
                            if ((aToken = strtok(aLine, ":")) != NULL)  	//get the frame number, mb position first
	                        l_idxF = atoi(aToken);
                            if ((aToken = strtok(NULL, ":")) != NULL)
	                        l_idxH = atoi(aToken);
                            if ((aToken = strtok(NULL, ":")) != NULL)
	                        l_idxW = atoi(aToken);
                            //get the dependency mb
                            l_curDepIdx = 0;
                            do {
	                        aToken = strtok(NULL, ":");
	                        if (aToken != NULL)  l_depH = (unsigned char) atoi(aToken);
	                        else break;
	                        aToken = strtok(NULL, ":");
	                        if (aToken != NULL)  l_depW = (unsigned char) atoi(aToken);
	                        else break;
	                        //put the dependencies into the array
	                        interDep[l_curDepIdx++] = l_depH;
	                        interDep[l_curDepIdx++] = l_depW;
                            } while (aToken != NULL);
                            fwrite(interDep, 1, 8, postF);
                        }
                        fclose(tmpF);
                        fclose(postF);
			            //post processing for intra-frame dependency
                        sprintf(l_depIntraFileName, "%s_intra_gop%d.txt.tmp", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                        sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                        tmpF = fopen(l_depIntraFileName, "r");
                        postF = fopen(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName, "w");
                        LOGI(10, "...........processing %s to %s", l_depIntraFileName, gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName);
                        while (fgets(aLine, 120, tmpF) != NULL) {
	                    //memset(intraDep, 0, 6);
                        memset(intraDep, 0, 4);
	                    if ((aToken = strtok(aLine, ":")) != NULL)  	//get the frame number, mb position first
		                l_idxF = atoi(aToken);
	                    if ((aToken = strtok(NULL, ":")) != NULL)
		                l_idxH = atoi(aToken);
                            if ((aToken = strtok(NULL, ":")) != NULL)
                                l_idxW = atoi(aToken);
                            do {
                                aToken = strtok(NULL, ":");
                                if (aToken != NULL)  l_depH = (unsigned char) atoi(aToken);
                                else break;
                                aToken = strtok(NULL, ":");
                                if (aToken != NULL)  l_depW = (unsigned char) atoi(aToken);
                                else break;
                                //put the dependencies into the array
                                if ((l_depH == l_idxH - 1) && (l_depW == l_idxW)) {
                                    intraDep[0] = l_depH;
                                    intraDep[1] = l_depW;
                                } else if ((l_depH == l_idxH) && (l_depW == l_idxW - 1)) {
                                    intraDep[2] = l_depH;
                                    intraDep[3] = l_depW;
                                } else {
                                    LOGE(1, "EEEEEEEEEerror: intra dependency unexpected dependency");
				                    exit(1);
                                }
                            } while (aToken != NULL);
	                        fwrite(intraDep, 1, 4, postF);
                        }
                        fclose(tmpF);
                        fclose(postF);
                    }
		            ++gVideoPacketQueueList[p_videoFileIndex].dep_gop_num;
                }	//end of if first frame
		/*check if the dependency files exist, if not, we'll need to dump the dependencies*/
                LOGI(10, "dependency files for video %d gop %d", p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num); 
#ifdef ANDROID_BUILD
                sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(l_depIntraFileName, "%s_intra_gop%d.txt.tmp", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(l_depInterFileName, "%s_inter_gop%d.txt.tmp", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#ifdef MV_BASED_DEPENDENCY
                sprintf(lmvFileName, "%s_mv_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#endif
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(lmvFileName, "%s_mv_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName, "%s_mbstpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenFileName, "%s_mblen_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#else 
                sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(l_depIntraFileName, "%s_intra_gop%d.txt.tmp", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(l_depInterFileName, "%s_inter_gop%d.txt.tmp", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#ifdef MV_BASED_DEPENDENCY
                sprintf(lmvFileName, "%s_mv_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#endif
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(lmvFileName, "%s_mv_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName, "%s_mbstpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenFileName, "%s_mblen_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                sprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#endif
                LOGI(10, "dependency files %s, %s, %s, %s, %s, %s for video %d gop %d", l_depGopRecFileName, l_depIntraFileName, l_depInterFileName, gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName, gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName, gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName, p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);  
#ifdef CLEAR_DEP_BEFORE_START
                remove(l_depGopRecFileName);
                remove(l_depIntraFileName);
                remove(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName);
                remove(l_depInterFileName);
                remove(lMvFileName);
                remove(gVideoCodecCtxDepList[p_videoFileIndex]->g_depInterFileName);
                remove(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName);
                remove(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName);
                remove(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenFileName);
                remove(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName);
#endif  
                gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency = 1;
                if ((if_file_exists(l_depGopRecFileName)) && (if_file_exists(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepFileName)) && (if_file_exists(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepFileName))&& (if_file_exists(lmvFileName)) && (if_file_exists(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName)) && (if_file_exists(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName)) && (if_file_exists(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenFileName)) && (if_file_exists(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName))) {
                //if all files exist, further check l_depGopRecFileName file content, see if it actually contains both GOP start and end frame
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF = fopen(l_depGopRecFileName, "r");					
                    if ((lret = load_gop_info(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF, &ti, &tj)) == 0) {
                        //the file content is complete, don't dump dependency
                        LOGI(10, "%d, dependency info is complete for video %d, gop %d", lret, p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                        gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency = 0;
                    } else {
                        LOGI(10, "%d, gop info not completed, need to dump dependency info", lret);
                    }
                    fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF);
                } else {
                    LOGI(10, "certain dependency files not exist, need to dump dependency info");
                }
                if (gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency) {
                    LOGI(10, "dumping dependency starts from video %d, gop %d", p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF = fopen(l_depGopRecFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF == NULL) {
                        LOGI(10, "cannot open gop file %s to write: %d", l_depGopRecFileName, errno);
                    }
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosF = fopen(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosF == NULL) {
                        LOGI(10, "cannot open mb pos file %s to write: %d", gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosFileName, errno);
                    }
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosF = fopen(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_mbStPosF == NULL) {
                        LOGI(10, "cannot open mb pos file %s to write: %d", gVideoCodecCtxDepList[p_videoFileIndex]->g_mbEdPosFileName, errno);
                    }
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenF = fopen(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenF == NULL) {
                        LOGI(10, "cannot open mb len file %s to write: %d", gVideoCodecCtxDepList[p_videoFileIndex]->g_mbLenFileName, errno);
                    }
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF = fopen(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF == NULL) {
                        LOGE(10, "cannot open dc prediction file %s to write: %d", gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredFileName, errno);
                    }
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF = fopen(l_depIntraFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF == NULL) {
                        LOGI(10, "cannot open intra frame dependency file %s to write: %d", l_depIntraFileName, errno);
                    }
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF = fopen(l_depInterFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF == NULL) {
                        LOGI(10, "cannot open inter frame dependency file %s to write: %d", l_depInterFileName, errno);
                    }
#ifdef MV_BASED_DEPENDENCY
                    gVideoCodecCtxDepList[p_videoFileIndex]->g_mvF = fopen(lmvFileName, "w");
                    if (gVideoCodecCtxDepList[p_videoFileIndex]->g_mvF == NULL) {
                        LOGI(10, "cannot open mv file %s to write: %d", lmvFileName, errno);
                    }
#endif
                    //dump the start frame number for the new gop
                    fprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF, "%d:", gVideoCodecCtxDepList[p_videoFileIndex]->dep_video_packet_num);	
                } 
             } // end of if I-frame
             /*dump the dependency info*/
             if (gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency) {
                 LOGI(10, "dependency info not exist, need to dump dependency info");
                 avcodec_decode_video2_dep(gVideoCodecCtxDepList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacketDepList[p_videoFileIndex]);
             } else {
                 LOGI(10, "dependency info already exists, no need to dump dependency info");
             }
             av_free_packet(&gVideoPacketDepList[p_videoFileIndex]);
             break;
         } else {
	     //it's not a video packet
             //LOGI(10, "%d != %d: it's not a video packet, continue reading!", gVideoPacketDep.stream_index, gVideoStreamIndex);
             av_free_packet(&gVideoPacketDepList[p_videoFileIndex]);
         } //end of if video packet
    } //end of while
    av_free(l_videoFrame);
    return lFrameDumped;
}

int decode_a_video_packet(int p_videoFileIndex, int _roiStH, int _roiStW, int _roiEdH, int _roiEdW) {
    AVFrame *l_videoFrame = avcodec_alloc_frame();
    int l_numOfDecodedFrames, lRet = 0;
    int l_i, l_j;
    int l_mbHeight, l_mbWidth;
    int l_selectiveDecodingDataSize;
    int l_numOfStuffingBits;
    int l_bufPos;
#ifdef DUMP_VIDEO_FRAME_BYTES
    char l_dumpPacketFileName[30];
    FILE *l_packetDumpF;
#endif
#ifdef DUMP_SELECTED_MB_MASK
    FILE *l_maskF;    
#endif
    int *lMbStPos, *lMbEdPos;
    unsigned int lLastMbHeight, lLastMbWidth;
    /*read the next video packet*/
    LOGI(10, "decode_a_video_packet %d: (%d, %d) (%d, %d)", gVideoPacketNum, _roiStH, _roiStW, _roiEdH, _roiEdW);
    if (gVideoCodecCtxList[p_videoFileIndex]->debug_selective == 1) {
#ifdef ANDROID_BUILD
        FILE *dctF = fopen("/sdcard/r10videocam/debug_dct.txt", "a+");
#else
        FILE *dctF = fopen("./debug_dct.txt", "a+");
#endif
        fprintf(dctF, "#####%d#####\n", gVideoPacketNum);
        fclose(dctF);
    }
    for (;;) {
        int lGetPacketStatus = -1;
        //advance the rest of the video files by 1 frame, then read the selected video file
        //TODO: is there a better way than reading the frame and free the space??? Maybe need to modify the lower layer
        LOGI(10, "skip the non selected video file frames");
        for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
            if (l_i != p_videoFileIndex) {
                LOGI(10, "av_read_frame: %d,  %d", l_i, p_videoFileIndex);				
                lGetPacketStatus = av_read_frame(gFormatCtxList[l_i], &gVideoPacket);
                av_free_packet(&gVideoPacket);
	    	}
        }
        LOGI(10, "read selected video file frame for video %d", p_videoFileIndex);
        if (gVideoCodecCtxList[p_videoFileIndex]->dump_dependency) {
            //lGetPacketStatus = packet_queue_get(&gVideoPacketQueue, &gVideoPacket);
            lGetPacketStatus = av_read_frame(gFormatCtxList[p_videoFileIndex], &gVideoPacket);
        } else {
            lGetPacketStatus = av_read_frame(gFormatCtxList[p_videoFileIndex], &gVideoPacket);
        }
        if (lGetPacketStatus < 0 || gVideoPacketNum == 336) {
			LOGI(10, "cannot get a video packet");
			break;
		}
        if (gVideoPacket.stream_index == gVideoStreamIndexList[p_videoFileIndex]) {
	    //it's a video packet
	    LOGI(3, "got a video packet, decode it");
#ifdef SELECTIVE_DECODING
            LOGI(1, "---CMP ST");
            l_mbHeight = (gVideoCodecCtxList[p_videoFileIndex]->height + 15) / 16;
            l_mbWidth = (gVideoCodecCtxList[p_videoFileIndex]->width + 15) / 16;
            LOGI(10, "selective decoding enabled: %d, %d", l_mbHeight, l_mbWidth);
            /*initialize the mask to all mb unselected*/
            //TODO: use memset
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                //for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    //gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 0;
                //}
                memset(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i], 0, l_mbWidth);
            }
            LOGI(10, "selected_mb_mask reseted");
            //add the needed mb mask based on inter-dependency, which is pre-computed before start decoding a gop
//[DEBUG]: 
	/*int i, j, k;
        FILE *tf = fopen("./interframe.txt", "w");
        FILE *tf1 = fopen("./interframe1.txt", "w");
        LOGI(8, "some debugging info");
        for (i = 0; i < MAX_FRAME_NUM_IN_GOP; ++i) {
            LOGI(8, "-----------------%d----------------\n", i);
            fprintf(tf, "-------------------%d----------------\n", i);
            for (j = 0; j < 45; ++j) {
                 for (k = 0; k < 80; ++k) {
                     fprintf(tf, "%d:", interDepMask[i][j][k]);
		     fprintf(tf1, "%d:", nextInterDepMask[i][j][k]);
                 }
                 fprintf(tf, "\n");
                 fprintf(tf1, "\n");
            }
            fprintf(tf, "\n");
            fprintf(tf1, "\n");
        }
        fclose(tf);
        fclose(tf1);*/
            //TODO: simply assign the memory using pointers, it's faster
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                /*for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    if ((*pInterDepMask)[gVideoPacketNum - gStFrame][l_i][l_j] == 1) {
                        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 1;
                    } 
                }*/
                memcpy(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i], (*pInterDepMask)[gVideoPacketNum - gStFrame][l_i], l_mbWidth);
            }
            LOGI(10, "roi marked");
#ifdef DUMP_SELECTIVE_DEP
	    FILE *l_interF;
	    char l_interFName[50];
#ifdef ANDROID_BUILD
	    sprintf(l_interFName, "/sdcard/r10videocam/%d/inter.txt", gVideoPacketNum);
#else
	    sprintf(l_interFName, "./%d/inter.txt", gVideoPacketNum);
#endif	/*ANDROID_BUILD*/
	    LOGI(10, "filename: %s", l_interFName);
	    l_interF = fopen(l_interFName, "w");
	    if (l_interF != NULL) {
	        for (l_i = 0; l_i < l_mbHeight; ++l_i) {
	            for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		        fprintf(l_interF, "%d:", gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]>0?1:0);
		    }
	 	    fprintf(l_interF, "\n");
	        }
	        fclose(l_interF);
	    } else {
		LOGE(1, "cannot open l_interFName");
	    }
#endif /*DUMP_SELECTIVE_DEP*/
            LOGI(3, "inter frame dependency counted");
            //compute the needed mb mask based on intra-dependency
            //mark all the mb in ROI as needed first
            /*for (l_i = _roiStH; l_i < _roiEdH; ++l_i) {
                for (l_j = _roiStW; l_j < _roiEdW; ++l_j) {
                    gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 1;
                }
            }*/
            //new method: for I-frame, get a sqaure of [0,0][y,x]
            if (gVideoPacketNum == gStFrame) {
                //I-frame
                for (l_i = 0; l_i < _roiEdH; ++l_i) {
                    memset(&(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][0]), 0xFF, _roiEdW);
                }
            } else {
                //P-frame
#ifdef INTRA_DEP_OPTIMIZATION
//P-frame: an optimization to trace only the top row and left row
	#ifdef MV_BASED_DEPENDENCY
                memset(&(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[_roiStH][_roiStW]), 0x01, _roiEdW - _roiStW + 1);
                for (l_i = _roiStH + 1; l_i <= _roiEdH; ++l_i) {
                    memset(&(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][_roiStW+1]), 0xFF, _roiEdW - _roiStW);
                    gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][_roiStW] = 1;
                }
	#else
     			for (l_i = _roiEdH - 1, l_j = 0; l_i >= 0; --l_i, ++l_j) {
					memset(&(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][0]), 0xFF, (_roiEdW + l_j) > l_mbWidth? l_mbWidth:(_roiEdW + l_j));
				}
	#endif
#else
                for (l_i = _roiEdH - 1, l_j = 0; l_i >= 0; --l_i, ++l_j) {
#ifdef MV_BASED_DEPENDENCY
                    memset(&(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][0]), 0xFF, _roiEdW);
#else
                    memset(&(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][0]), 0xFF, (_roiEdW + l_j) > l_mbWidth? l_mbWidth:(_roiEdW + l_j));
#endif
                }
#endif
            }
 	        //load the dc prediction direction
            load_frame_dc_pred_direction(p_videoFileIndex, l_mbHeight, l_mbWidth);
/*
#ifdef DUMP_SELECTIVE_DEP
	    FILE *l_dcpF;
	    char l_dcpFName[50];
#ifdef ANDROID_BUILD
	    sprintf(l_dcpFName, "/sdcard/r10videocam/%d/dcp.txt", gVideoPacketNum);
#else
	    sprintf(l_dcpFName, "./%d/dcp.txt", gVideoPacketNum);
#endif
	    l_dcpF = fopen(l_dcpFName, "w");
	    for (l_i = 0; l_i < l_mbHeight; ++l_i) {
            for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		        fprintf(l_dcpF, "%d:", gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_i][l_j]);
	        }
	 	    fprintf(l_dcpF, "\n");
	    }
	    fclose(l_dcpF);
#endif	//DUMP_SELECTIVE_DEP
*/
            //mark all mb needed due to intra-frame dependency
            //compute_mb_mask_from_intra_frame_dependency(p_videoFileIndex, gStFrame, gVideoPacketNum, l_mbHeight, l_mbWidth); 
            compute_mb_mask_from_intra_frame_dependency_without_queue(p_videoFileIndex, gStFrame, gVideoPacketNum, l_mbHeight, l_mbWidth, &lLastMbHeight, &lLastMbWidth); 
            if (lLastMbHeight < _roiEdH-1) {
                lLastMbHeight = _roiEdH-1;
                lLastMbWidth = _roiEdW-1;
            } else if (lLastMbHeight == _roiEdH-1) {
                if (lLastMbWidth <  _roiEdW-1) {
                    lLastMbWidth = _roiEdW-1;
                }
            }
            //if a mb is selected multiple times, set it to 1
            /*for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		            if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] > 1) {
                        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 1;
                    }
                }
            }*/
#ifdef DUMP_SELECTIVE_DEP
	    FILE *l_intraF;
	    char l_intraFName[50];
	#ifdef ANDROID_BUILD
			sprintf(l_intraFName, "/sdcard/r10videocam/%d/intra.txt", gVideoPacketNum);
	#else
			sprintf(l_intraFName, "./%d/intra.txt", gVideoPacketNum);
	#endif
	    l_intraF = fopen(l_intraFName, "w");
	    for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		    fprintf(l_intraF, "%d:", gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]>0?1:0);
	        }
	 	fprintf(l_intraF, "\n");
	    }
	    fclose(l_intraF);
#endif	/*DUMP_SELECTIVE_DEP*/
#ifdef DUMP_SELECTED_MB_MASK
	    if (gVideoPacketNum == 1) {
	#ifdef ANDROID_BUILD
			    l_maskF = fopen("/sdcard/r10videocam/debug_mask.txt", "w");
	#else
		            l_maskF = fopen("./debug_mask.txt", "w");
	#endif
	    } else {
	#ifdef ANDROID_BUILD
		            l_maskF = fopen("/sdcard/r10videocam/debug_mask.txt", "a+");
	#else
		            l_maskF = fopen("./debug_mask.txt", "a+");
	#endif
	    }
	    fprintf(l_maskF, "-----%d-----\n", gVideoPacketNum);
	    for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		    fprintf(l_maskF, "%d ", gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]);
		}
		fprintf(l_maskF, "\n");
	    }
	    fclose(l_maskF);
#endif	/*DUMP_SELECTED_MB_MASK*/
        LOGI(1, "---CMP ED");
        LOGI(1, "---COMPOSE ST");
#ifdef COMPOSE_PACKET_OR_SKIP
        //based on the mask, compose the video packet
	    lMbStPos = mbStartPos;
	    lMbEdPos = mbEndPos;
        l_selectiveDecodingDataSize = 0;
	    lMbStPos += (gVideoPacketNum - gStFrame)*l_mbHeight*l_mbWidth;
	    lMbEdPos += (gVideoPacketNum - gStFrame)*l_mbHeight*l_mbWidth;
        l_selectiveDecodingDataSize += *lMbStPos;
        //get the size for needed mbs
        for (l_i = 0; l_i < l_mbHeight; ++l_i) {
            for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]) {
                    l_selectiveDecodingDataSize += (*lMbEdPos) - (*lMbStPos);
                } 
                ++lMbEdPos;
                ++lMbStPos;
            }
        } 
        LOGI(10, "total number of bits: %d", l_selectiveDecodingDataSize);
        l_numOfStuffingBits = (l_selectiveDecodingDataSize + 7) / 8 * 8 - l_selectiveDecodingDataSize;
        l_selectiveDecodingDataSize = (l_selectiveDecodingDataSize + 7) / 8;
        LOGI(10, "total number of bytes: %d; number of stuffing bits: %d", l_selectiveDecodingDataSize, l_numOfStuffingBits);
        memcpy(&gVideoPacket2, &gVideoPacket, sizeof(gVideoPacket));
        gVideoPacket2.data = av_malloc(l_selectiveDecodingDataSize + FF_INPUT_BUFFER_PADDING_SIZE);
        gVideoPacket2.size = l_selectiveDecodingDataSize;
        memset(gVideoPacket2.data, 0, gVideoPacket2.size + FF_INPUT_BUFFER_PADDING_SIZE);
        l_bufPos = 0;
        lMbStPos = mbStartPos, lMbEdPos = mbEndPos;
        lMbStPos += (gVideoPacketNum - gStFrame)*l_mbHeight*l_mbWidth;
        lMbEdPos += (gVideoPacketNum - gStFrame)*l_mbHeight*l_mbWidth;
        //l_bufPos = copy_bits(gVideoPacket.data, gVideoPacket2.data, 0, mbStartPos[gVideoPacketNum - gStFrame][0][0], l_bufPos);
        l_bufPos = copy_bits(gVideoPacket.data, gVideoPacket2.data, 0, *lMbStPos, l_bufPos);
        LOGI(10, "%d bits for header: video packet: %d; start frame: %d", *lMbStPos, gVideoPacketNum, gStFrame);
        for (l_i = 0; l_i < l_mbHeight; ++l_i) {
            for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                //put the data bits into the composed video packet
                if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]) {
                    l_bufPos = copy_bits(gVideoPacket.data, gVideoPacket2.data, *lMbStPos, (*lMbEdPos) - (*lMbStPos), l_bufPos);
                }
                ++lMbEdPos;
                ++lMbStPos;
            }
        }
        //stuffing the last byte
        for (l_i = 0; l_i < l_numOfStuffingBits; ++l_i) {
            gVideoPacket2.data[l_selectiveDecodingDataSize - 1] |= (0x01 << l_i);
        }
#else
        /*lMbEdPos = mbEndPos;
        l_selectiveDecodingDataSize = 0;
	    lMbEdPos += (gVideoPacketNum - gStFrame)*l_mbHeight*l_mbWidth + lLastMbHeight*l_mbWidth + lLastMbWidth;
        l_selectiveDecodingDataSize = (*lMbEdPos);
        l_numOfStuffingBits = (l_selectiveDecodingDataSize + 7) / 8 * 8 - l_selectiveDecodingDataSize;
        l_selectiveDecodingDataSize = (l_selectiveDecodingDataSize + 7) / 8;
        gVideoPacket.size = l_selectiveDecodingDataSize;
        //stuffing the last byte
        for (l_i = 0; l_i < l_numOfStuffingBits; ++l_i) {
            gVideoPacket.data[l_selectiveDecodingDataSize - 1] |= (0x01 << l_i);
        }*/
#endif 
        LOGI(1, "---COMPOSE ED");
    #ifdef DUMP_VIDEO_FRAME_BYTES
	    sprintf(l_dumpPacketFileName, "debug_packet_dump_%d_%d.txt", gVideoPacketNum, gVideoCodecCtxList[p_videoFileIndex]->dump_dependency);
	    l_packetDumpF = fopen(l_dumpPacketFileName, "wb");
	    fwrite(gVideoPacket2.data, 1, gVideoPacket2.size, l_packetDumpF);
	    fclose(l_packetDumpF);

	    sprintf(l_dumpPacketFileName, "debug_packet_dump_%d_base.txt", gVideoPacketNum);
	    l_packetDumpF = fopen(l_dumpPacketFileName, "wb");
	    fwrite(gVideoPacket.data, 1, gVideoPacket.size, l_packetDumpF);
	    fclose(l_packetDumpF);
    #endif
            //LOGI(1, "---CMP ED");
            LOGI(1, "---DECODE ST");
#ifdef COMPOSE_PACKET_OR_SKIP
            avcodec_decode_video2(gVideoCodecCtxList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket2);
#else
            avcodec_decode_video2(gVideoCodecCtxList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket);
#endif
            LOGI(1, "---DECODE ED");
#else
   #ifdef DUMP_VIDEO_FRAME_BYTES
	    sprintf(l_dumpPacketFileName, "debug_packet_dump_%d_full.txt", gVideoPacketNum);
	    l_packetDumpF = fopen(l_dumpPacketFileName, "wb");
	    fwrite(gVideoPacket.data, 1, gVideoPacket.size, l_packetDumpF);
	    fclose(l_packetDumpF);
    #endif
	    LOGI(3, "avcodec_decode_video2: %d: %d", p_videoFileIndex, &gVideoPacket==NULL);
	    LOGI(1, "---DECODE ST");
        avcodec_decode_video2(gVideoCodecCtxList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket);
        LOGI(1, "---DECODE ED");
#endif	/*SELECTIVE_DECODING*/
	    LOGI(3, "avcodec_decode_video2 result: %d", l_numOfDecodedFrames);
	    //TODO: keep gVideoPicture, avoid avpicture_alloc, unless there's a change
	    if (l_numOfDecodedFrames) {
#ifdef ANDROID_BUILD
        avpicture_alloc(&gVideoPicture.data, PIX_FMT_YUV420P, gVideoPicture.width, gVideoPicture.height);
		LOGI(3, "video color space: %d, %d, gVideoPicture.width=%d, gVideoPicture.height=%d\n", gVideoCodecCtxList[p_videoFileIndex]->pix_fmt, PIX_FMT_YUV420P, gVideoPicture.width, gVideoPicture.height);
		if (gVideoCodecCtxList[p_videoFileIndex]->pix_fmt == PIX_FMT_YUV420P) {
                    LOGI(3, "video color space is YUV420, convert to RGB: %d; %d; %d, %d, %d", l_videoFrame->linesize[0], l_videoFrame->linesize[1], l_videoFrame->linesize[2], gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height);
                    //we scale the YUV first
                    LOGI(1, "SCALE ST");
                    I420Scale(l_videoFrame->data[0], l_videoFrame->linesize[0],
                             l_videoFrame->data[1], l_videoFrame->linesize[1],
                             l_videoFrame->data[2], l_videoFrame->linesize[2],
                             gVideoCodecCtxList[p_videoFileIndex]->width,
                             gVideoCodecCtxList[p_videoFileIndex]->height,
                             gVideoPicture.data.data[0], gVideoPicture.width,
                             gVideoPicture.data.data[1], gVideoPicture.width>>1,
                             gVideoPicture.data.data[2], gVideoPicture.width>>1,
                             gVideoPicture.width, gVideoPicture.height,
                             kFilterNone);
                     LOGI(1, "SCALE ED");
                     //if it's YUV 420
                     LOGI(1, "COLOR ST");
                     // convert the unscaled data
				//_yuv420_2_rgb8888(gBitmap, 
				//		l_videoFrame->data[0], 
				//		l_videoFrame->data[2],
				//		l_videoFrame->data[1], 
				//		gVideoCodecCtxList[p_videoFileIndex]->width,		//width
				//		gVideoCodecCtxList[p_videoFileIndex]->height, 		//height
				//		//gVideoCodecCtxList[p_videoFileIndex]->width,		//Y span/pitch: No. of bytes in a row
				//		//gVideoCodecCtxList[p_videoFileIndex]->width>>1,		//UV span/pitch
				//		l_videoFrame->linesize[0],
				//		l_videoFrame->linesize[1],
				//		gVideoCodecCtxList[p_videoFileIndex]->width<<2,		//bitmap span/pitch
				//		//l_videoFrame->linesize[0]<<2,
				//		yuv2rgb565_table,
				//		0
				//		);
				// convert the scaled data
/*				_yuv420_2_rgb8888(gBitmap, gVideoPicture.data.data[0], gVideoPicture.data.data[2], gVideoPicture.data.data[1], 
						gVideoPicture.width,								//width
						gVideoPicture.height, 								//height
						gVideoPicture.width,	//Y span/pitch: No. of bytes in a row
						gVideoPicture.width>>1,								//UV span/pitch
						//l_videoFrame->linesize[0],
						//l_videoFrame->linesize[1],
						gVideoPicture.width<<2,		//bitmap span/pitch
						//l_videoFrame->linesize[0]<<2,
						yuv2rgb565_table,
						0
						);*/
                    //convert the scaled data: neon version
                    _yuv420_2_rgb8888_neon(gBitmap, 
						gVideoPicture.data.data[0], 
						gVideoPicture.data.data[2],
						gVideoPicture.data.data[1], 
						gVideoPicture.width,								//width
						gVideoPicture.height, 								//height
						gVideoPicture.width,								//Y span/pitch: No. of bytes in a row
						gVideoPicture.width>>1,								//UV span/pitch
						gVideoPicture.width<<2								//bitmap span/pitch
						);
					/*_yuv420_2_rgb8888_neon(gBitmap, 
						l_videoFrame->data[0], 
						l_videoFrame->data[2],
						l_videoFrame->data[1], 
						gVideoPicture.width,								//width
						gVideoPicture.height, 								//height
						l_videoFrame->linesize[0],								//Y span/pitch: No. of bytes in a row
						l_videoFrame->linesize[1],								//UV span/pitch
						gVideoPicture.width<<2								//bitmap span/pitch
						);*/
                    LOGI(1, "COLOR ED");
                    lRet = 1;
                } else { //TODO: color space is not YUV
                    lRet = 0;
                }
                LOGI(3, "video packet conversion done, start free memory");
                /*free gVideoPicture*/
		avpicture_free(&gVideoPicture.data);	
#else
                LOGI(10, "video packet decoded, start conversion, allocate a picture (%d, %d)", gVideoPicture.width, gVideoPicture.height);
                //allocate the memory space for a new VideoPicture
                avpicture_alloc(&gVideoPicture.data, PIX_FMT_RGBA, gVideoPicture.width, gVideoPicture.height);
                //convert the frame to RGB format
                LOGI(3, "video picture data allocated, try to get a sws context: %d, %d", gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height);
                gImgConvertCtx = sws_getCachedContext(gImgConvertCtx, gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height, gVideoCodecCtxList[p_videoFileIndex]->pix_fmt, gVideoPicture.width, gVideoPicture.height, PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);
                if (gImgConvertCtx == NULL) {
                    LOGE(1, "Error initialize the video frame conversion context");
                }
                LOGI(3, "got sws context, try to scale the video frame: from (%d, %d) to (%d, %d)", gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height, gVideoPicture.width, gVideoPicture.height);
                sws_scale(gImgConvertCtx, l_videoFrame->data, l_videoFrame->linesize, 0, gVideoCodecCtxList[p_videoFileIndex]->height, gVideoPicture.data.data, gVideoPicture.data.linesize);		   
#endif	
            }
	    /*free the packet*/
	    av_free_packet(&gVideoPacket);
#ifdef SELECTIVE_DECODING
            av_free_packet(&gVideoPacket2);
#endif 
	    break;
        } else {
	    //it's not a video packet
	    LOGI(10, "it's not a video packet, continue reading! %d != %d", gVideoPacket.stream_index, gVideoStreamIndexList[p_videoFileIndex]);
            av_free_packet(&gVideoPacket);
        }
    }
    av_free(l_videoFrame);
    return lRet;
}

int get_gop_info_given_gop_num(int p_videoFileIndex, int pGopNum, int *pStartF, int *pEndF) {
    char lGopFileName[200];
    char l_gopRecLine[150];
    char *l_aToken;
#ifdef ANDROID_BUILD
    sprintf(lGopFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#else
    sprintf(lGopFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], pGopNum);
#endif
    FILE *gopRecFile = fopen(lGopFileName, "r");
    *pStartF = 0;
    *pEndF = 0;
    LOGI(10, "load gop info given gop number start: %s", lGopFileName);
    if (fgets(l_gopRecLine, 50, gopRecFile) == NULL) {
        fclose(gopRecFile);
        return -1;
    }
    LOGI(10, "gop line: %s", l_gopRecLine);
    if ((l_aToken = strtok(l_gopRecLine, ":")) != NULL) {
        LOGI(10, "a token: %s", l_aToken);
        *pStartF = atoi(l_aToken);
    } else {
        fclose(gopRecFile);
    	return -1;
    }
    if ((l_aToken = strtok(NULL, ":")) != NULL) {
        LOGI(10, "another token: %s", l_aToken);
        *pEndF = atoi(l_aToken);
    } else {
        fclose(gopRecFile);
    	return -1;
    }
    LOGI(10, "load gop info given gop number ends: %d-%d", *pStartF, *pEndF);
    fclose(gopRecFile);
    if (((*pStartF) > 0) && ((*pEndF) >= (*pStartF))) 
        return 0;
    else
        return -1;
}

/*load the gop information, return 0 if everything goes well; otherwise, return a non-zero value*/
int load_gop_info(FILE* p_gopRecFile, int *p_startF, int *p_endF) {
    char l_gopRecLine[150];
    char *l_aToken;
    //char l_aToken[10];
    *p_startF = 0;
    *p_endF = 0;
    LOGI(10, "load gop info start:");
    if (fgets(l_gopRecLine, 50, p_gopRecFile) == NULL) {
        LOGE(1, "cannot read gop file line");
        return -1;
    }
    LOGI(10, "gop line: %s", l_gopRecLine);
    if ((l_aToken = strtok(l_gopRecLine, ":")) != NULL) {
        LOGI(10, "a token: %s", l_aToken);
        *p_startF = atoi(l_aToken);
    } else
    	return -1;
    if ((l_aToken = strtok(NULL, ":")) != NULL) {
        LOGI(10, "another token: %s", l_aToken);
        *p_endF = atoi(l_aToken);
    } else
    	return -1;
    LOGI(10, "load gop info ends: %d-%d", *p_startF, *p_endF);
    if (((*p_startF) > 0) && ((*p_endF) >= (*p_startF))) 
        return 0;
    else
        return -1;
}

/*load the pre computation for a gop and also compute the inter frame dependency for a gop*/
void prepare_decode_of_gop(int p_videoFileIndex, int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw) {
    LOGI(9, "prepare decode of gop started: %d, %d, %d, %d", _roiSh, _roiSw, _roiEh, _roiEw);
    gRoiSh = _roiSh;
    gRoiSw = _roiSw;
    gRoiEh = _roiEh;
    gRoiEw = _roiEw;
    //load_pre_computation_result(p_videoFileIndex, _stFrame, _edFrame);
    load_pre_computation_result(p_videoFileIndex);
    compute_mb_mask_from_inter_frame_dependency(p_videoFileIndex, g_decode_gop_num, _stFrame, _edFrame, _roiSh, _roiSw, _roiEh, _roiEw, 0);
    LOGI(9, "prepare decode of gop ended");
}

