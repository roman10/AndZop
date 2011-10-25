#include "dependency.h"


static int videoIndexCmp(const int *a, const int *b) {
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
    gFormatCtxList = (AVFormatContext*)malloc(gNumOfVideoFiles*sizeof(AVFormatContext));
    gVideoStreamIndexList = (int*)malloc(gNumOfVideoFiles*sizeof(int));
    gVideoCodecCtxList = (AVCodecContext*)malloc(gNumOfVideoFiles*sizeof(AVCodecContext));
    gVideoPacketQueueList = (PacketQueue*)malloc(gNumOfVideoFiles*sizeof(PacketQueue));
	gFormatCtxDepList = (AVFormatContext*)malloc(gNumOfVideoFiles*sizeof(AVFormatContext));
    gVideoCodecCtxDepList = (AVCodecContext*)malloc(gNumOfVideoFiles*sizeof(AVCodecContext));
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
void allocate_selected_decoding_fields(int p_videoFileIndex, int _mbHeight, int _mbWidth) {
    int l_i;
	LOGI(10, "allocate %d video selected decoding fields: %d, %d", p_videoFileIndex, _mbHeight, _mbWidth);
    gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask = (unsigned char **) malloc(_mbHeight * sizeof(unsigned char *));
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i] = (unsigned char *) malloc(_mbWidth * sizeof(unsigned char));
    }
    gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir = (unsigned char **) malloc(_mbHeight * sizeof(unsigned char *));
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_i] = (unsigned char *) malloc(_mbWidth * sizeof(unsigned char));
    }
	LOGI(10, "allocate %d video selected decoding fields: %d, %d is done", p_videoFileIndex, _mbHeight, _mbWidth);
}

//TODO: more fields to clear
void free_selected_decoding_fields(int p_videoFileIndex, int _mbHeight) {
    int l_i;
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        free(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i]);
    }
    free(gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask);
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        free(gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_i]);
    }
    free(gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir);
}

int mbStartPos[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
int mbEndPos[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];

struct MBIdx intraDep[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W][MAX_DEP_MB];
struct MBIdx interDep[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W][MAX_DEP_MB];
int interDepMask[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];

/*load frame mb index from frame _stFrame to frame _edFrame*/
static void load_frame_mb_index(int p_videoFileIndex, int _stFrame, int _edFrame) {
    char aLine[30];
	//char temp[30];
    char *aToken;
    int idxF, idxH, idxW, stP, edP;
    LOGI(10, "+++++++load_frame_mb_index: %d to %d\n", _stFrame, _edFrame);
    if (gVideoCodecCtxList[p_videoFileIndex]->g_mbPosF == NULL) {
        LOGE(1, "Error: no valid mb index records!!!");
    }
    memset(mbStartPos, 0, MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
    memset(mbEndPos, 0, MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
    idxF = 0; idxH = 0; idxW = 0;
    while (fgets(aLine, 30, gVideoCodecCtxList[p_videoFileIndex]->g_mbPosF) != NULL) {
		//strcpy(temp, aLine);
        //parse the line
		if ((idxF == _edFrame) && (strcmp(aLine, "\n") == 0)) {
			//we must continue to read to the last line of this frame, otherwise, only the first line of 
			//this frame is read, the empty space indicates the end of a frame
			LOGI(10, "+++++++load_frame_mb_index finished");
			break;
		}
		if (idxF > _edFrame) {
			LOGI(10, "+++++++load_frame_mb_index finished, overread");
			break;
		}
        //LOGI(10, "line in mb pos file: %s", aLine);
        if ((aToken = strtok(aLine, ":")) != NULL)
            idxF = atoi(aToken);
        if (idxF < _stFrame) {
            //not the start frame yet, continue reading
	    	//LOGI(10, "not the start frame yet, continue reading, %d:%d-%s", idxF, _stFrame, temp);
            continue;
        } 
        if ((aToken = strtok(NULL, ":")) != NULL)
            idxH = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            idxW = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            stP = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            edP = atoi(aToken);
        mbStartPos[idxF - _stFrame][idxH][idxW] = stP;
        mbEndPos[idxF - _stFrame][idxH][idxW] = edP;
    }
     LOGI(10, "+++++++load_frame_mb_index finished, exit the function");
}

static void load_intra_frame_mb_dependency(int p_videoFileIndex, int _stFrame, int _edFrame) {
    char aLine[40], *aToken;
    int l_idxF, l_idxH, l_idxW, l_depH, l_depW, l_curDepIdx;
    LOGI(10, "load_intra_frame_mb_dependency\n");
    if (gVideoCodecCtxList[p_videoFileIndex]->g_intraDepF == NULL) {
         LOGE(1, "no valid intra frame mb dependency!!!");
    }
    for (l_idxF = 0; l_idxF < MAX_FRAME_NUM_IN_GOP; ++l_idxF) {
        for (l_idxH = 0; l_idxH < MAX_MB_H; ++l_idxH) {
            for (l_idxW = 0; l_idxW < MAX_MB_W; ++l_idxW) {
                for (l_curDepIdx = 0; l_curDepIdx < MAX_DEP_MB; ++l_curDepIdx) {
		    intraDep[l_idxF][l_idxH][l_idxW][l_curDepIdx].h = -1;
		    intraDep[l_idxF][l_idxH][l_idxW][l_curDepIdx].w = -1;
                }
            }
        }
    }
    while (fgets(aLine, 40, gVideoCodecCtxList[p_videoFileIndex]->g_intraDepF) != NULL) {
        //parse the line
        //get the frame number, mb position first
        if ((aToken = strtok(aLine, ":")) != NULL)
            l_idxF = atoi(aToken);
        if (l_idxF < _stFrame) {
            continue;
        } 
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxH = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxW = atoi(aToken);
        l_curDepIdx = 0;
        do {
            aToken = strtok(NULL, ":");
            if (aToken != NULL) l_depH = atoi(aToken);
            else break;
            aToken = strtok(NULL, ":");
            if (aToken != NULL) l_depW = atoi(aToken);
            else break;
            //put the dependencies into the array
            intraDep[l_idxF - _stFrame][l_idxH][l_idxW][l_curDepIdx].h = l_depH;
            intraDep[l_idxF - _stFrame][l_idxH][l_idxW][l_curDepIdx++].w = l_depW;
        } while (aToken != NULL);
	if (l_idxF == _edFrame) {
	    break;
	}
    }
}

static void load_inter_frame_mb_dependency(int p_videoFileIndex, int _stFrame, int _edFrame) {
    char aLine[40], *aToken;
    int l_idxF, l_idxH, l_idxW, l_depH, l_depW, l_curDepIdx;
    LOGI(10, "load_inter_frame_mb_dependency for video %d: %d: %d\n", p_videoFileIndex, _stFrame, _edFrame);
    if (gVideoCodecCtxList[p_videoFileIndex]->g_interDepF == NULL) {
        LOGE(1, "no valid inter frame mb dependency!!!");
    }
    for (l_idxF = 0; l_idxF < MAX_FRAME_NUM_IN_GOP; ++l_idxF) {
        for (l_idxH = 0; l_idxH < MAX_MB_H; ++l_idxH) {
            for (l_idxW = 0; l_idxW < MAX_MB_W; ++l_idxW) {
                for (l_curDepIdx = 0; l_curDepIdx < MAX_DEP_MB; ++l_curDepIdx) {
                    interDep[l_idxF][l_idxH][l_idxW][l_curDepIdx].h = -1;
                    interDep[l_idxF][l_idxH][l_idxW][l_curDepIdx].w = -1;
                }
            }
        }
    }
    while (fgets(aLine, 40, gVideoCodecCtxList[p_videoFileIndex]->g_interDepF) != NULL) {
        //get the frame number, mb position first
        if ((aToken = strtok(aLine, ":")) != NULL) 
            l_idxF = atoi(aToken);
        if (l_idxF < _stFrame) {
            continue;
        }
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxH = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxW = atoi(aToken);
        //get the dependency mb
        l_curDepIdx = 0;
        do {
            aToken = strtok(NULL, ":");
            if (aToken != NULL)  l_depH = atoi(aToken);
            else break;
            aToken = strtok(NULL, ":");
            if (aToken != NULL)  l_depW = atoi(aToken);
            else break;
            //put the dependencies into the array
            if ((l_idxH < MAX_MB_H) && (l_idxW < MAX_MB_W)) {
                interDep[l_idxF - _stFrame][l_idxH][l_idxW][l_curDepIdx].h = l_depH;
                interDep[l_idxF - _stFrame][l_idxH][l_idxW][l_curDepIdx++].w = l_depW;
            } else {
                LOGI(1, "*******Error***********************************************");
            }
        } while (aToken != NULL);
	if (l_idxF == _edFrame) {
	    break;
	}
    }
}

static void load_frame_dc_pred_direction(int p_videoFileIndex, int _frameNum, int _height, int _width) {
    int l_i, l_j, l_idxF, l_idxH, l_idxW, l_idxDir;
    char aLine[40], *aToken;
    LOGI(10, "load_frame_dc_pred_direction\n");
    //g_dcPredF = fopen("/sdcard/r10videocam/dcp.txt", "r");
    if (gVideoCodecCtxList[p_videoFileIndex]->g_dcPredF==NULL) {
        LOGI(1, "no valid dc pred!!!");
    }
    for (l_i = 0; l_i < _height; ++l_i) {
        for (l_j = 0; l_j < _width; ++l_j) {
            gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_i][l_j] = 0;
        }
    }
    while (fgets(aLine, 40, gVideoCodecCtxList[p_videoFileIndex]->g_dcPredF) != NULL) {
        //get the frame number, mb position first
        if ((aToken = strtok(aLine, ":")) != NULL) 
            l_idxF = atoi(aToken);
        if (l_idxF < _frameNum) {
            continue;
        } else if (l_idxF > _frameNum) {
	    //continue to parse the string to get length
	    l_i = strlen(aToken) + 1;
	    aToken = strtok(NULL, "\n");
	    l_i += strlen(aToken) + 1;
	    //go back to the previous line
	    fseek(gVideoCodecCtxList[p_videoFileIndex]->g_dcPredF, -l_i, SEEK_CUR);
	    //LOGI(10, "go back: %d", l_i);
	    break;
	}
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxH = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxW = atoi(aToken);
        if ((aToken = strtok(NULL, ":")) != NULL)
            l_idxDir = atoi(aToken);
        //get the dependency mb
        gVideoCodecCtxList[p_videoFileIndex]->pred_dc_dir[l_idxH][l_idxW] = l_idxDir;
    }
    LOGI(10, "load_frame_dc_pred_direction done\n");
}

/*done on a GOP basis*/
static void load_pre_computation_result(int p_videoFileIndex, int _stFrame, int _edFrame) {
    load_frame_mb_index(p_videoFileIndex, _stFrame, _edFrame);              //the mb index position
    load_intra_frame_mb_dependency(p_videoFileIndex, _stFrame, _edFrame);   //the intra-frame dependency
    load_inter_frame_mb_dependency(p_videoFileIndex, _stFrame, _edFrame);   //the inter-frame dependency
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
    for (y = 0; y < gVideoPicture.height; ++y) {
        for (k = 0; k < gVideoPicture.width; ++k) {
            fwrite(gVideoPicture.data.data[0] + y * gVideoPicture.data.linesize[0] + k*4, 1, 3, l_pFile);
        }
    }
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

static void compute_mb_mask_from_intra_frame_dependency_for_single_mb(int p_videoFileIndex, int _stFrame, int _frameNum, struct MBIdx _Pmb) {
    struct Queue l_q;
    struct MBIdx l_mb;
    int l_i;

    initQueue(&l_q);
    enqueue(&l_q, _Pmb);
    while (ifEmpty(&l_q) == 0) {
        //get the front value
        l_mb = front(&l_q);
        //mark the corresponding position in the mask
        if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb.h][l_mb.w] > 1) {
	    //it has been tracked before
            dequeue(&l_q);
            continue;
        }
        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_mb.h][l_mb.w]++;
        for (l_i = 0; l_i < MAX_DEP_MB; ++l_i) {
            if (intraDep[_frameNum - _stFrame][l_mb.h][l_mb.w][l_i].h == -1)
                break;
            enqueue(&l_q, intraDep[_frameNum - _stFrame][l_mb.h][l_mb.w][l_i]);
        }
        dequeue(&l_q);
    }
}

/*based on the start pos (_stH, _stW) and end pos (_edH, _edW), compute the mb needed to decode the roi due to inter-frame dependency
for I-frame: intra-frame dependency are due to differential encoding of DC and AC coefficients
for P-frame: intra-frame dependency are due to differential encoding of motion vectors*/
static void compute_mb_mask_from_intra_frame_dependency(int p_videoFileIndex, int _stFrame, int _frameNum, int _height, int _width) {
   int l_i, l_j;
   struct MBIdx l_mb;
   for (l_i = 0; l_i < _height; ++l_i) {
       for (l_j = 0; l_j < _width; ++l_j) {
           //dependency list traversing for a block
           //e.g. mb A has two dependencies mb B and C. We track down to B and C, mark them as needed, then do the same for B and C as we did for A.
           //basically a tree traversal problem.
           if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] == 1) {
               l_mb.h = l_i;
               l_mb.w = l_j;
               compute_mb_mask_from_intra_frame_dependency_for_single_mb(p_videoFileIndex, _stFrame, _frameNum, l_mb);
           }
       }
   } 
}

/*starting from the last frame of the GOP, calculate the inter-dependency backwards 
if the calculation is forward, then the case below might occur:
mb 3 in frame 3 depends on mb 2 on frame 2, but mb 2 is not decoded
if we know the roi for the entire GOP, we can pre-calculate the needed mbs at every frame*/
//TODO: the inter dependency list contains some negative values, we haven't figured it out yet
static void compute_mb_mask_from_inter_frame_dependency(int _stFrame, int _edFrame, int _stH, int _stW, int _edH, int _edW) {
    int l_i, l_j, l_k, l_m;
    LOGI(10, "start of compute_mb_mask_from_inter_frame_dependency");
    for (l_i = 0; l_i < MAX_FRAME_NUM_IN_GOP; ++l_i) {
        for (l_j = 0; l_j < MAX_MB_H; ++l_j) {
            for (l_k = 0; l_k < MAX_MB_W; ++l_k) {
                interDepMask[l_i][l_j][l_k] = 0;
            }
        }
    }
    //from last frame in the GOP, going backwards to the first frame of the GOP
    //1. mark the roi as needed
    for (l_i = _edFrame; l_i >= _stFrame; --l_i) {
        for (l_j = _stH; l_j <= _edH; ++l_j) {
            for (l_k = _stW; l_k <= _edW; ++l_k) {
                interDepMask[l_i - _stFrame][l_j][l_k] = 1;
            }
        }
    }
    //2. based on inter-dependency list, mark the needed mb
    //TODO: it's not necessary to process _stFrame, as there's no inter-dependency for it
    for (l_i = _edFrame; l_i >=  _stFrame; --l_i) {
        for (l_j = 0; l_j <= MAX_MB_H; ++l_j) {
            for (l_k = 0; l_k <= MAX_MB_W; ++l_k) {
                if (interDepMask[l_i - _stFrame][l_j][l_k] == 1) {
                    for (l_m = 0; l_m < MAX_DEP_MB; ++l_m) {
                        //mark the needed mb in the previous frame
                        if ((interDep[l_i - _stFrame][l_j][l_k][l_m].h < 0) || (interDep[l_i - _stFrame][l_j][l_k][l_m].w < 0))
                            continue;
                        LOGI(20, "%d,%d,%d,%d,%d,%d\n", l_i, l_j, l_k, l_m, interDep[l_i - _stFrame][l_j][l_k][l_m].h, interDep[l_i - _stFrame][l_j][l_k][l_m].w);
                        interDepMask[l_i - 1 - _stFrame][interDep[l_i - _stFrame][l_j][l_k][l_m].h][interDep[l_i - _stFrame][l_j][l_k][l_m].w] = 1;
                    }
                }
            }
        }
    }
    LOGI(10, "end of compute_mb_mask_from_inter_frame_dependency");
}

/*called by decoding thread, to check if needs to wait for dumping thread to dump dependency*/
/*or called by dumping thread, see if it needs to generate dependency files for this gop*/
/*return 1 if it's complete; otherwise, return 0*/
int if_dependency_complete(int p_videoFileIndex, int p_gopNum) {
	char l_depGopRecFileName[100], l_depIntraFileName[100], l_depInterFileName[100], l_depMbPosFileName[100], l_depDcpFileName[100];
	FILE* lGopF;
	int ret, ti, tj;
	/*check if the dependency files exist, if not, we'll need to dump the dependencies*/
#ifdef ANDROID_BUILD
	sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depIntraFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depInterFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depMbPosFileName, "%s_mbpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depDcpFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);  
#else
	sprintf(l_depGopRecFileName, "./%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depIntraFileName, "./%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depInterFileName, "./%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depMbPosFileName, "./%s_mbpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);
	sprintf(l_depDcpFileName, "./%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], p_gopNum);  
#endif
	if ((!if_file_exists(l_depGopRecFileName)) || (!if_file_exists(l_depIntraFileName)) || (!if_file_exists(l_depInterFileName)) || (!if_file_exists(l_depMbPosFileName)) || (!if_file_exists(l_depDcpFileName))) {
		return 0;
	} else {
		//further check if the gop file contains both start frame and end frame
		lGopF = fopen(l_depGopRecFileName, "r");
		ret = load_gop_info(lGopF, &ti, &tj);
		fclose(lGopF);
		return ((ret == 0)?1:0);
	}
}

void dep_decode_a_video_packet(int p_videoFileIndex) {
	char l_depGopRecFileName[100], l_depIntraFileName[100], l_depInterFileName[100], l_depMbPosFileName[100], l_depDcpFileName[100];
    AVFrame *l_videoFrame = avcodec_alloc_frame();
    int l_numOfDecodedFrames, l_frameType;
	int ti, tj;
    LOGI(10, "dep_decode_a_video_packet for video: %d", p_videoFileIndex);
    while (av_read_frame(gFormatCtxDepList[p_videoFileIndex], &gVideoPacketDepList[p_videoFileIndex]) >= 0) {
		if (gVideoPacketDepList[p_videoFileIndex].stream_index == gVideoStreamIndexList[p_videoFileIndex]) {
			LOGI(10, "got a video packet, dump dependency: %d", p_videoFileIndex);	
		    ++gVideoCodecCtxDepList[p_videoFileIndex]->dep_video_packet_num;
			/*put the video packet into the packet queue, for the decoding thread to access*/
			LOGI(10, "put a video packet into queue: %d", p_videoFileIndex);
			packet_queue_put(&gVideoPacketQueueList[p_videoFileIndex], &gVideoPacketDepList[p_videoFileIndex]);
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
						//TODO: fflush all the dependency files for previous gop, may not be necessary since we're closing these files
				        fflush(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF);
				        fflush(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbPosF);
				        fflush(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF);
				        fflush(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF);
				        fflush(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF);
						//close all dependency files for this GOP
						fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF);
				        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_mbPosF);
				        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF);
				        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF);
				        fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF);
					}
					++gVideoPacketQueueList[p_videoFileIndex].dep_gop_num;
				}
				/*check if the dependency files exist, if not, we'll need to dump the dependencies*/
				LOGI(10, "dependency files for video %d gop %d", p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num); 
#ifdef ANDROID_BUILD
				sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depIntraFileName, "%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depInterFileName, "%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depMbPosFileName, "%s_mbpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depDcpFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);

#else 
    			sprintf(l_depGopRecFileName, "./%s_goprec_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depIntraFileName, "./%s_intra_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depInterFileName, "./%s_inter_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depMbPosFileName, "./%s_mbpos_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
    			sprintf(l_depDcpFileName, "./%s_dcp_gop%d.txt", gVideoFileNameList[p_videoFileIndex], gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
#endif
				LOGI(10, "dependency files %s, %s, %s, %s, %s for video %d gop %d", l_depGopRecFileName, l_depIntraFileName, l_depInterFileName, l_depMbPosFileName, l_depDcpFileName, p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);  
#ifdef CLEAR_DEP_BEFORE_START
				remove(l_depGopRecFileName);
				remove(l_depIntraFileName);
				remove(l_depInterFileName);
				remove(l_depMbPosFileName);
				remove(l_depDcpFileName);
#endif  
				gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency = 1;
				if ((if_file_exists(l_depGopRecFileName)) && (if_file_exists(l_depIntraFileName)) && (if_file_exists(l_depInterFileName)) && (if_file_exists(l_depMbPosFileName)) && (if_file_exists(l_depDcpFileName))) {
					//if all files exist, further check l_depGopRecFileName file content, see if it actually contains both GOP start and end frame
					gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF = fopen(l_depGopRecFileName, "r");					
					if (load_gop_info(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF, &ti, &tj) != 0) {
						//the file content is complete, don't dump dependency
						LOGI(10, "dependency info is complete for video %d, gop %d", p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
						gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency = 0;
					}
					fclose(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF);
				} 
				if (gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency) {
					LOGI(10, "dumping dependency starts from video %d, gop %d", p_videoFileIndex, gVideoPacketQueueList[p_videoFileIndex].dep_gop_num);
					gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF = fopen(l_depGopRecFileName, "w");
					if (gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF == NULL) {
						LOGI(10, "cannot open gop file to write");
					}
					gVideoCodecCtxDepList[p_videoFileIndex]->g_mbPosF = fopen(l_depMbPosFileName, "w");
					if (gVideoCodecCtxDepList[p_videoFileIndex]->g_mbPosF == NULL) {
						LOGI(10, "cannot open mb pos file to write");
					}
					gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF = fopen(l_depDcpFileName, "w");
					if (gVideoCodecCtxDepList[p_videoFileIndex]->g_dcPredF == NULL) {
						LOGI(10, "cannot open dc prediction file to write");
					}
					gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF = fopen(l_depIntraFileName, "w");
					if (gVideoCodecCtxDepList[p_videoFileIndex]->g_intraDepF == NULL) {
						LOGI(10, "cannot open intra frame dependency file to write");
					}
					gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF = fopen(l_depInterFileName, "w");
					if (gVideoCodecCtxDepList[p_videoFileIndex]->g_interDepF == NULL) {
						LOGI(10, "cannot open inter frame dependency file to write");
					}
					//dump the start frame number for the new gop
					fprintf(gVideoCodecCtxDepList[p_videoFileIndex]->g_gopF, "%d:", gVideoCodecCtxDepList[p_videoFileIndex]->dep_video_packet_num);	
				} 
		    } 
			/*dump the dependency info*/
			if (gVideoCodecCtxDepList[p_videoFileIndex]->dump_dependency) {
				avcodec_decode_video2_dep(gVideoCodecCtxDepList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacketDepList[p_videoFileIndex]);
			}
			//av_free_packet(&gVideoPacketDepList[p_videoFileIndex]);
			break;
		} else {
			//it's not a video packet
			//LOGI(10, "%d != %d: it's not a video packet, continue reading!", gVideoPacketDep.stream_index, gVideoStreamIndex);
		    av_free_packet(&gVideoPacketDepList[p_videoFileIndex]);
		}
    }
    av_free(l_videoFrame);
}

void decode_a_video_packet(int p_videoFileIndex, int _roiStH, int _roiStW, int _roiEdH, int _roiEdW) {
    AVFrame *l_videoFrame = avcodec_alloc_frame();
    int l_numOfDecodedFrames;
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
        if (lGetPacketStatus < 0) {
	    	LOGI(10, "cannot get a video packet");
	    break;
	}
        if (gVideoPacket.stream_index == gVideoStreamIndexList[p_videoFileIndex]) {
	    //it's a video packet
	    LOGI(10, "got a video packet, decode it");
#ifdef SELECTIVE_DECODING
            l_mbHeight = (gVideoCodecCtxList[p_videoFileIndex]->height + 15) / 16;
            l_mbWidth = (gVideoCodecCtxList[p_videoFileIndex]->width + 15) / 16;
            LOGI(10, "selective decoding enabled: %d, %d", l_mbHeight, l_mbWidth);
            /*initialize the mask to all mb unselected*/
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 0;
                }
            }
            LOGI(10, "selected_mb_mask reseted");
            //add the needed mb mask based on inter-dependency, which is pre-computed before start decoding a gop
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    if (interDepMask[gVideoPacketNum - gStFrame][l_i][l_j] == 1) {
                        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 1;
                    }
                }
            }
#ifdef DUMP_SELECTIVE_DEP
	    FILE *l_interF;
	    char l_interFName[50];
#ifdef ANDROID_BUILD
		sprintf(l_interFName, "/sdcard/r10videocam/%d/inter.txt", gVideoPacketNum);
#else
	    sprintf(l_interFName, "./%d/inter.txt", gVideoPacketNum);
#endif
	    LOGI(10, "filename: %s", l_interFName);
	    l_interF = fopen(l_interFName, "w");
	    if (l_interF != NULL) {
	        for (l_i = 0; l_i < l_mbHeight; ++l_i) {
	            for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		        fprintf(l_interF, "%d:", gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]);
		    }
	 	    fprintf(l_interF, "\n");
	        }
	        fclose(l_interF);
	    } else {
		LOGE(1, "cannot open l_interFName");
	    }
#endif
            LOGI(10, "inter frame dependency counted");
            //compute the needed mb mask based on intra-dependency
            //mark all the mb in ROI as needed first
            for (l_i = _roiStH; l_i < _roiEdH; ++l_i) {
                for (l_j = _roiStW; l_j < _roiEdW; ++l_j) {
                    gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 1;
                }
            }
 	    //load the dc prediction direction
            load_frame_dc_pred_direction(p_videoFileIndex, gVideoPacketNum, l_mbHeight, l_mbWidth);
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
#endif
            //mark all mb needed due to intra-frame dependency
            compute_mb_mask_from_intra_frame_dependency(p_videoFileIndex, gStFrame, gVideoPacketNum, l_mbHeight, l_mbWidth); 
            //if a mb is selected multiple times, set it to 1
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		    if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] > 1) {
                        gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] = 1;
                    }
                }
            }
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
		    fprintf(l_intraF, "%d:", gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j]);
	        }
	 	fprintf(l_intraF, "\n");
	    }
	    fclose(l_intraF);
#endif
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
#endif
            //based on the mask, compose the video packet
            l_selectiveDecodingDataSize = 0;
            l_selectiveDecodingDataSize += mbStartPos[gVideoPacketNum - gStFrame][0][0];
            //get the size for needed mbs
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] == 1) {
			//LOGI(10, "%d:%d", mbEndPos[gVideoPacketNum - gStFrame][l_i][l_j], mbStartPos[gVideoPacketNum - gStFrame][l_i][l_j]);
                        l_selectiveDecodingDataSize += (mbEndPos[gVideoPacketNum - gStFrame][l_i][l_j] - mbStartPos[gVideoPacketNum - gStFrame][l_i][l_j]);
                    }
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
            l_bufPos = copy_bits(gVideoPacket.data, gVideoPacket2.data, 0, mbStartPos[gVideoPacketNum - gStFrame][0][0], l_bufPos);
            LOGI(10, "%d bits for header: video packet: %d; start frame: %d", mbStartPos[gVideoPacketNum - gStFrame][0][0], gVideoPacketNum, gStFrame);
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    //put the data bits into the composed video packet
                    if (gVideoCodecCtxList[p_videoFileIndex]->selected_mb_mask[l_i][l_j] == 1) {
                        l_bufPos = copy_bits(gVideoPacket.data, gVideoPacket2.data, mbStartPos[gVideoPacketNum - gStFrame][l_i][l_j],(mbEndPos[gVideoPacketNum - gStFrame][l_i][l_j] - mbStartPos[gVideoPacketNum - gStFrame][l_i][l_j]), l_bufPos);
                    }
                }
            }
            //stuffing the last byte
            for (l_i = 0; l_i < l_numOfStuffingBits; ++l_i) {
                gVideoPacket2.data[l_selectiveDecodingDataSize - 1] |= (0x01 << l_i);
            }
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
            LOGI(10, "avcodec_decode_video2");
            avcodec_decode_video2(gVideoCodecCtxList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket2);
#else
   #ifdef DUMP_VIDEO_FRAME_BYTES
	    sprintf(l_dumpPacketFileName, "debug_packet_dump_%d_full.txt", gVideoPacketNum);
	    l_packetDumpF = fopen(l_dumpPacketFileName, "wb");
	    fwrite(gVideoPacket.data, 1, gVideoPacket.size, l_packetDumpF);
	    fclose(l_packetDumpF);
    #endif
            avcodec_decode_video2(gVideoCodecCtxList[p_videoFileIndex], l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket);
#endif
		LOGI(10, "avcodec_decode_video2 result: %d", l_numOfDecodedFrames);
	    if (l_numOfDecodedFrames) {
		   LOGI(10, "video packet decoded, start conversion, allocate a picture (%d, %d)", gVideoPicture.width, gVideoPicture.height);
		   //allocate the memory space for a new VideoPicture
		   avpicture_alloc(&gVideoPicture.data, PIX_FMT_RGBA, gVideoPicture.width, gVideoPicture.height);
		   //convert the frame to RGB format
		   LOGI(10, "video picture data allocated, try to get a sws context: %d, %d", gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height);
		   gImgConvertCtx = sws_getCachedContext(gImgConvertCtx, gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height, gVideoCodecCtxList[p_videoFileIndex]->pix_fmt, gVideoPicture.width, gVideoPicture.height, PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);           
		   if (gImgConvertCtx == NULL) {
		       LOGE(1, "Error initialize the video frame conversion context");
		   }
		   LOGI(10, "got sws context, try to scale the video frame: from (%d, %d) to (%d, %d)", gVideoCodecCtxList[p_videoFileIndex]->width, gVideoCodecCtxList[p_videoFileIndex]->height, gVideoPicture.width, gVideoPicture.height);
		   sws_scale(gImgConvertCtx, l_videoFrame->data, l_videoFrame->linesize, 0, gVideoCodecCtxList[p_videoFileIndex]->height, gVideoPicture.data.data, gVideoPicture.data.linesize);
		   LOGI(10, "video packet conversion done, start free memory");
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
}

/*load the gop information, return 0 if everything goes well; otherwise, return a non-zero value*/
int load_gop_info(FILE* p_gopRecFile, int *p_startF, int *p_endF) {
    char l_gopRecLine[50];
    char *l_aToken;
    *p_startF = 0;
	*p_endF = 0;
	if (fgets(l_gopRecLine, 50, p_gopRecFile) == NULL)
			return -1;
    if ((l_aToken = strtok(l_gopRecLine, ":")) != NULL) 
        *p_startF = atoi(l_aToken);
	else
    	return -1;
    if ((l_aToken = strtok(NULL, ":")) != NULL) 
        *p_endF = atoi(l_aToken);
    else
    	return -1;
    LOGI(10, "load gop info ends: %d-%d", *p_startF, *p_endF);
	return 0;
}

/*load the pre computation for a gop and also compute the inter frame dependency for a gop*/
void prepare_decode_of_gop(int p_videoFileIndex, int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw) {
    LOGI(10, "prepare decode of gop started: %d, %d, %d, %d", _roiSh, _roiSw, _roiEh, _roiEw);
    gRoiSh = _roiSh;
    gRoiSw = _roiSw;
    gRoiEh = _roiEh;
    gRoiEw = _roiEw;
    load_pre_computation_result(p_videoFileIndex, _stFrame, _edFrame);
    compute_mb_mask_from_inter_frame_dependency(_stFrame, _edFrame, _roiSh, _roiSw, _roiEh, _roiEw);
    LOGI(10, "prepare decode of gop ended");
}

