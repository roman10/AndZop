JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naClose(JNIEnv *pEnv, jobject pObj) {
    andzop_finish();
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naInit(JNIEnv *pEnv, jobject pObj) {
    int l_mbH, l_mbW;
    int l_i;
    //char* l_videoFileNameList[10];
    jobjectArray l_videoFileNameObjList; 
    jfieldID l_fid;
    jstring l_videoFileNameStr;
    /*get the video file name*/
    //l_videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
    if (pObj == NULL) {
        LOGE(1, "pObj is NULL");
    }
    jclass cls = (*pEnv)->GetObjectClass(pEnv, pObj);
    l_fid = (*pEnv)->GetFieldID(pEnv, cls, "fileNameList", "[Ljava/lang/String;"); 
    l_videoFileNameObjList = (*pEnv)->GetObjectField(pEnv, pObj, l_fid);
    gNumOfVideoFiles = (*pEnv)->GetArrayLength(pEnv, l_videoFileNameObjList);
    LOGI(10, "number of video files: %d", gNumOfVideoFiles);
    gVideoFileNameList = (char **)malloc(gNumOfVideoFiles*sizeof(char*));
    for (l_i = 0; l_i < gNumOfVideoFiles; ++l_i) {
	l_videoFileNameStr = (*pEnv)->GetObjectArrayElement(pEnv, l_videoFileNameObjList, l_i);
	gVideoFileNameList[l_i] = (char *)(*pEnv)->GetStringUTFChars(pEnv, l_videoFileNameStr, NULL);
	LOGI(10, "%d video file name is %s", l_i, gVideoFileNameList[l_i]);
    }
    andzop_init(0);
}

JNIEXPORT jstring JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoCodecName(JNIEnv *pEnv, jobject pObj) {
    char* lCodecName = (char*)gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->codec->name;
    return (*pEnv)->NewStringUTF(pEnv, lCodecName);
}

JNIEXPORT jstring JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoFormatName(JNIEnv *pEnv, jobject pObj) {
    char* lFormatName = (char*)gFormatCtxList[gCurrentDecodingVideoFileIndex]->iformat->name;
    return (*pEnv)->NewStringUTF(pEnv, lFormatName);
}

JNIEXPORT jintArray JNICALL Java_feipeng_andzop_render_RenderView_naGetVideoResolution(JNIEnv *pEnv, jobject pObj) {
    jintArray lRes;
	LOGI(2, "start of get video resolution for %d", gCurrentDecodingVideoFileIndex);
    lRes = (*pEnv)->NewIntArray(pEnv, 2);
    if (lRes == NULL) {
        LOGI(1, "cannot allocate memory for video size");
        return NULL;
    }
    jint lVideoRes[2];
    lVideoRes[0] = gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
    lVideoRes[1] = gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
    LOGI(2, "end of get video resolution for %d (%d, %d)", gCurrentDecodingVideoFileIndex, lVideoRes[0], lVideoRes[1]);
    (*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
    return lRes;
}

/*get the actual roi*/
/*the actual roi may differ from the user requested roi, as the roi can only change at the beginning of gop*/
JNIEXPORT jfloatArray JNICALL Java_feipeng_andzop_render_RenderView_naGetActualRoi(JNIEnv *pEnv, jobject pObj) {
    jfloatArray lRes;
    LOGI(2, "start of naGetActualRoi");
    lRes = (*pEnv)->NewFloatArray(pEnv, 4);
    if (lRes == NULL) {
        LOGI(1, "cannot allocate memory for video size");
        return NULL;
    }
    jfloat lVideoRes[4];
    //enlarge or shrink the roi size according to the ratio of current video
    LOGI(2, "(%d, %d) to (%d, %d)", gRoiSh, gRoiSw, gRoiEh, gRoiEw);
    lVideoRes[0] = gRoiSh*16*gVideoPicture.height/gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
    lVideoRes[1] = gRoiSw*16*gVideoPicture.width/gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
    lVideoRes[2] = gRoiEh*16*gVideoPicture.height/gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->height;
    lVideoRes[3] = gRoiEw*16*gVideoPicture.width/gVideoCodecCtxList[gCurrentDecodingVideoFileIndex]->width;
    LOGI(2, "(%.2f, %.2f) to (%.2f, %.2f)", lVideoRes[0], lVideoRes[1], lVideoRes[2], lVideoRes[3]);
    (*pEnv)->SetFloatArrayRegion(pEnv, lRes, 0, 4, lVideoRes);
    LOGI(2, "end of naGetActualRoi");
    return lRes;
}

JNIEXPORT void JNICALL Java_feipeng_andzop_render_RenderView_naUpdateZoomLevel(JNIEnv * pEnv, jobject pObj, int _zoomLevelUpdate) {
	gZoomLevelUpdate = _zoomLevelUpdate;
}

/*fill in data for a bitmap*/
JNIEXPORT jint JNICALL Java_feipeng_andzop_render_RenderView_naRenderAFrame(JNIEnv * pEnv, jobject pObj, jobject pBitmap, int _width, int _height, float _roiSh, float _roiSw, float _roiEh, float _roiEw) {
    AndroidBitmapInfo lInfo;
    //void* lPixels;
    int lRet;
    int li, lj, lk;
    int lPos;
    unsigned char* ltmp;
    LOGI(3, "start of render_a_frame: %d:%d", _width, _height);    
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
    if ((lRet = AndroidBitmap_lockPixels(pEnv, pBitmap, &gBitmap)) < 0) {
        LOGE(1, "AndroidBitmap_lockPixels() failed! error = %d", lRet);
    }
    //3. modify the pixel buffer
    //decode a video frame: the pBitmap will be filled with decoded pixels
    lRet = decode_a_frame(_width, _height, _roiSh, _roiSw, _roiEh, _roiEw);
    AndroidBitmap_unlockPixels(pEnv, pBitmap);
    LOGI(3, "~~~~~~~~~~end of rendering a frame~~~~~~~~~~~~~~~~~`");
    return lRet;
}
