/*
 * Copyright (c) 2010, Sony Ericsson Mobile Communication AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice, this 
 *      list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *    * Neither the name of the Sony Ericsson Mobile Communication AB nor the names
 *      of its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package feipeng.andzop.render;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

import java.util.ArrayList;
import java.util.Observable;
import java.util.Observer;

import feipeng.andzop.Main.ROISettingsStatic;

/**
 * View capable of drawing an image at different zoom state levels
 */
public class RenderView extends View implements Observer {
	private static final String TAG = "RenderView";
	private String[] fileNameList;
	private final Paint prTextPaint = new Paint();
	private final Paint prFramePaint = new Paint(Paint.FILTER_BITMAP_FLAG);
	/*native methods definition*/	
	private native void naInit(int pDumpDep);
	private static native int[] naGetVideoResolution();
	private static native float[] naGetActualRoi();
	private static native String naGetVideoCodecName();
	private static native String naGetVideoFormatName();
	private static native void naUpdateZoomLevel(int _updateZoomLevel);
	private static native int naRenderAFrame(int _mode, Bitmap _bitmap, int _width, int _height, 
			float _roiSh, float _roiSw, float _roiEh, float _roiEw,
			int _viewSh, int _viewSw, int _viewEh, int _viewEw);
	private static native void naClose();
	//for profiling
	private long prStartTime, prTotalTime;			//total time
	private int lastFrameRate, displayFrameRate;
	private int prFrameCountDecoded, prFrameCountRendered;	//total number of rendered frames
	private long prLastProfile;		//last profile update time, we refresh every second
	private String prLastTime;//last profile is displayed
	
	private boolean ifDisplayStat = true;//set true to display stats on screen
	private boolean prIsProfiling = true;//set true to enable profiling
	
	private int prWidth, prHeight;	//the width and height of the view
	private int[] prVideoRes;//input video resolution
	private String prVideoCodecName, prVideoFormatName;//video codec name and container format name
	
    private final Rect mRectSrc = new Rect();	//for cropping source image.
    private final Rect mRectDst = new Rect();	//drawing area on canvas
    private final AspectQuotient mAspectQuotient = new AspectQuotient();//Object holding aspect quotient
    private Bitmap mBitmap;
    private ZoomState mState;	//State of the zoom.
    
    private boolean ifPlaybackInLoop = true;	//set to true to play in a loop
	/***
	 * 
	 * @param _context
	 * @param _videoFileNameList: input video file list, this implementation only expects the first element
	 * the multi zoom levels implementation supports multiple input videos. 
	 * @param _width: width of the view
	 * @param _height: height of the view
	 */
	public RenderView(Context _context, ArrayList<String> _videoFileNameList, int _width, int _height) {
		super(_context);
		prWidth = _width;
		prHeight = _height;
		/*initialize the paint for painting text info*/
		prTextPaint.setAntiAlias(true);
		prTextPaint.setColor(0xffffffff);
		prTextPaint.setShadowLayer(3.0f, 0.0f, 0.0f, 0xff000000);
		prTextPaint.setTextSize(15.0f);
		//initialize andzop
		fileNameList = new String[_videoFileNameList.size()];
		for (int i = 0; i < _videoFileNameList.size(); ++i) {
			fileNameList[i] = _videoFileNameList.get(i);
		}
		Log.i("RenderView-number of input video", String.valueOf(_videoFileNameList.size()));		
		//initialize the profile parameters
		prTotalTime = 0;
		prFrameCountDecoded = 0;
		prFrameCountRendered = 0;
		prLastProfile = 0;
		prLastTime = "0";
		prStartTime = System.nanoTime();
		//initialize the bitmap
//		mBitmap = Bitmap.createBitmap(prVideoRes[0], prVideoRes[1], Bitmap.Config.ARGB_8888);
		mBitmap = Bitmap.createBitmap(800, 450, Bitmap.Config.ARGB_8888);
		mAspectQuotient.updateAspectQuotient(getWidth(), getHeight(), mBitmap.getWidth(), mBitmap
                .getHeight());
        mAspectQuotient.notifyObservers();
        //prepare video playback
        startPlayback(false);
		//start the video play
		invalidate();
	}
	
	private void startPlayback(boolean ifRestart) {
		if (ifRestart) {
			naClose();
		}
		naInit(0);
		prVideoRes = naGetVideoResolution();//video resolution
		prVideoCodecName = naGetVideoCodecName();//codec name
		prVideoFormatName = naGetVideoFormatName(); //container format
	}
	
	//this is for dumping dependency files only
	public RenderView(Context _context, ArrayList<String> _videoFileNameList) {
        super(_context);
        //initialize andzop
        fileNameList = new String[_videoFileNameList.size()];
        for (int i = 0; i < _videoFileNameList.size(); ++i) {
                fileNameList[i] = _videoFileNameList.get(i);
        }
        Log.i("RenderView-number of input video", String.valueOf(_videoFileNameList.size()));
        Log.i("RenderView dump dep info", "naInit called");
        naInit(1);
        Log.i("RenderView dump dep info", "naInit finished");
}


    /**
     * Set object holding the zoom state that should be used
     * 
     * @param state The zoom state
     */
    public void setZoomState(ZoomState state) {
        if (mState != null) {
            mState.deleteObserver(this);
        }
        mState = state;
        mState.addObserver(this);
        invalidate();
    }

    /**
     * Gets reference to object holding aspect quotient
     * 
     * @return Object holding aspect quotient
     */
    public AspectQuotient getAspectQuotient() {
        return mAspectQuotient;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (mBitmap != null && mState != null) {
            final float aspectQuotient = mAspectQuotient.get();

            final int viewWidth = getWidth();
            final int viewHeight = getHeight();
//            final int bitmapWidth = mBitmap.getWidth();
//            final int bitmapHeight = mBitmap.getHeight();
            final int bitmapWidth = 1920;
            final int bitmapHeight = 1080;

            final float panX = mState.getPanX();
            final float panY = mState.getPanY();
            final float zoomX = mState.getZoomX(aspectQuotient) * viewWidth / bitmapWidth;
            final float zoomY = mState.getZoomY(aspectQuotient) * viewHeight / bitmapHeight;

            // Setup source and destination rectangles
            mRectSrc.left = (int)(panX * bitmapWidth - viewWidth / (zoomX * 2));
            mRectSrc.top = (int)(panY * bitmapHeight - viewHeight / (zoomY * 2));
            mRectSrc.right = (int)(mRectSrc.left + viewWidth / zoomX);
            mRectSrc.bottom = (int)(mRectSrc.top + viewHeight / zoomY);
            mRectDst.left = getLeft();
            mRectDst.top = getTop();
            mRectDst.right = getRight();
            mRectDst.bottom = getBottom();

            // Adjust source rectangle so that it fits within the source image.
            if (mRectSrc.left < 0) {
                mRectDst.left += -mRectSrc.left * zoomX;
                mRectSrc.left = 0;
            }
            if (mRectSrc.right > bitmapWidth) {
                mRectDst.right -= (mRectSrc.right - bitmapWidth) * zoomX;
                mRectSrc.right = bitmapWidth;
            }
            if (mRectSrc.top < 0) {
                mRectDst.top += -mRectSrc.top * zoomY;
                mRectSrc.top = 0;
            }
            if (mRectSrc.bottom > bitmapHeight) {
                mRectDst.bottom -= (mRectSrc.bottom - bitmapHeight) * zoomY;
                mRectSrc.bottom = bitmapHeight;
            }
//            Log.i(TAG, "crop " + mRectSrc.toString() + ";dest " + mRectDst.toString());
            
//            int res = naRenderAFrame(ROISettingsStatic.VIEW_MODE_AUTO, mBitmap, mBitmap.getWidth(), mBitmap.getHeight(),
//            		mRectSrc.top, mRectSrc.left, mRectSrc.bottom, mRectSrc.right);
            int res = naRenderAFrame(ROISettingsStatic.VIEW_MODE_AUTO, mBitmap, 800, 450,
            		mRectSrc.top, mRectSrc.left, mRectSrc.bottom, mRectSrc.right,
            		mRectDst.top, mRectDst.left, mRectDst.bottom, mRectDst.right);
//            canvas.drawBitmap(mBitmap, mRectSrc, mRectDst, prFramePaint);
            if (0 != res) {
            	//video playback finished
            	Log.i(TAG, "playback finished");
            	if (ifPlaybackInLoop) {
            		startPlayback(true);
            	} else {
            		return;
            	}
            }
            canvas.drawBitmap(mBitmap, 0, 0, prFramePaint);
            invalidate();
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        mAspectQuotient.updateAspectQuotient(right - left, bottom - top, mBitmap.getWidth(),
                mBitmap.getHeight());
        mAspectQuotient.notifyObservers();
    }

    // implements Observer
    public void update(Observable observable, Object data) {
        invalidate();
    }

}
