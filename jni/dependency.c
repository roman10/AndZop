#include "dependency.h"

/*parsing the video file, done by parse thread*/
void get_video_info(char *prFilename) {
    AVCodec *lVideoCodec;
    int lError;
    /*some global variables initialization*/
    LOGI(10, "get video info starts!");
    /*register the codec*/
    extern AVCodec ff_h263_decoder;
    avcodec_register(&ff_h263_decoder);
    extern AVCodec ff_h264_decoder;
    avcodec_register(&ff_h264_decoder);
    extern AVCodec ff_mpeg4_decoder;
    avcodec_register(&ff_mpeg4_decoder);
    extern AVCodec ff_mjpeg_decoder;
    avcodec_register(&ff_mjpeg_decoder);
    /*register parsers*/
    //extern AVCodecParser ff_h264_parser;
    //av_register_codec_parser(&ff_h264_parser);
    //extern AVCodecParser ff_mpeg4video_parser;
    //av_register_codec_parser(&ff_mpeg4video_parser);
    /*register demux*/
    extern AVInputFormat ff_mov_demuxer;
    av_register_input_format(&ff_mov_demuxer);
    //extern AVInputFormat ff_h264_demuxer;
    //av_register_input_format(&ff_h264_demuxer);
    /*register the protocol*/
    extern URLProtocol ff_file_protocol;
    av_register_protocol2(&ff_file_protocol, sizeof(ff_file_protocol));
    /*open the video file*/
    if ((lError = av_open_input_file(&gFormatCtx, gFileName, NULL, 0, NULL)) !=0 ) {
        LOGE(1, "Error open video file: %d", lError);
        return;	//open file failed
    }
    /*retrieve stream information*/
    if ((lError = av_find_stream_info(gFormatCtx)) < 0) {
        LOGE(1, "Error find stream information: %d", lError);
        return;
    } 
    /*find the video stream and its decoder*/
    gVideoStreamIndex = av_find_best_stream(gFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &lVideoCodec, 0);
    if (gVideoStreamIndex == AVERROR_STREAM_NOT_FOUND) {
        LOGE(1, "Error: cannot find a video stream");
        return;
    } else {
	LOGI(10, "video codec: %s", lVideoCodec->name);
    }
    if (gVideoStreamIndex == AVERROR_DECODER_NOT_FOUND) {
        LOGE(1, "Error: video stream found, but no decoder is found!");
        return;
    }   
    /*open the codec*/
    gVideoCodecCtx = gFormatCtx->streams[gVideoStreamIndex]->codec;
    LOGI(10, "open codec: (%d, %d)", gVideoCodecCtx->height, gVideoCodecCtx->width);
#ifdef SELECTIVE_DECODING
    gVideoCodecCtx->allow_selective_decoding = 1;
#endif
    if (avcodec_open(gVideoCodecCtx, lVideoCodec) < 0) {
	LOGE(1, "Error: cannot open the video codec!");
        return;
    }
    LOGI(10, "get video info ends");
}

void allocate_selected_decoding_fields(int _mbHeight, int _mbWidth) {
    int l_i;
    gVideoCodecCtx->selected_mb_mask = (unsigned char **) malloc(_mbHeight * sizeof(unsigned char *));
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        gVideoCodecCtx->selected_mb_mask[l_i] = (unsigned char *) malloc(_mbWidth * sizeof(unsigned char));
    }
    gVideoCodecCtx->pred_dc_dir = (unsigned char **) malloc(_mbHeight * sizeof(unsigned char *));
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        gVideoCodecCtx->pred_dc_dir[l_i] = (unsigned char *) malloc(_mbWidth * sizeof(unsigned char));
    }
}

void free_selected_decoding_fields(int _mbHeight) {
    int l_i;
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        free(gVideoCodecCtx->selected_mb_mask[l_i]);
    }
    free(gVideoCodecCtx->selected_mb_mask);
    for (l_i = 0; l_i < _mbHeight; ++l_i) {
        free(gVideoCodecCtx->pred_dc_dir[l_i]);
    }
    free(gVideoCodecCtx->pred_dc_dir);
}

int mbStartPos[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];
int mbEndPos[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];

struct MBIdx intraDep[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W][MAX_DEP_MB];
struct MBIdx interDep[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W][MAX_DEP_MB];
int interDepMask[MAX_FRAME_NUM_IN_GOP][MAX_MB_H][MAX_MB_W];

/*load frame mb index from frame _stFrame to frame _edFrame*/
static void load_frame_mb_index(int _stFrame, int _edFrame) {
    char aLine[30];
    char *aToken;
    int idxF, idxH, idxW, stP, edP;
    LOGI(10, "load_frame_mb_index\n");
    if (g_mbPosF == NULL) {
        LOGE(1, "Error: no valid mb index records!!!");
    }
    memset(mbStartPos, 0, MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
    memset(mbEndPos, 0, MAX_FRAME_NUM_IN_GOP*MAX_MB_H*MAX_MB_W);
    idxF = 0; idxH = 0; idxW = 0;
    while (fgets(aLine, 30, g_mbPosF) != NULL) {
        //parse the line
        if ((aToken = strtok(aLine, ":")) != NULL)
            idxF = atoi(aToken);
        if (idxF < _stFrame) {
            //not the start frame yet, continue reading
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
	if (idxF == _edFrame) {
	    break;
	}
    }
}

void load_intra_frame_mb_dependency(int _stFrame, int _edFrame) {
    char aLine[40], *aToken;
    int l_idxF, l_idxH, l_idxW, l_depH, l_depW, l_curDepIdx;
    LOGI(10, "load_intra_frame_mb_dependency\n");
    if (g_intraDepF == NULL) {
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
    while (fgets(aLine, 40, g_intraDepF) != NULL) {
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

void load_inter_frame_mb_dependency(int _stFrame, int _edFrame) {
    char aLine[40], *aToken;
    int l_idxF, l_idxH, l_idxW, l_depH, l_depW, l_curDepIdx;
    LOGI(10, "load_inter_frame_mb_dependency: %d: %d\n", _stFrame, _edFrame);
    if (g_interDepF == NULL) {
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
    while (fgets(aLine, 40, g_interDepF) != NULL) {
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

static void load_frame_dc_pred_direction(int _frameNum, int _height, int _width) {
    int l_i, l_j, l_idxF, l_idxH, l_idxW, l_idxDir;
    char aLine[40], *aToken;
    LOGI(10, "load_frame_dc_pred_direction\n");
    //g_dcPredF = fopen("/sdcard/r10videocam/dcp.txt", "r");
    if (g_dcPredF==NULL) {
        LOGI(1, "no valid dc pred!!!");
    }
    for (l_i = 0; l_i < _height; ++l_i) {
        for (l_j = 0; l_j < _width; ++l_j) {
            gVideoCodecCtx->pred_dc_dir[l_i][l_j] = 0;
        }
    }
    while (fgets(aLine, 40, g_dcPredF) != NULL) {
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
	    fseek(g_dcPredF, -l_i, SEEK_CUR);
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
        gVideoCodecCtx->pred_dc_dir[l_idxH][l_idxW] = l_idxDir;
    }
}

/*done on a GOP basis*/
static void load_pre_computation_result(int _stFrame, int _edFrame) {
    load_frame_mb_index(_stFrame, _edFrame);              //the mb index position
    load_intra_frame_mb_dependency(_stFrame, _edFrame);   //the intra-frame dependency
    load_inter_frame_mb_dependency(_stFrame, _edFrame);   //the inter-frame dependency
}

void dump_frame_to_file(int _frameNum) {
    FILE *l_pFile;
    char l_filename[32];
    int y, k;
    LOGI(10, "dump frame to file");
    //open file
    sprintf(l_filename, "/sdcard/r10videocam/frame_%d.ppm", _frameNum);
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
}

//copy count bits starting from startPos from data to buf starting at bufPos
static int copy_bits(unsigned char *data, unsigned char *buf, int startPos, int count, int bufPos) {
    unsigned char value;
    int length;
    int bitsCopied;
    int i;
    int numOfAlignedBytes;
    bitsCopied = 0;
    //LOGI("*****************startPos: %d; count: %d; bufPos: %d:\n", startPos, count, bufPos);
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

static void compute_mb_mask_from_intra_frame_dependency_for_single_mb(int _stFrame, int _frameNum, struct MBIdx _Pmb) {
    struct Queue l_q;
    struct MBIdx l_mb;
    int l_i, l_j;
    
    initQueue(&l_q);
    enqueue(&l_q, _Pmb);
    while (ifEmpty(&l_q) == 0) {
        //get the front value
        l_mb = front(&l_q);
        //mark the corresponding position in the mask
        if (gVideoCodecCtx->selected_mb_mask[l_mb.h][l_mb.w] > 1) {
	    //it has been tracked before
            dequeue(&l_q);
            continue;
        }
        //LOGI("$$$$$ %d %d $$$$$\n", l_mb.h, l_mb.w);
        gVideoCodecCtx->selected_mb_mask[l_mb.h][l_mb.w]++;
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
static void compute_mb_mask_from_intra_frame_dependency(int _stFrame, int _frameNum, int _height, int _width) {
   int l_i, l_j;
   struct MBIdx l_mb;
   for (l_i = 0; l_i < _height; ++l_i) {
       for (l_j = 0; l_j < _width; ++l_j) {
           //dependency list traversing for a block
           //e.g. mb A has two dependencies mb B and C. We track down to B and C, mark them as needed, then do the same for B and C as we did for A.
           //basically a tree traversal problem.
           if (gVideoCodecCtx->selected_mb_mask[l_i][l_j] == 1) {
               l_mb.h = l_i;
               l_mb.w = l_j;
               compute_mb_mask_from_intra_frame_dependency_for_single_mb(_stFrame, _frameNum, l_mb);
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

#define DUMP_PACKET_TYPE
FILE *packetTypeFile;
void decode_a_video_packet(int _roiStH, int _roiStW, int _roiEdH, int _roiEdW) {
    AVFrame *l_videoFrame = avcodec_alloc_frame();
    int l_ret;
    int l_numOfDecodedFrames;
    int l_i, l_j;
    int l_mbHeight, l_mbWidth;
    int l_selectiveDecodingDataSize;
    int l_numOfStuffingBits;
    int l_bufPos;
    unsigned char l_type;
    FILE *packetF, *logDep;
    char dumpPacketFileName[30];
    /*read the next video packet*/
    LOGI(10, "decode_a_video_packet");
    while (av_read_frame(gFormatCtx, &gVideoPacket) >= 0) {
        if (gVideoPacket.stream_index == gVideoStreamIndex) {
	    //it's a video packet
	    LOGI(10, "got a video packet, decode it");
#ifdef SELECTIVE_DECODING
            LOGI(10, "selective decoding enabled!");
            l_mbHeight = (gVideoCodecCtx->height + 15) / 16;
            l_mbWidth = (gVideoCodecCtx->width + 15) / 16;
            /*initialize the mask to all mb unselected*/
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    gVideoCodecCtx->selected_mb_mask[l_i][l_j] = 0;
                }
            }
            LOGI(10, "selected_mb_mask reseted");
            //add the needed mb mask based on inter-dependency, which is pre-computed before start decoding a gop
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    if (interDepMask[gVideoPacketNum - gStFrame][l_i][l_j] == 1) {
                        gVideoCodecCtx->selected_mb_mask[l_i][l_j] = 1;
                    }
                }
            }
            LOGI(10, "inter frame dependency counted");
            //compute the needed mb mask based on intra-dependency
            //mark all the mb in ROI as needed first
            for (l_i = _roiStH; l_i < _roiEdH; ++l_i) {
                for (l_j = _roiStW; l_j < _roiEdW; ++l_j) {
                    gVideoCodecCtx->selected_mb_mask[l_i][l_j] = 1;
                }
            }
 	    //load the dc prediction direction
            load_frame_dc_pred_direction(gVideoPacketNum, l_mbHeight, l_mbWidth);
            //mark all mb needed due to intra-frame dependency
            compute_mb_mask_from_intra_frame_dependency(gStFrame, gVideoPacketNum, l_mbHeight, l_mbWidth); 
            //if a mb is selected multiple times, set it to 1
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
		    if (gVideoCodecCtx->selected_mb_mask[l_i][l_j] > 1) {
                        gVideoCodecCtx->selected_mb_mask[l_i][l_j] = 1;
                    }
                }
            }
            //based on the mask, compose the video packet
            l_selectiveDecodingDataSize = 0;
            l_selectiveDecodingDataSize += mbStartPos[gVideoPacketNum - gStFrame][0][0];
            //get the size for needed mbs
            for (l_i = 0; l_i < l_mbHeight; ++l_i) {
                for (l_j = 0; l_j < l_mbWidth; ++l_j) {
                    if (gVideoCodecCtx->selected_mb_mask[l_i][l_j] == 1) {
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
                    if (gVideoCodecCtx->selected_mb_mask[l_i][l_j] == 1) {
                        l_bufPos = copy_bits(gVideoPacket.data, gVideoPacket2.data, mbStartPos[gVideoPacketNum - gStFrame][l_i][l_j],(mbEndPos[gVideoPacketNum - gStFrame][l_i][l_j] - mbStartPos[gVideoPacketNum - gStFrame][l_i][l_j]), l_bufPos);
                    }
                }
            }
            //stuffing the last byte
            for (l_i = 0; l_i < l_numOfStuffingBits; ++l_i) {
                gVideoPacket2.data[l_selectiveDecodingDataSize - 1] |= (0x01 << l_i);
            }
            LOGI(10, "avcodec_decode_video2");
            avcodec_decode_video2(gVideoCodecCtx, l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket2);
#else
            avcodec_decode_video2(gVideoCodecCtx, l_videoFrame, &l_numOfDecodedFrames, &gVideoPacket);
#endif
	    if (l_numOfDecodedFrames) {
		   LOGI(10, "video packet decoded, start conversion, allocate a picture (%d, %d)", gVideoPicture.width, gVideoPicture.height);
		   //allocate the memory space for a new VideoPicture
		   avpicture_alloc(&gVideoPicture.data, PIX_FMT_RGBA, gVideoPicture.width, gVideoPicture.height);
		   //convert the frame to RGB format
		   LOGI(10, "video picture data allocated, try to get a sws context: %d, %d", gVideoCodecCtx->width, gVideoCodecCtx->height);
		   gImgConvertCtx = sws_getCachedContext(gImgConvertCtx, gVideoCodecCtx->width, gVideoCodecCtx->height, gVideoCodecCtx->pix_fmt, gVideoPicture.width, gVideoPicture.height, PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);           
		   if (gImgConvertCtx == NULL) {
		       LOGE(1, "Error initialize the video frame conversion context");
		   }
		   LOGI(10, "got sws context, try to scale the video frame: from (%d, %d) to (%d, %d)", gVideoCodecCtx->width, gVideoCodecCtx->height, gVideoPicture.width, gVideoPicture.height);
		   sws_scale(gImgConvertCtx, l_videoFrame->data, l_videoFrame->linesize, 0, gVideoCodecCtx->height, gVideoPicture.data.data, gVideoPicture.data.linesize);
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
	    LOGI(10, "it's not a video packet, continue reading!");
            av_free_packet(&gVideoPacket);
        }
    }
    av_free(l_videoFrame);
}

/*load the pre computation for a gop and also compute the inter frame dependency for a gop*/
void prepare_decode_of_gop(int _stFrame, int _edFrame, int _roiSh, int _roiSw, int _roiEh, int _roiEw) {
    LOGI(10, "prepare decode of gop started: %d, %d, %d, %d", _roiSh, _roiSw, _roiEh, _roiEw);
    gRoiSh = _roiSh;
    gRoiSw = _roiSw;
    gRoiEh = _roiEh;
    gRoiEw = _roiEw;
    load_pre_computation_result(_stFrame, _edFrame);
    compute_mb_mask_from_inter_frame_dependency(_stFrame, _edFrame, _roiSh, _roiSw, _roiEh, _roiEw);
    LOGI(10, "prepare decode of gop ended");
}

/*load the gop information*/
void load_gop_info() {
    FILE *l_gopRecF;
    char l_gopRecLine[50];
    char *l_aToken;
    int l_stFrame = 0, l_edFrame = 0;
    LOGI(10, "load gop info starts");
    l_gopRecF = fopen("/sdcard/r10videocam/goprec.txt", "r");
    if (l_gopRecF == NULL) {
        LOGI(10, "error opening goprec.txt");
        return;
    }
    gNumOfGop = 0;
    while (fgets(l_gopRecLine, 50, l_gopRecF) != NULL) {
        if ((l_aToken = strtok(l_gopRecLine, ":")) != NULL) 
            l_stFrame = atoi(l_aToken);
        if ((l_aToken = strtok(NULL, ":")) != NULL) 
            l_edFrame = atoi(l_aToken);
        gGopStart[gNumOfGop] = l_stFrame;
        gGopEnd[gNumOfGop] = l_edFrame;
        gNumOfGop += 1;
    }
    fclose(l_gopRecF);
    LOGI(10, "load gop info ends");
}
