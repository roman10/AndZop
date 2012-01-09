/**
this is the wrapper of the native functions that provided for AndZop
it also glues the decoders
current implementation only works for video without sound
**/
/**
TODO: 1. need to clear the frame buffer before decoding next frame, otherwise, the previous block might affect current block
2. the decoding decides the dependency on the fly based on candicate mbs, as some of these candidate mbs are not used, the decision making process is affected, and we need to correct this
2.1 for I-frame, it's already done.
2.2 check it for P-frame, especially the motion vector differential decoding
3. need to establish a criteria to decide whether the decoding is done correctly based on logs
3.1 for each mb decoded, compare the coefficients [done]
3.2 for each p-frame mb, compare the motion vector [done]
3.3 for each mb decoded, compare the dependencies 
**/

/**
[WARNING]: we keep two file pointers to each file, one for read and one for write. If the file content is updated by the
write pointer, whether the file pointer for reading will be displaced???
**/


/**
[TODO]: currently the dependency relationship are dumped to files first, then the decoding thread read from the file.
it's better we put the relationship into some data structure and let the decoding thread to read it directly from memory.
This doesn't apply to dcp as it's part of the avcodecContext
**/
/*android specific headers*/
#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
/*standard library*/
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>

/*multi-thread support*/
#include <pthread.h>

#include "dependency.h"

#ifndef ANDROID_BUILD
/*these two lines are necessary for compilation*/
const char program_name[] = "FFplay";
const int program_birth_year = 2003;
#endif

//should be larger for release build, set 100 for debug build
#define NUM_OF_FRAMES_TO_DECODE 10000

pthread_t gVideoDecodeThread;
#ifdef PRE_LOAD_DEP
pthread_t gPreloadThread;
pthread_mutex_t preloadMutex;
pthread_cond_t preloadCondVar;
#endif

#ifdef BG_DUMP_THREAD
pthread_t *gDepDumpThreadList;
typedef struct {
	int videoFileIndex;
} DUMP_DEP_PARAMS;
DUMP_DEP_PARAMS *gDumpThreadParams;
#endif

int gZoomLevelUpdate;

void *decode_video(void *arg);

static void wait_get_dependency() {
    /*wait for the dump dependency thread to finish dumping dependency info first before start decoding a frame*/
    while (g_decode_gop_num >= gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num) {
        /*[TODO]it might be more appropriate to use some sort of signal*/
        LOGI(10, ".......waiting for dependency for video %d, on gop %d, decode gop %d", gCurrentDecodingVideoFileIndex, gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num, g_decode_gop_num);
        usleep(1000);    
    }
    LOGI(10, "ready to decode gop %d:%d", g_decode_gop_num, gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num);    
}

#ifdef BG_DUMP_THREAD
static void *dump_dependency_function(void *arg) {
    int l_i;
    DUMP_DEP_PARAMS *l_params = (DUMP_DEP_PARAMS*)arg;
    /*TODO: figure out a way to avoid the looping for 500000*/
    for (l_i = 0; l_i < NUM_OF_FRAMES_TO_DECODE; ++l_i) {
        LOGI(10, "dump dependency for video %d packet %d", l_params->videoFileIndex, l_i);
        dep_decode_a_video_packet(l_params->videoFileIndex);
    }
    //print the last frame to the gop file, then close all dependency files
    fprintf(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_gopF, "%d:\n", gVideoCodecCtxDepList[l_params->videoFileIndex]->dep_video_packet_num);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_mbStPosF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_mbEdPosF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_intraDepF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_interDepF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_dcPredF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_gopF);
    avcodec_close(gVideoCodecCtxDepList[l_params->videoFileIndex]);	
    av_close_input_file(gFormatCtxDepList[l_params->videoFileIndex]);
}
#endif

#ifdef PRE_LOAD_DEP
//this is the function for preload thread. It should execute on the following two conditions
//1. at initial set up, preload [changed, should be called directly from naInit]
//2. at starting of decoding of a new GOP, preload
static void *preload_dependency_function(void *arg) {
    pthread_mutex_lock(&preloadMutex);
    pthread_cond_wait(&preloadCondVar, &preloadMutex);
    preload_pre_computation_result(gCurrentDecodingVideoFileIndex, g_decode_gop_num + 1);
    pthread_mutex_unlock(&preloadMutex);
}
#endif

//contains the init code for both android and linux
static void andzop_init(int pDebug) {
    int l_i;
    int l_mbH, l_mbW;
    gCurrentDecodingVideoFileIndex = 0;
    get_video_info(pDebug);
    gVideoPacketNum = 0;
#ifdef SELECTIVE_DECODING
    for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
        LOGI(10, "allocate_selected_decoding_fields for %d, current video index %d", l_i, gCurrentDecodingVideoFileIndex);
        l_mbH = (gVideoCodecCtxList[l_i]->height + 15) / 16;
        l_mbW = (gVideoCodecCtxList[l_i]->width + 15) / 16;
        allocate_selected_decoding_fields(l_i, l_mbH, l_mbW);
    }
#ifdef BG_DUMP_THREAD
    LOGI(10, "initialize dumping threads, current video index %d", gCurrentDecodingVideoFileIndex);
    gDepDumpThreadList = (pthread_t*)malloc(gNumOfVideoFiles *sizeof(pthread_t));
    gDumpThreadParams = (DUMP_DEP_PARAMS *)malloc(sizeof(DUMP_DEP_PARAMS)*gNumOfVideoFiles);
    for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
        //start a background thread for dependency dumping
        gDumpThreadParams[l_i].videoFileIndex = l_i;
        if (pthread_create(&gDepDumpThreadList[l_i], NULL, dump_dependency_function, (void *)&gDumpThreadParams[l_i])) {
	    LOGE(1, "Error: failed to create a native thread for dumping dependency");
        }
        LOGI(10, "tttttt: dependency dumping thread started! tttttt");
    }
#endif		//for BG_DUMP_THREAD
#ifdef PRE_LOAD_DEP
    //preload the first GOP at start up
    pthread_mutex_init(&preloadMutex, NULL);
    pthread_cond_init(&preloadCondVar, NULL);
    LOGI(10, "preload at initialization");
    preload_pre_computation_result(gCurrentDecodingVideoFileIndex, 1);
    LOGI(10, "preload at initialization done");
    LOGI(10, "initialize thread to preload dependencies");
    if (pthread_create(&gPreloadThread, NULL, preload_dependency_function, NULL)) {
        LOGE(1, "Error: failed to create a native thread for preloading dependency");
        exit(1);
    }
    LOGI(10, "preloading thread started!");
#endif		//for PRE_LOAD_DEP
#endif		//for SELECTIVE_DECODING
    LOGI(10, "initialization done, current video index %d", gCurrentDecodingVideoFileIndex);
}

static void andzop_finish() {
    int l_i;
    int l_mbH;
    LOGI(10, "andzop_finish is called");
    for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
	l_mbH = (gVideoCodecCtxList[l_i]->height + 15) / 16;
	/*close the video codec*/
	avcodec_close(gVideoCodecCtxList[l_i]);
	/*close the video file*/
	av_close_input_file(gFormatCtxList[l_i]);
#ifdef SELECTIVE_DECODING
	free_selected_decoding_fields(l_i, l_mbH);
#endif 
#if defined(SELECTIVE_DECODING) || defined(NORM_DECODE_DEBUG)
	/*close all dependency files*/
	fclose(gVideoCodecCtxList[l_i]->g_gopF);
#endif
    }
    free(gFormatCtxList);
    free(gFormatCtxDepList);
    free(gVideoStreamIndexList);
    free(gVideoCodecCtxDepList);
    free(gVideoCodecCtxList);
    free(gVideoPacketQueueList);
    LOGI(10, "clean up done");
}

static int decode_a_frame(int _width, int _height, float _roiSh, float _roiSw, float _roiEh, float _roiEw) {
    int li, lRet;
    int l_roiSh, l_roiSw, l_roiEh, l_roiEw;
    char l_depGopRecFileName[200], l_depIntraFileName[200], l_depInterFileName[200], l_depDcpFileName[200], l_depMbPosFileName[200];
    LOGI(10, "render_a_frame");
    gVideoPicture.height = _height;
    gVideoPicture.width = _width;
    ++gVideoPacketNum;  
#ifndef SELECTIVE_DECODING
    lRet = decode_a_video_packet(gCurrentDecodingVideoFileIndex, gRoiSh, gRoiSw, gRoiEh, gRoiEw);
#else
    /*see if it's a gop start, if so, load the gop info*/
    LOGI(10, "--------------gVideoPacketNum = %d;  = %d;", gVideoPacketNum, g_decode_gop_num);
    if (gVideoPacketNum == 1) {
	/*if it's first packet, we load the gop info*/
#ifdef BG_DUMP_THREAD
        wait_get_dependency();
#endif
#ifdef ANDROID_BUILD
        sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
#else
        sprintf(l_depGopRecFileName, "./%s_goprec_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
#endif
        gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_gopF = fopen(l_depGopRecFileName, "r");
        load_gop_info(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_gopF, &gGopStart, &gGopEnd);
    } 
    if (gVideoPacketNum == gGopStart) {
        LOGI(1, "---LD ST");
#ifdef PRE_LOAD_DEP
        pthread_cond_signal(&preloadCondVar);
#endif
        //start of a gop
        gStFrame = gGopStart;
        //enlarge or shrink the roi size according to the ratio of current video
        l_roiSh = (_roiSh)*gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height/_height;
        l_roiSw = (_roiSw)*gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width/_width;
        l_roiEh = (_roiEh)*gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height/_height;
        l_roiEw = (_roiEw)*gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width/_width;
        LOGI(2, "projected roi: (%d, %d) (%d, %d)", l_roiSh, l_roiSw, l_roiEh, l_roiEw);
        //based on roi pixel coordinates, calculate the mb-based roi coordinates
        l_roiSh = (l_roiSh - 15) > 0 ? (l_roiSh - 15):0;
        l_roiSw = (l_roiSw - 15) > 0 ? (l_roiSw - 15):0;
        l_roiEh = (l_roiEh + 15) < gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height ? (l_roiEh + 15):gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
        l_roiEw = (l_roiEw + 15) < gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width ? (l_roiEw + 15):gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
        LOGI(2, "corrected roi: (%d, %d) (%d, %d)", l_roiSh, l_roiSw, l_roiEh, l_roiEw);
        l_roiSh = l_roiSh * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
        l_roiSw = l_roiSw * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
 	l_roiEh = l_roiEh * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
        l_roiEw = l_roiEw * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
        LOGI(2, "decode video %d (%d, %d) with roi (%d:%d) to (%d:%d)", gCurrentDecodingVideoFileIndex,gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height,  gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width, l_roiSh, l_roiSw, l_roiEh, l_roiEw);
        //open the dependency files for this gop
#ifdef ANDROID_BUILD
        sprintf(l_depIntraFileName, "%s_intra_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(l_depInterFileName, "%s_inter_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_mbStPosFileName, "%s_mbstpos_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_mbEdPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(l_depMbPosFileName, "%s_mbedpos_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_dcPredFileName, "%s_dcp_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);  	    
#else
        sprintf(l_depIntraFileName, "./%s_intra_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(l_depInterFileName, "./%s_inter_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_mbStPosFileName, "./%s_mbstpos_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_mbEdPosFileName, "./%s_mbedpos_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
        sprintf(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_dcPredFileName, "./%s_dcp_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);  	    
#endif
        //load the pre computation result and compute the inter frame dependency
        prepare_decode_of_gop(gCurrentDecodingVideoFileIndex, gGopStart, gGopEnd, l_roiSh, l_roiSw, l_roiEh, l_roiEw);
        LOGI(1, "---LD ED");	
    }  
    LOGI(10, "decode video %d frame %d", gCurrentDecodingVideoFileIndex, gVideoPacketNum);
    lRet = decode_a_video_packet(gCurrentDecodingVideoFileIndex, gRoiSh, gRoiSw, gRoiEh, gRoiEw);
    /*if the gop is done decoding*/
    LOGI(10, "_____________________%d: %d: %d: %d", gVideoPacketNum, gGopEnd, gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num, g_decode_gop_num);
    if (gVideoPacketNum == gGopEnd) {
        LOGI(10, "-------------------------%d--------------------------", g_decode_gop_num);
        ++g_decode_gop_num;		//increase the counter
        //close the dependency files 
        fclose(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_gopF);
	/*check if we need to update zoom level, note that we only update the zoom level at the end of GOP*/
        if (gZoomLevelUpdate != 0) {
            gCurrentDecodingVideoFileIndex += gZoomLevelUpdate;
            if (gCurrentDecodingVideoFileIndex >= gNumOfVideoFiles) {
                gCurrentDecodingVideoFileIndex = gNumOfVideoFiles - 1;
            } else if (gCurrentDecodingVideoFileIndex < 0) {
                gCurrentDecodingVideoFileIndex = 0;
            }
	    gZoomLevelUpdate = 0;
        }
#ifdef BG_DUMP_THREAD
        //read the gop info for next gop
        wait_get_dependency();
#endif
#ifdef ANDROID_BUILD
        sprintf(l_depGopRecFileName, "%s_goprec_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
#else
        sprintf(l_depGopRecFileName, "./%s_goprec_gop%d.txt", gVideoFileNameList[gCurrentDecodingVideoFileIndex], g_decode_gop_num);
#endif
        gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_gopF = fopen(l_depGopRecFileName, "r");
        //unmap the files
        LOGI(10, "unmap files");
        unload_frame_mb_stindex();
        unload_frame_mb_edindex();
        unload_frame_dc_pred_direction();
        unload_intra_frame_mb_dependency();
        LOGI(10, "unmap files done");
        if (load_gop_info(gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_gopF, &gGopStart, &gGopEnd) == -1) {
            LOGE(1, "load gop info error, exit");
            exit(1);
        }
    }
#endif
    return lRet;
}

#ifdef ANDROID_BUILD
    #include "android_specific.c"
#else
    #include "linux_specific.c"
#endif

