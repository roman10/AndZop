package feipeng.andzop.render;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Observable;
import java.util.Observer;

import feipeng.andzop.Main.ROISettingsStatic;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;

/**
 * This class represents the player rendering screen
 *  @author roman10
 *	naming convention:
 *  class private fields start with pr, public fields start with pu
 *  method local fields start with l, input parameters start with _
 *  final fields start with c (as in constant)
 *  all native methods start with na
 */
/**
 * UI control:
 * 1. change of ROI: Long press the screen, then drag the red box, roi is updated at beginning of a gop 
 * 2. zoom control: pinch zoom in/zoom out
 */
public class RenderView extends View implements Observer {
	private static final String TAG = "RenderView";
	//private String prVideoFileName;
	private String[] fileNameList;
	private Bitmap prBitmap;
	private final Paint prTextPaint = new Paint();
	private final Paint prRoiPaint = new Paint();
	private final Paint prActualRoiPaint = new Paint();
	private final Paint prFramePaint = new Paint(Paint.FILTER_BITMAP_FLAG);
	private int prDelay;	//in milliseconds
	private Handler prVideoDisplayHandler = new Handler();
	
	private native void naInit();
	private static native int[] naGetVideoResolution();
	private static native float[] naGetActualRoi();
	private static native String naGetVideoCodecName();
	private static native String naGetVideoFormatName();
	//private static native int naGetNextFrameDelay();
	private static native void naUpdateZoomLevel(int _updateZoomLevel);
	private static native int naRenderAFrame(Bitmap _bitmap, int _width, int _height, float _roiSh, float _roiSw, float _roiEh, float _roiEw);
	//fake method for testing
	private int naGetNextFrameDelay() {
		//this is useful when multi-thread support is added
		return 0;
	}
	private static native void naClose();
	
	private int prCurrentColorIndex = 0;
	private int[] prColorList = {
		Color.WHITE, Color.BLUE, Color.CYAN, Color.RED, Color.YELLOW 
	};
	private int prTmpCnt = 0;
	//for profiling
	private boolean prIsProfiling;
	private long prStartTime, prTotalTime;			//total time
	private int lastFrameRate, displayFrameRate;
	private int prFrameCountDecoded, prFrameCountRendered;	//total number of rendered frames
	private long prLastProfile;		//last profile update time, we refresh every second
	private String prLastTime;//last profile is displayed
	private boolean prStopPlay = false;
	//implementation below is simplified for getting the frame rate more conveniently
	private Runnable prDisplayVideoTask = new Runnable() {
		public void run() {
			prStartTime = System.nanoTime();
			while (true) {
				//check if we should stop playing
				if (prStopPlay) {
					stopRendering();
					return;
				}
//				if (ROISettingsStatic.getViewMode(getContext()) == 1) {
//					//auto
//					updateROIAuto();
//				}
				float[] prVideoRoi = prZoomState.getRoi();
				if (prZoomLevelUpdate != 0) {
					//update the zoom level
					naUpdateZoomLevel(prZoomLevelUpdate);
					prZoomLevelUpdate = 0;
				}
				int res = naRenderAFrame(prBitmap, prBitmap.getWidth(), prBitmap.getHeight(), prVideoRoi[0], prVideoRoi[1], prVideoRoi[2], prVideoRoi[3]); //fill the bitmap with video frame data
				if (res == 0) {
					//video is finished playing
					Log.i("prDisplayVideoTask", "video play finished");
					prStopPlay = true;
				}
				if (prIsProfiling) {
					++prFrameCountDecoded;
				}
				invalidate();	                 //display the frame
			}
		}
	};
	//implementation below is more likely to appear in real player
//	private Runnable prDisplayVideoTask = new Runnable() {
//		public void run() {
//			if (prStopPlay) {
//				stopRendering();
//				return;
//			}
//			//Log.i("renderView-run", "FRAME ST");
//			long lStartTime = System.nanoTime();
//			if (ROISettingsStatic.getViewMode(getContext()) == 1) {
//				//auto
//				updateROIAuto();
//			}
//			float[] prVideoRoi = prZoomState.getRoi();
//			if (prZoomLevelUpdate != 0) {
//				//update the zoom level
//				naUpdateZoomLevel(prZoomLevelUpdate);
//				prZoomLevelUpdate = 0;
//			}
//			int res = naRenderAFrame(prBitmap, prBitmap.getWidth(), prBitmap.getHeight(), prVideoRoi[0], prVideoRoi[1], prVideoRoi[2], prVideoRoi[3]); //fill the bitmap with video frame data
//			if (res == 0) {
//				//video is finished playing
//				Log.i("prDisplayVideoTask", "video play finished");
//				prStopPlay = true;
//			}
//			if (prIsProfiling) {
//				++prFrameCountDecoded;
//			}
//			invalidate();	                 //display the frame
//			prDelay = naGetNextFrameDelay(); //retrieve the next frame delay time
//			//based on next frame delay to trigger another execution of handler
//			//if (prTmpCnt++ < 1) 
//			prVideoDisplayHandler.postDelayed(this, prDelay);
//			long lEndTime = System.nanoTime();
//			if (prIsProfiling) {
//				prTotalTime += (lEndTime - lStartTime) / 1000000;	//in milliseconds
//			}
//			//Log.i("renderView-run", "FRAME ED");
//		}
//	};
	public int prZoomLevelUpdate;
	private int prVisHeight, prVisWidth;
	private int[] prVideoRes;
	private String prVideoCodecName, prVideoFormatName;
	public RenderView(Context _context, ArrayList<String> _videoFileNameList, int _width, int _height) {
		super(_context);
		prRoiPaint.setColor(Color.RED);
		prActualRoiPaint.setColor(Color.BLUE);
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
		naInit();
		/*get the video resolution*/
		prVideoRes = naGetVideoResolution();
		/*calculate the visible video frame size/bitmap size*/
		if (((float)prVideoRes[0])/prVideoRes[1] > ((float)_width)/_height) {
			prVisWidth = _width;
			prVisHeight = (int)((float)prVideoRes[1])*prVisWidth/prVideoRes[0];
		} else {
			prVisHeight = _height;
			prVisWidth = (int)((float)prVideoRes[0])*prVisHeight/prVideoRes[1];
		}
		Log.i("RenderView-visible rect", prVisHeight + ":" + prVisWidth);
		/*initialize the bitmap according to visible size*/
		prBitmap = Bitmap.createBitmap(prVisWidth, prVisHeight, Bitmap.Config.ARGB_8888);
		//prBitmap = Bitmap.createBitmap(prVideoRes[0], prVideoRes[1], Bitmap.Config.ARGB_8888);
		//prBitmap = Bitmap.createBitmap(prVideoRes[0], prVideoRes[1], Bitmap.Config.RGB_565);
		//prBitmap = Bitmap.createBitmap(cW, cH, Bitmap.Config.RGB_565);
		/*get video codec name*/
		prVideoCodecName = naGetVideoCodecName();
		prVideoFormatName = naGetVideoFormatName();
		//initialize the profile parameters
		prTotalTime = 0;
		prFrameCountDecoded = 0;
		prFrameCountRendered = 0;
		prLastProfile = 0;
		prLastTime = "0";
		prIsProfiling = true;
		//start the video play
		//prDelay = 1000;		//initial delay
		prDelay = 0;
		//initialize the log file 
		DumpUtils.createDirIfNotExist("/sdcard/andzop/");
		String logFstr = "/sdcard/andzop/" + new File(fileNameList[0]).getName() + System.currentTimeMillis() + ".txt";
		try {
			_logF = new FileWriter(logFstr);
		} catch (IOException e) {
			Log.v(TAG, "error creating log file");
		}
		prStartTime = System.nanoTime();
		invalidate();
//		prVideoDisplayHandler.removeCallbacks(prDisplayVideoTask);
//		prVideoDisplayHandler.postDelayed(prDisplayVideoTask, prDelay);
	}
	
	public RenderView(Context _context, ArrayList<String> _videoFileNameList) {
		super(_context);
		//initialize andzop
		fileNameList = new String[_videoFileNameList.size()];
		for (int i = 0; i < _videoFileNameList.size(); ++i) {
			fileNameList[i] = _videoFileNameList.get(i);
		}
		Log.i("RenderView-number of input video", String.valueOf(_videoFileNameList.size()));
		Log.i("RenderView dump dep info", "naInit called");
		naInit();
		Log.i("RenderView dump dep info", "naInit finished");
	}
	
	//onDraw method actually displays the video on screen
	//private int prColor = 0xFF000001;
	private ZoomState prZoomState;
	public void setZoomState(ZoomState _zoomState) {
		if (prZoomState != null) {
			prZoomState.deleteObserver(this);
		}
		prZoomState = _zoomState;
		int l_viewMode = ROISettingsStatic.getViewMode(this.getContext());
		if (l_viewMode == 1) {
			//auto mode, set the initial ROI as the entire frame
			prZoomState.setRoiTop(0);
			prZoomState.setRoiBottom(prVideoRes[1]);
			prZoomState.setRoiLeft(0);
			prZoomState.setRoiRight(prVideoRes[0]);
		} else {
//			float l_tmp = (prVideoRes[1] - prVideoRes[1]*ROISettingsStatic.getROIHeight(getContext())/100)/2;
//			prZoomState.setRoiTop(l_tmp);
//			prZoomState.setRoiBottom(l_tmp + prVideoRes[1]*ROISettingsStatic.getROIHeight(getContext())/100);
			
			float l_tmp = (prVisHeight - prVisHeight*ROISettingsStatic.getROIHeight(getContext())/100)/2;
			prZoomState.setRoiTop(l_tmp);
			prZoomState.setRoiBottom(l_tmp + prVisHeight*ROISettingsStatic.getROIHeight(getContext())/100);
			
			l_tmp = (prVisWidth - prVisWidth*ROISettingsStatic.getROIWidth(getContext())/100)/2;
			prZoomState.setRoiLeft(l_tmp);
			prZoomState.setRoiRight(l_tmp + prVisWidth*ROISettingsStatic.getROIWidth(getContext())/100);
		}
		prZoomState.addObserver(this);
	}
	
	public void updateROI() {
		int l_viewMode = ROISettingsStatic.getViewMode(this.getContext());
		if (l_viewMode == 1) {
			
		} else {
//			float l_tmp = (prVideoRes[1] - prVideoRes[1]*ROISettingsStatic.getROIHeight(getContext())/100)/2;
//			prZoomState.setRoiTop(l_tmp);
//			prZoomState.setRoiBottom(l_tmp + prVideoRes[1]*ROISettingsStatic.getROIHeight(getContext())/100);
//			
//			l_tmp = (prVideoRes[0] - prVideoRes[0]*ROISettingsStatic.getROIWidth(getContext())/100)/2;
//			prZoomState.setRoiLeft(l_tmp);
//			prZoomState.setRoiRight(l_tmp + prVideoRes[0]*ROISettingsStatic.getROIWidth(getContext())/100);
			
			float l_tmp = (prVisHeight - prVisHeight*ROISettingsStatic.getROIHeight(getContext())/100)/2;
			prZoomState.setRoiTop(l_tmp);
			prZoomState.setRoiBottom(l_tmp + prVisHeight*ROISettingsStatic.getROIHeight(getContext())/100);
			
			l_tmp = (prVisWidth - prVisWidth*ROISettingsStatic.getROIWidth(getContext())/100)/2;
			prZoomState.setRoiLeft(l_tmp);
			prZoomState.setRoiRight(l_tmp + prVisWidth*ROISettingsStatic.getROIWidth(getContext())/100);
		}
	}
	
	private float prLastZoom = 0;
	private float prLastPanX = 0;
	private float prLastPanY = 0;
	public void updateROIAuto() {
		float lZoom = prZoomState.getZoom();
		float lPanX = prZoomState.getPanX();
		float lPanY = prZoomState.getPanY();
		if (Math.abs(lZoom - prLastZoom) > 0.1 || Math.abs(lPanX - prLastPanX) > 0.1 || Math.abs(lPanY - prLastPanY) > 0.1) {
			int lBitmapWidth = prBitmap.getWidth();
			int lBitmapHeight = prBitmap.getHeight();
			//AUTO
			int l_tmp = (int)(lPanX*lBitmapWidth - prVisWidth / (lZoom*2));
			prZoomState.setRoiLeft(l_tmp);
			prZoomState.setRoiRight((int)(l_tmp + prVisWidth / lZoom));
			l_tmp = (int)(lPanY*lBitmapHeight - prVisHeight / (lZoom*2));
			prZoomState.setRoiTop(l_tmp);
			prZoomState.setRoiBottom((int)(l_tmp + prVisHeight / lZoom));
			ROISettingsStatic.setROIHeight(this.getContext(), (int)(prZoomState.getRoiBottom() - prZoomState.getRoiTop())/lBitmapHeight*100);
			ROISettingsStatic.setROIWidth(this.getContext(), (int)(prZoomState.getRoiRight() - prZoomState.getRoiLeft())/lBitmapWidth*100);
			prLastZoom = lZoom;
			prLastPanX = lPanX;
			prLastPanY = lPanY;
		}
	}
	
	private Rect prSrcRect = new Rect();
	private Rect prDestRect = new Rect();
	private FileWriter _logF;
	private long totalTime;
	//float[] prVideoRoi = new float[]{480.0f, 960.0f, 1024.0f, 1920.0f};
	//float[] prVideoRoi = new float[]{480.0f, 0.0f, 1024.0f, 960.0f};
	//float[] prVideoRoi = new float[]{0.0f, 0.0f, 752.0f, 1344.0f};
	//float[] prVideoRoi = new float[]{0.0f, 144.0f, 752.0f, 1488.0f};
	//float[] prVideoRoi = new float[]{0.0f, 288.0f, 752.0f, 1632.0f};
	//float[] prVideoRoi = new float[]{0.0f, 432.0f, 752.0f, 1776.0f};
	//float[] prVideoRoi = new float[]{0.0f, 576.0f, 752.0f, 1920.0f};
	//float[] prVideoRoi = new float[]{160.0f, 0.0f, 912.0f, 1344.0f};
	//float[] prVideoRoi = new float[]{160.0f, 144.0f, 912.0f, 1488.0f};
	//float[] prVideoRoi = new float[]{160.0f, 288.0f, 912.0f, 1632.0f};
	//float[] prVideoRoi = new float[]{160.0f, 432.0f, 912.0f, 1776.0f};
//	float[] prVideoRoi = new float[]{160.0f, 576.0f, 912.0f, 1920.0f};
//	float[] prVideoRoi = new float[]{320.0f, 0.0f, 1072.0f, 1344.0f};
//	float[] prVideoRoi = new float[]{320.0f, 144.0f, 1072.0f, 1488.0f};
//	float[] prVideoRoi = new float[]{320.0f, 288.0f, 1072.0f, 1632.0f};
//	float[] prVideoRoi = new float[]{320.0f, 432.0f, 1072.0f, 1776.0f};
//	float[] prVideoRoi = new float[]{320.0f, 576.0f, 1072.0f, 1920.0f};
//	float[] prVideoRoi = new float[]{336.0f, 0.0f, 1088.0f, 1344.0f};
//	float[] prVideoRoi = new float[]{336.0f, 144.0f, 1088.0f, 1488.0f};
//	float[] prVideoRoi = new float[]{336.0f, 288.0f, 1088.0f, 1632.0f};
//	float[] prVideoRoi = new float[]{336.0f, 432.0f, 1088.0f, 1776.0f};
//	float[] prVideoRoi = new float[]{336.0f, 576.0f, 1088.0f, 1920.0f};
//	float[] prVideoRoi = new float[]{0.0f, 0.0f, 544.0f, 960.0f};
//	float[] prVideoRoi = new float[]{0.0f, 160.0f, 544.0f, 1120.0f};
//	float[] prVideoRoi = new float[]{0.0f, 320.0f, 544.0f, 1280.0f};
//	float[] prVideoRoi = new float[]{0.0f, 480.0f, 544.0f, 1440.0f};
//	float[] prVideoRoi = new float[]{0.0f, 640.0f, 544.0f, 1600.0f};
//	float[] prVideoRoi = new float[]{0.0f, 800.0f, 544.0f, 1760.0f};
//	float[] prVideoRoi = new float[]{0.0f, 960.0f, 544.0f, 1920.0f};
	
//	float[] prVideoRoi = new float[]{160.0f, 0.0f, 704.0f, 960.0f};
//	float[] prVideoRoi = new float[]{160.0f, 160.0f, 704.0f, 1120.0f};
//	float[] prVideoRoi = new float[]{160.0f, 320.0f, 704.0f, 1280.0f};
//	float[] prVideoRoi = new float[]{160.0f, 480.0f, 704.0f, 1440.0f};
//	float[] prVideoRoi = new float[]{160.0f, 640.0f, 704.0f, 1600.0f};
//	float[] prVideoRoi = new float[]{160.0f, 800.0f, 704.0f, 1760.0f};
//	float[] prVideoRoi = new float[]{160.0f, 960.0f, 704.0f, 1920.0f};
	
	
//	float[] prVideoRoi = new float[]{320.0f, 0.0f, 864.0f, 960.0f};
//	float[] prVideoRoi = new float[]{320.0f, 160.0f, 864.0f, 1120.0f};
//	float[] prVideoRoi = new float[]{320.0f, 320.0f, 864.0f, 1280.0f};
//	float[] prVideoRoi = new float[]{320.0f, 480.0f, 864.0f, 1440.0f};
//	float[] prVideoRoi = new float[]{320.0f, 640.0f, 864.0f, 1600.0f};
//	float[] prVideoRoi = new float[]{320.0f, 800.0f, 864.0f, 1760.0f};
//	float[] prVideoRoi = new float[]{320.0f, 960.0f, 864.0f, 1920.0f};
	
//	float[] prVideoRoi = new float[]{480.0f, 0.0f, 1024.0f, 960.0f};
//	float[] prVideoRoi = new float[]{480.0f, 160.0f, 1024.0f, 1120.0f};
//	float[] prVideoRoi = new float[]{480.0f, 320.0f, 1024.0f, 1280.0f};
//	float[] prVideoRoi = new float[]{480.0f, 480.0f, 1024.0f, 1440.0f};
	
	float[] prVideoRoi = new float[]{496.0f, 864.0f, 608.0f, 1056.0f};
//	float[] prVideoRoi = new float[]{432.0f, 768.0f, 656.0f, 1152.0f};
//	float[] prVideoRoi = new float[]{384.0f, 672.0f, 704.0f, 1248.0f};
//	float[] prVideoRoi = new float[]{336.0f, 576.0f, 768.0f, 1344.0f};  //40%
//	float[] prVideoRoi = new float[]{272.0f, 480.0f, 816.0f, 1440.0f}; //50%
//	float[] prVideoRoi = new float[]{224.0f, 384.0f, 880.0f, 1536.0f};  //60%
//	float[] prVideoRoi = new float[]{176.0f, 288.0f, 928.0f, 1632.0f};  //70%
//	float[] prVideoRoi = new float[]{112.0f, 192.0f, 976.0f, 1728.0f};  //80%
//	float[] prVideoRoi = new float[]{48.0f, 96.0f, 1024.0f, 1824.0f};  //90%
	//float[] prVideoRoi = new float[]{0.0f, 0.0f, 1080.0f, 1920.0f};  //100%
	
	
//	float[] prVideoRoi = new float[]{480.0f, 800.0f, 1024.0f, 1760.0f};
//	float[] prVideoRoi = new float[]{480.0f, 960.0f, 1024.0f, 1920.0f};
	
//	float[] prVideoRoi = new float[]{544.0f, 0.0f, 1088.0f, 960.0f};
//	float[] prVideoRoi = new float[]{544.0f, 160.0f, 1088.0f, 1120.0f};
//	float[] prVideoRoi = new float[]{544.0f, 320.0f, 1088.0f, 1280.0f};
//	float[] prVideoRoi = new float[]{544.0f, 480.0f, 1088.0f, 1440.0f};
//	float[] prVideoRoi = new float[]{544.0f, 640.0f, 1088.0f, 1600.0f};
//	float[] prVideoRoi = new float[]{544.0f, 800.0f, 1088.0f, 1760.0f};
//	float[] prVideoRoi = new float[]{544.0f, 960.0f, 1088.0f, 1920.0f};
		
	@Override protected void onDraw(Canvas _canvas) {
//		if (prFrameCountDecoded > 1500) {
//			return;
//		}
		//float[] prVideoRoi = prZoomState.getRoi();
		if (prZoomLevelUpdate != 0) {
			//update the zoom level
			naUpdateZoomLevel(prZoomLevelUpdate);
			prZoomLevelUpdate = 0;
		}
		if (prFrameCountRendered > 300) {
			Log.i(TAG, "---FR" + ":" + totalTime + ":" + prFrameCountRendered);
		}
		int res = naRenderAFrame(prBitmap, prBitmap.getWidth(), prBitmap.getHeight(), prVideoRoi[0], prVideoRoi[1], prVideoRoi[2], prVideoRoi[3]); //fill the bitmap with video frame data
		if (res == 0) {
			//video is finished playing
			Log.i("prDisplayVideoTask", "video play finished");
			prStopPlay = true;
			return;
		}
		if (prIsProfiling) {
			++prFrameCountDecoded;
		}
		if (prBitmap != null && prZoomState != null) {
			int l_viewMode = ROISettingsStatic.getViewMode(this.getContext());
			/*computation of the scaling*/
			float lZoom = prZoomState.getZoom();
			float lPanX = prZoomState.getPanX();
			float lPanY = prZoomState.getPanY();
			int lBitmapWidth = prBitmap.getWidth();
			int lBitmapHeight = prBitmap.getHeight();
			float[] prActualVideoRoi = naGetActualRoi();
			if (l_viewMode == 0) {
				//FULL
				prSrcRect.left = 0;
				prSrcRect.top = 0;
				prSrcRect.right = lBitmapWidth;
				prSrcRect.bottom = lBitmapHeight;
			} else if (l_viewMode == 1) {
				//AUTO
				prSrcRect.left = (int)(lPanX*lBitmapWidth - prVisWidth / (lZoom*2));
				prSrcRect.top = (int)(lPanY*lBitmapHeight - prVisHeight / (lZoom*2));
				prSrcRect.right = (int)(prSrcRect.left + prVisWidth / lZoom);
				prSrcRect.bottom = (int)(prSrcRect.top + prVisHeight / lZoom);
			} else if (l_viewMode == 2) {
				//ROI
//				prSrcRect.top = (int) prActualVideoRoi[0];
//				prSrcRect.left = (int) prActualVideoRoi[1];
//				prSrcRect.bottom = (int) prActualVideoRoi[2];
//				prSrcRect.right = (int) prActualVideoRoi[3];
			}
			prDestRect.left = 0;
			prDestRect.top = 0;
			prDestRect.right = prVisWidth;
			prDestRect.bottom = prVisHeight;	
			/*adjust the source rectangle so it doesn't exceed source bitmap boundary*/
			if (prSrcRect.left < 0) {
				prSrcRect.left = 0;
			}
			if (prSrcRect.right > lBitmapWidth) {
				prSrcRect.right = lBitmapWidth;
			}
			if (prSrcRect.top < 0) {
				prSrcRect.top = 0;
			}
			if (prSrcRect.bottom > lBitmapHeight) {
				prSrcRect.bottom = lBitmapHeight;
			}
			//System.gc();		//this call will cause some pause, and consume time
			Log.i("drawbitmap", "---RENDER ST");
			//_canvas.drawBitmap(prBitmap, prSrcRect, prDestRect, prFramePaint);
			_canvas.drawBitmap(prBitmap, 0, 0, prFramePaint);
			//SystemClock.sleep(100);
			++prFrameCountRendered;
			++lastFrameRate;
			Log.i("drawbitmap", "---RENDER ED");
			//DumpUtils.dumpZoom(lZoom, _logF);
			//DumpUtils.dumpPanXY(lPanX, lPanY, _logF);
			//DumpUtils.dumpROI(prSrcRect, _logF);  //this roi is the display screen
			//draw the profiled time
		    totalTime = (System.nanoTime() - prStartTime)/1000000;
			StringBuilder sb = new StringBuilder();
			sb.append("Avg Time/F: ").append(totalTime/prFrameCountRendered).append("; Last Second Frame rate: ").append(displayFrameRate);
			//Log.i(TAG, "---FR" + ":" + totalTime + ":" + prFrameCountRendered);
			_canvas.drawText(sb.toString(), 10.0f, 20.0f, prTextPaint);
			sb.setLength(0);
			sb.append("frame No. : ").append(prFrameCountRendered).append(",").append(prFrameCountDecoded);
			_canvas.drawText(sb.toString(), 10.0f, 40.0f, prTextPaint);
			sb.setLength(0);
			prVideoRes = naGetVideoResolution();		//todo: tmp solution
			sb.append("Video Resolution: ").append(prVideoRes[0]).append("x").append(prVideoRes[1]);
			_canvas.drawText(sb.toString(), 10.0f, 60.0f, prTextPaint);
			sb.setLength(0);
			sb.append("Display Size: ").append(prVisWidth).append("x").append(prVisHeight);
			_canvas.drawText(sb.toString(), 10.0f, 80.0f, prTextPaint);
			sb.setLength(0);
			sb.append("Video Format/Codec: ").append(prVideoFormatName).append("/").append(prVideoCodecName);
			_canvas.drawText(sb.toString(), 10.0f, 100.0f, prTextPaint);
			sb.setLength(0);
			//prVideoRoi = prZoomState.getRoi();
			sb.append("Requested ROI: [").append(prVideoRoi[0]).append(", ").append(prVideoRoi[1]).append("], [").append(prVideoRoi[2]).append(", ").append(prVideoRoi[3]).append("]");
			_canvas.drawText(sb.toString(), 10.0f, 120.0f, prTextPaint);
			//draw the roi in red line
			//top, left, bottom, right = prVideoRoi[0,1,2,3]
			//_canvas.drawRect(prVideoRoi[0], prVideoRoi[1], prVideoRoi[2], prVideoRoi[3], prRoiPaint);
//			if (l_viewMode == 0) {
//				_canvas.drawLine(prVideoRoi[1], prVideoRoi[0], prVideoRoi[3], prVideoRoi[0], prRoiPaint);
//				_canvas.drawLine(prVideoRoi[1], prVideoRoi[0], prVideoRoi[1], prVideoRoi[2], prRoiPaint);
//				_canvas.drawLine(prVideoRoi[1], prVideoRoi[2], prVideoRoi[3], prVideoRoi[2], prRoiPaint);
//				_canvas.drawLine(prVideoRoi[3], prVideoRoi[0], prVideoRoi[3], prVideoRoi[2], prRoiPaint);
//			}
			sb.setLength(0);
			sb.append("Actual ROI: [").append(prActualVideoRoi[0]).append(", ").append(prActualVideoRoi[1]).append("], [").append(prActualVideoRoi[2]).append(", ").append(prActualVideoRoi[3]).append("]");
			_canvas.drawText(sb.toString(), 10.0f, 140.0f, prTextPaint);
			if (l_viewMode == 0) {
				_canvas.drawLine(prActualVideoRoi[1], prActualVideoRoi[0], prActualVideoRoi[3], prActualVideoRoi[0], prActualRoiPaint);
				_canvas.drawLine(prActualVideoRoi[1], prActualVideoRoi[0], prActualVideoRoi[1], prActualVideoRoi[2], prActualRoiPaint);
				_canvas.drawLine(prActualVideoRoi[1], prActualVideoRoi[2], prActualVideoRoi[3], prActualVideoRoi[2], prActualRoiPaint);
				_canvas.drawLine(prActualVideoRoi[3], prActualVideoRoi[0], prActualVideoRoi[3], prActualVideoRoi[2], prActualRoiPaint);
			}
			if (prIsProfiling) {
				if (SystemClock.elapsedRealtime() - prLastProfile > 1000 && prFrameCountRendered > 2) {
					//-1: the current has not counted to prTotalTime yet, so -1
					displayFrameRate = lastFrameRate;
					lastFrameRate = 0;
					prLastProfile = SystemClock.elapsedRealtime();
				}
			}
		}
		invalidate();	                 //display the frame
	}
	
	@Override protected void onDetachedFromWindow() {
		stopRendering();
	}
	
	private void stopRendering() {
		prVideoDisplayHandler.removeCallbacks(prDisplayVideoTask);
		naClose();
		try {
			_logF.flush();
			_logF.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
	/*the update method is triggered when ZoomState.notifyObservers() is called*/
	public void update(Observable observable, Object data) {
		int lZoomLevelUpdate = prZoomState.getZoomLevelUpdate();
		if (lZoomLevelUpdate!=0) {
			naUpdateZoomLevel(lZoomLevelUpdate);
		}
		invalidate();
	}
}
