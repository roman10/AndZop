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

/**/
static int gsState;       //gs=>global static

pthread_t gVideoDecodeThread;
pthread_t *gDepDumpThreadList;
typedef struct {
	int videoFileIndex;
} DUMP_DEP_PARAMS;
DUMP_DEP_PARAMS *gDumpThreadParams;

int gZoomLevelUpdate;

void *test_thread(void *arg);
void *decode_video(void *arg);

void *dump_dependency_function(void *arg) {
    int l_i;
	DUMP_DEP_PARAMS *l_params = (DUMP_DEP_PARAMS*)arg;
    /*TODO: figure out a way to avoid the looping for 500000*/
    for (l_i = 0; l_i < 500000; ++l_i) {
		LOGI(10, "dump dependency for video %d packet %d", l_params->videoFileIndex, l_i);
		dep_decode_a_video_packet(l_params->videoFileIndex);
    }
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_mbPosF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_intraDepF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_interDepF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_dcPredF);
    fclose(gVideoCodecCtxDepList[l_params->videoFileIndex]->g_gopF);
    avcodec_close(gVideoCodecCtxDepList[l_params->videoFileIndex]);	
    av_close_input_file(gFormatCtxDepList[l_params->videoFileIndex]);
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naClose(JNIEnv *pEnv, jobject pObj) {
    int l_i;
    int l_mbH;
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
		fclose(gVideoCodecCtxList[l_i]->g_mbPosF);
		fclose(gVideoCodecCtxList[l_i]->g_intraDepF);
		fclose(gVideoCodecCtxList[l_i]->g_interDepF);
		fclose(gVideoCodecCtxList[l_i]->g_dcPredF);
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

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naInit(JNIEnv *pEnv, jobject pObj) {
    int l_mbH, l_mbW;
	int l_NumOfFile, l_i;
	char* l_videoFileNameList[10];
	jobjectArray l_videoFileNameObjList;
	jfieldID l_fid;
	jstring l_videoFileNameStr;
    /*get the video file name*/
    //l_videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    //if (l_videoFileName == NULL) {
    //    LOGE(1, "Error: cannot get the video file name!");
    //    return;
    //} 
    //LOGI(10, "video file name is %s", l_videoFileName);
	if (pObj == NULL) {
		LOGE(10, "pObj is NULL");
	}
	//LOGI(10, "naInit: %s", pObj);
	jclass cls = (*pEnv)->GetObjectClass(pEnv, pObj);
	//LOGI(10, "naInit: %d", cls);
	//l_fid = (*pEnv)->GetFieldID(pEnv, cls, "fileNameListTest", "Ljava/lang/String;"); 
	l_fid = (*pEnv)->GetFieldID(pEnv, cls, "fileNameList", "[Ljava/lang/String;"); 
	l_videoFileNameObjList = (*pEnv)->GetObjectField(pEnv, pObj, l_fid);
	l_NumOfFile = (*pEnv)->GetArrayLength(pEnv, l_videoFileNameObjList);
	LOGI(10, "number of video files: %d", l_NumOfFile);
	for (l_i = 0; l_i < l_NumOfFile; ++l_i) {
		l_videoFileNameStr = (*pEnv)->GetObjectArrayElement(pEnv, l_videoFileNameObjList, l_i);
		l_videoFileNameList[l_i] = (char *)(*pEnv)->GetStringUTFChars(pEnv, l_videoFileNameStr, NULL);
		LOGI(10, "%d video file name is %s", l_i, l_videoFileNameList[l_i]);
	}

    get_video_info(l_NumOfFile, l_videoFileNameList, 0);
    gVideoPacketNum = 0;
    gNumOfGop = 0;

#ifdef SELECTIVE_DECODING
	for (l_i = 0; l_i < l_NumOfFile; ++l_i) {
	    if (!gVideoCodecCtxList[l_i]->dump_dependency) {
			load_gop_info(l_i, gVideoCodecCtxList[l_i]->g_gopF);
	    }
	}
	for (l_i = 0; l_i < l_NumOfFile; ++l_i) {
		l_mbH = (gVideoCodecCtxList[l_i]->height + 15) / 16;
		l_mbW = (gVideoCodecCtxList[l_i]->width + 15) / 16;
		allocate_selected_decoding_fields(l_i, l_mbH, l_mbW);
	}
#endif
	gDepDumpThreadList = (pthread_t*)malloc((l_NumOfFile)*sizeof(pthread_t));
	gDumpThreadParams = (DUMP_DEP_PARAMS *)malloc(sizeof(DUMP_DEP_PARAMS)*(l_NumOfFile));
	for (l_i = 0; l_i < l_NumOfFile; ++l_i) {
		if (gVideoCodecCtxList[l_i]->dump_dependency) {
			/*if we need to dump dependency, start a background thread for it*/
		    gDumpThreadParams[l_i].videoFileIndex = l_i;
			if (pthread_create(&gDepDumpThreadList[l_i], NULL, dump_dependency_function, (void *)&gDumpThreadParams[l_i])) {
				LOGE(1, "Error: failed to create a native thread for dumping dependency");
		    }
			
		    LOGI(10, "tttttt: dependency dumping thread started! tttttt");
		}
	}
    LOGI(10, "initialization done");
}

JNIEXPORT jstring JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoCodecName(JNIEnv *pEnv, jobject pObj) {
    char* lCodecName = gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->codec->name;
    return (*pEnv)->NewStringUTF(pEnv, lCodecName);
}

JNIEXPORT jstring JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoFormatName(JNIEnv *pEnv, jobject pObj) {
    char* lFormatName = gFormatCtxList[gCurrentDecodingVideoFileIndex]->iformat->name;
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
    lVideoRes[0] = gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
    lVideoRes[1] = gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
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

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naUpdateZoomLevel(JNIEnv * pEnv, jobject pObj, int _zoomLevelUpdate) {
	gZoomLevelUpdate = _zoomLevelUpdate;
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
    /*wait for the dump dependency thread to finish dumping dependency info first before start decoding a frame*/
    if (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->dump_dependency) {
        while (gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].decode_gop_num >= gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num) {
	    //pthread_mutex_lock(&gVideoPacketQueue.mutex);
            //pthread_cond_wait(&gVideoPacketQueue.cond, &gVideoPacketQueue.mutex);
	    //pthread_mutex_unlock(&gVideoPacketQueue.mutex);
	    usleep(50);    
        }
		LOGI(10, "%d:%d:%d", gNumOfGop, gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].decode_gop_num, gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num);    
		if (gNumOfGop < gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].dep_gop_num) {
	    	load_gop_info(gCurrentDecodingVideoFileIndex, gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->g_gopF);
		}
    }
    for (li = 0; li < gNumOfGop; ++li) {
        if (gVideoPacketNum == gGopStart[li]) {
			//only update the zoom level at the beginning of GOP
			if (gZoomLevelUpdate != 0) {
				gCurrentDecodingVideoFileIndex += gZoomLevelUpdate;
				if (gCurrentDecodingVideoFileIndex >= gNumOfVideoFiles) {
					gCurrentDecodingVideoFileIndex = gNumOfVideoFiles - 1;
				} else if (gCurrentDecodingVideoFileIndex < 0) {
					gCurrentDecodingVideoFileIndex = 0;
				}
				gZoomLevelUpdate = 0;
				LOGI(10, "zoom level updated to %d", gCurrentDecodingVideoFileIndex);
				gCurrentDecodingVideoFileIndex = gZoomLevelToVideoIndex[gCurrentDecodingVideoFileIndex];
				LOGI(10, "use file %d", gCurrentDecodingVideoFileIndex);
			}
            //start of a gop
            gStFrame = gGopStart[li];
			//start of a gop indicates the previous gop is done decoding
	    	gVideoPacketQueueList[gCurrentDecodingVideoFileIndex].decode_gop_num = li;
	    	//3.0 based on roi pixel coordinates, calculate the mb-based roi coordinates
			l_roiSh = (_roiSh - 15) > 0 ? (_roiSh - 15):0;
			l_roiSw = (_roiSw - 15) > 0 ? (_roiSw - 15):0;
			l_roiEh = (_roiEh + 15) < gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height ? (_roiEh + 15):gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
			l_roiEw = (_roiEw + 15) < gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width ? (_roiEw + 15):gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
			l_roiSh = l_roiSh * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
			l_roiSw = l_roiSw * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
	 	    l_roiEh = l_roiEh * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
			l_roiEw = l_roiEw * (gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width/16) / gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
	    	//3.1 check if it's a beginning of a gop, if so, load the pre computation result and compute the inter frame dependency
			LOGI(10, "decode video %d with roi (%d:%d) to (%d:%d)", gCurrentDecodingVideoFileIndex, l_roiSh, l_roiSw, l_roiEh, l_roiEw);
            prepare_decode_of_gop(gCurrentDecodingVideoFileIndex, gGopStart[li], gGopEnd[li], l_roiSh, l_roiSw, l_roiEh, l_roiEw);
            break;
        } else if (gVideoPacketNum < gGopEnd[li]) {
            break;
        }
    }
    //3.2 decode the video packet
	LOGI(10, "decode video %d frame %d", gCurrentDecodingVideoFileIndex, gVideoPacketNum);
    decode_a_video_packet(gCurrentDecodingVideoFileIndex, gRoiSh, gRoiSw, gRoiEh, gRoiEw);
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



