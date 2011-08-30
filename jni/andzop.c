/**
this is the wrapper of the native functions that provided for AndZop
it also glues the decoders
single-thread version for simplicity
1. parse the video to retrieve video packets 
2. takes a packet decode the video and put them into a picture/frame queue

current implementation only works for video without sound
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

/**/
static int gsState;       //gs=>global static

pthread_t gDepDumpThread;

void *dump_dependency_function(void *arg);

void *dump_dependency_function(void *arg) {
    int l_i;
    /*TODO: figure out a way to avoid the looping for 500000*/
    for (l_i = 0; l_i < 500000; ++l_i) {
	LOGI(20, "dump dependency for video packet %d", l_i);
	dep_decode_a_video_packet();
    }
    fclose(gVideoCodecCtxDep->g_mbPosF);
    fclose(gVideoCodecCtxDep->g_intraDepF);
    fclose(gVideoCodecCtxDep->g_interDepF);
    fclose(gVideoCodecCtxDep->g_dcPredF);
    fclose(gVideoCodecCtxDep->g_gopF);
    avcodec_close(gVideoCodecCtxDep);	
    av_close_input_file(gFormatCtxDep);
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naClose(JNIEnv *pEnv, jobject pObj) {
    int l_mbH = (gVideoCodecCtx->height + 15) / 16;
#ifdef SELECTIVE_DECODING
    free_selected_decoding_fields(l_mbH);
#endif
    /*close the video codec*/
    avcodec_close(gVideoCodecCtx);
    /*close the video file*/
    av_close_input_file(gFormatCtx);
#if defined(SELECTIVE_DECODING) || defined(NORM_DECODE_DEBUG)
    /*close all dependency files*/
    fclose(gVideoCodecCtx->g_mbPosF);
    fclose(gVideoCodecCtx->g_intraDepF);
    fclose(gVideoCodecCtx->g_interDepF);
    fclose(gVideoCodecCtx->g_dcPredF);
    fclose(gVideoCodecCtx->g_gopF);
#endif
    LOGI(10, "clean up done");
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
    int l_mbH, l_mbW;
    char *l_videoFileName;
    /*get the video file name*/
    l_videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    if (l_videoFileName == NULL) {
        LOGE(1, "Error: cannot get the video file name!");
        return;
    } 
    LOGI(10, "video file name is %s", l_videoFileName);
    get_video_info(l_videoFileName, 0);			//at android version, we set debug to 0
    gVideoPacketNum = 0;
    gNumOfGop = 0;
#ifdef SELECTIVE_DECODING
    if (!gVideoCodecCtx->dump_dependency) {
	load_gop_info(gVideoCodecCtx->g_gopF);
    }
    l_mbH = (gVideoCodecCtx->height + 15) / 16;
    l_mbW = (gVideoCodecCtx->width + 15) / 16;
    allocate_selected_decoding_fields(l_mbH, l_mbW);
#endif
    if (gVideoCodecCtx->dump_dependency) {
	/*if we need to dump dependency, start a background thread for it*/
        if (pthread_create(&gDepDumpThread, NULL, dump_dependency_function, NULL)) {
	    LOGE(1, "Error: failed to create a native thread for dumping dependency");
        }
        LOGI(10, "tttttt: dependency dumping thread started! tttttt");
    }
    LOGI(10, "initialization done");
}

JNIEXPORT jstring JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoCodecName(JNIEnv *pEnv, jobject pObj) {
    char* lCodecName = gVideoCodecCtx->codec->name;
    return (*pEnv)->NewStringUTF(pEnv, lCodecName);
}

JNIEXPORT jstring JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoFormatName(JNIEnv *pEnv, jobject pObj) {
    char* lFormatName = gFormatCtx->iformat->name;
    return (*pEnv)->NewStringUTF(pEnv, lFormatName);
}


JNIEXPORT jintArray JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoResolution(JNIEnv *pEnv, jobject pObj) {
    jintArray lRes;
    lRes = (*pEnv)->NewIntArray(pEnv, 2);
    if (lRes == NULL) {
        LOGI(1, "cannot allocate memory for video size");
        return NULL;
    }
    jint lVideoRes[2];
    lVideoRes[0] = gVideoCodecCtx->width;
    lVideoRes[1] = gVideoCodecCtx->height;
    (*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
    return lRes;
}

/*get the actual roi*/
/*the actual roi may differ from the user requested roi, as the roi can only change at the beginning of gop*/
JNIEXPORT jfloatArray JNICALL Java_feipeng_andzop_render_RenderView_naGetActualRoi(JNIEnv *pEnv, jobject pObj) {
    jfloatArray lRes;
    lRes = (*pEnv)->NewFloatArray(pEnv, 4);
    if (lRes == NULL) {
        LOGI(1, "cannot allocate memory for video size");
        return NULL;
    }
    jfloat lVideoRes[4];
    lVideoRes[0] = gRoiSh*16;
    lVideoRes[1] = gRoiSw*16;
    lVideoRes[2] = gRoiEh*16;
    lVideoRes[3] = gRoiEw*16;
    (*pEnv)->SetFloatArrayRegion(pEnv, lRes, 0, 4, lVideoRes);
    return lRes;
}

/*fill in data for a bitmap*/
JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naRenderAFrame(JNIEnv * pEnv, jobject pObj, jobject pBitmap, int _width, int _height, float _roiSh, float _roiSw, float _roiEh, float _roiEw) {
    AndroidBitmapInfo lInfo;
    void* lPixels;
    int lRet;
    int li, lj, lk;
    int lPos;
    char* ltmp;
    int l_roiSh, l_roiSw, l_roiEh, l_roiEw;
    if (!gsState) {
        //[TODO]if not initialized, initialize 
    }
    //1. retrieve information about the bitmap
    if ((lRet = AndroidBitmap_getInfo(pEnv, pBitmap, &lInfo)) < 0) {
        LOGE(1, "AndroidBitmap_getInfo failed! error = %d", lRet);
        return;
    }
    if (lInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE(1, "Bitmap format is not RGBA_8888!");
        return;
    }
    //2. lock the pixel buffer and retrieve a pointer to it
    if ((lRet = AndroidBitmap_lockPixels(pEnv, pBitmap, &lPixels)) < 0) {
        LOGE(1, "AndroidBitmap_lockPixels() failed! error = %d", lRet);
    }
    //3. modify the pixel buffer
    //take a VideoPicture nd read the data into lPixels
    gVideoPicture.height = _height;
    gVideoPicture.width = _width;
    ++gVideoPacketNum;
    LOGI(10, "decode video frame %d", gVideoPacketNum);
    /*wait for the dump dependency thread to finish dumping dependency info first before start decoding a frame*/
    if (gVideoCodecCtx->dump_dependency) {
        while (gVideoPacketQueue.decode_gop_num >= gVideoPacketQueue.dep_gop_num) {
	    //pthread_mutex_lock(&gVideoPacketQueue.mutex);
            //pthread_cond_wait(&gVideoPacketQueue.cond, &gVideoPacketQueue.mutex);
	    //pthread_mutex_unlock(&gVideoPacketQueue.mutex);
	    usleep(50);    
        }
	LOGI(10, "%d:%d:%d", gNumOfGop, gVideoPacketQueue.decode_gop_num, gVideoPacketQueue.dep_gop_num);    
	if (gNumOfGop < gVideoPacketQueue.dep_gop_num) {
	    load_gop_info(gVideoCodecCtx->g_gopF);
	}
    }
    for (li = 0; li < gNumOfGop; ++li) {
        if (gVideoPacketNum == gGopStart[li]) {
            //start of a gop
            gStFrame = gGopStart[li];
	    //3.0 based on roi pixel coordinates, calculate the mb-based roi coordinates
	    l_roiSh = (_roiSh - 15) > 0 ? (_roiSh - 15):0;
	    l_roiSw = (_roiSw - 15) > 0 ? (_roiSw - 15):0;
	    l_roiEh = (_roiEh + 15) < gVideoCodecCtx->height ? (_roiEh + 15):gVideoCodecCtx->height;
	    l_roiEw = (_roiEw + 15) < gVideoCodecCtx->width ? (_roiEw + 15):gVideoCodecCtx->width;
	    l_roiSh = l_roiSh * (gVideoCodecCtx->height/16) / gVideoCodecCtx->height;
	    l_roiSw = l_roiSw * (gVideoCodecCtx->width/16) / gVideoCodecCtx->width;
 	    l_roiEh = l_roiEh * (gVideoCodecCtx->height/16) / gVideoCodecCtx->height;
	    l_roiEw = l_roiEw * (gVideoCodecCtx->width/16) / gVideoCodecCtx->width;
	    //3.1 check if it's a beginning of a gop, if so, load the pre computation result and compute the inter frame dependency
            prepare_decode_of_gop(gGopStart[li], gGopEnd[li], l_roiSh, l_roiSw, l_roiEh, l_roiEw);
            break;
        } else if (gVideoPacketNum < gGopEnd[li]) {
            break;
        }
    }
    //3.2 decode the video packet
    decode_a_video_packet(gRoiSh, gRoiSw, gRoiEh, gRoiEw);
    //dump_frame_to_file(gVideoPacketNum);
    LOGI(10, "start to fill in the bitmap pixels: h: %d, w: %d", gVideoPicture.height, gVideoPicture.width);
    LOGI(10, "line size: %d", gVideoPicture.data.linesize[0]);
    for (li = 0; li < gVideoPicture.height; ++li) {
        //copy the data to lPixels line by line
        for (lj = 0; lj < gVideoPicture.width; ++lj) {
            for (lk = 0; lk < 3; ++lk) {
		//LOGI(10, "%d-%d-%d", li, lj, lk);
                lPos = (li*gVideoPicture.width + lj)*4 + lk;
                ltmp = (char *) lPixels;
		//LOGI("%d", lPos);
                *ltmp = gVideoPicture.data.data[0][lPos];
		//LOGI("%d-%d-%d:%d", li, lj, lk, *ltmp);
	        lPixels++;
            }
	    lPixels++;	//for alpha, we don't copy it
        }
    }
    //4. unlock pixel buffer
    AndroidBitmap_unlockPixels(pEnv, pBitmap);
    //5. clear the allocated picture buffer
    avpicture_free(&gVideoPicture.data);
    LOGI(10, "~~~~~~~~~~end of rendering a frame~~~~~~~~~~~~~~~~~`");
}



