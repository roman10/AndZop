package feipeng.andzop.render;

import feipeng.andzop.Main.ROISettingsStatic;
import feipeng.andzop.render.ZoomState.MODE;
import android.graphics.PointF;
import android.util.FloatMath;
import android.view.MotionEvent;
import android.view.View;

/**
 * Simple on touch listener for RenderView 
 * This listener updates the zoomstate according to user actions
 * currently supports pinch zoom and pan actions
 */
public class SimpleZoomListener implements View.OnTouchListener {
    public enum ControlType {
        NONE, PAN, ZOOM
    }
    private ControlType prControlType = ControlType.ZOOM;
    private ZoomState prState; 		//State being controlled by touch events
    private float prPrevX, prPrevY;	//XY-coordinates of previously handled touch event

    public void setZoomState(ZoomState _state) {
    	prState = _state;
    }
    public void setControlType(ControlType _controlType) {
        prControlType = _controlType;
    }
    public ControlType getControlType() {
    	return prControlType;
    }
    
    /** Determine the space between the first two fingers */
    private float spacing(MotionEvent _event) {
       float x = _event.getX(0) - _event.getX(1);
       float y = _event.getY(0) - _event.getY(1);
       return FloatMath.sqrt(x * x + y * y);
    }
    //for zoom control
    private PointF prStart = new PointF();	//start position of the first finger
    private float prPrevDist = 1f;
    
    public boolean onTouch(View _view, MotionEvent _event) {
        final int lAction = _event.getAction();
        final float lx = _event.getX();
        final float ly = _event.getY();

        switch (lAction & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN:
            	if (prControlType == ControlType.ZOOM) {
	            	//record down the first finger down position and 
	            	//set it as start position
	                prPrevX = lx;
	                prPrevY = ly;
	                prStart.set(lx, ly);
	                prControlType = ControlType.PAN;
            	} else if (prState.getMode() == MODE.FULL) {
            		//decide on which point to move
            		float[] l_roi = prState.getRoi(); //left, top, right, bottom
            		float[] l_dist = new float[4];
            		l_dist[0] = (lx - l_roi[0])*(lx - l_roi[0]) + (ly - l_roi[1])*(ly - l_roi[1]);
            		l_dist[1] = (lx - l_roi[2])*(lx - l_roi[2]) + (ly - l_roi[1])*(ly - l_roi[1]);
            		l_dist[2] = (lx - l_roi[0])*(lx - l_roi[0]) + (ly - l_roi[3])*(ly - l_roi[3]);
            		l_dist[3] = (lx - l_roi[2])*(lx - l_roi[2]) + (ly - l_roi[3])*(ly - l_roi[3]);
            		float minDist = l_dist[0];
            		int minIdx = 0;
            		for (int i = 1; i < 4; ++i) {
            			if (l_dist[i] < minDist) {
            				minDist = l_dist[i];
            				minIdx = i;
            			}
            		}
            		prState.setMoveDir(minIdx);
            	}
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
            	//a second finger detected, calculate the distance, and if the distance
            	//is bigger than a threshold, we confirm it as a second finger
            	//change the control type to zoom
            	prPrevDist = spacing(_event);
            	if (prPrevDist > 10f) {
            		prControlType = ControlType.ZOOM;
            	}
            	break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            	prControlType = ControlType.NONE;
            	break;
            case MotionEvent.ACTION_MOVE: {
            	if (prState.getMode() == MODE.AUTO) {
	            	//the finger(s) is moving
	            	//calculate the relative moved distance
	                final float ldx = (lx - prPrevX) / _view.getWidth();
	                final float ldy = (ly - prPrevY) / _view.getHeight();
	                if (prControlType == ControlType.ZOOM) {
	                	//if it's a zoom action
	                	float newDist = spacing(_event);
	                	if (newDist > 10f) {
	                		float lMovementScale;
	                		if (newDist > prPrevDist) {
	                			lMovementScale = newDist/prPrevDist;	//zoom in
	                		} else {
	                			lMovementScale = (prPrevDist/newDist)*-1;	//zoom out
	                		}
	                		//set threshold, so the update won't be too sensitive
	                		if (Math.abs(lMovementScale) > 1.1f) {
		                		float lZoomScale = lMovementScale/10;
		                		float lPrevZoom = prState.getZoom();
		                		float lNewZoom = lPrevZoom*(1 + lZoomScale);
		                		if (lNewZoom < ZoomState.PUCMINZOOM) {
		                			lNewZoom = ZoomState.PUCMINZOOM;
		                		} else if (lNewZoom > ZoomState.PUCMAXZOOM) {
		                			lNewZoom = ZoomState.PUCMAXZOOM;
		                		}
		                		prState.setZoom(lNewZoom);
		                		prState.notifyObservers();
	                		}
	                	}
	                } else if (prControlType == ControlType.PAN) {
	                    prState.setPanX(prState.getPanX() - ldx);
	                    prState.setPanY(prState.getPanY() - ldy);
	                    prState.notifyObservers();
	                }
	                //update the previous coordinates
	                prPrevX = lx;
	                prPrevY = ly;
	                break;
            	} else if (prState.getMode() == MODE.FULL) {
            		//move the center of the ROI to where user figure points
            		float[] l_roi = prState.getRoi();
            		float l_roi_height = l_roi[2] - l_roi[0];
            		float l_roi_width = l_roi[3] - l_roi[1];
            		prState.setRoiLeft(lx - l_roi_width / 2);
            		prState.setRoiRight(lx + l_roi_width / 2);
            		prState.setRoiTop(ly - l_roi_height / 2);
            		prState.setRoiBottom(ly + l_roi_height / 2);
            		prState.notifyObservers();
            	}
            }

        }
        return true;
    }

}
