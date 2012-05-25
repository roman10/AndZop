package feipeng.andzop.render;

import java.util.Observable;


/**Observable is used to notify a group of observer objects when a change occurs.
 * On creation, the set of observers is empty. 
 * After a change occurs, the application should call notifyObservers() method, which
 * will cause the invocation of the update() method of all registered Observers.
 * The default implementation call the observers in the oder they registered.
 */
/**
 * A ZoomState holds zoom and pan values and allows the user to read and listen
 * to changes. Clients that modify ZoomState should call notifyObservers()
 */
public class ZoomState extends Observable {
    // Zoom level A value of 1.0 means the content fits the view
    private float prZoom;
    /*X Y-coordinate of zoom window center position,
     * relative to the width and height of the content.*/
    private float prPanX, prPanY;
    //the minimum and maximum zoom levels
    public static final float PUCMINZOOM = 0.3f;
    public static final float PUCMAXZOOM = 5;
    
    //the minimum and maximum pan levels
    public static final float PUCMINPAN = 0.0f;
    public static final float PUCMAXPAN = 1.0f;
    
    private int prZoomLevel = 0;
    private int prLastZoomLevel = 0;
    
    public enum MODE {
		FULL, AUTO, ROI
	}
    private MODE prMode;
    //for full view mode, keep track of the top, left, bottom, right coordinates
    public static float[] prVideoRoi = new float[4];  //video roi, stH, stW, edH, edW
    private int prMoveDir;
    
    public ZoomState() {
    	prPanX = 0.5f;
    	prPanY = 0.5f;
    	prZoom = 1f;
    	prMode = MODE.AUTO;
    	prVideoRoi[0] = 0;
    	prVideoRoi[1] = 0;
    	prVideoRoi[2] = 1080;
    	prVideoRoi[3] = 1920;
    	prZoomLevel = 0;
    	prLastZoomLevel = 0;
    }
    
    public void setMode(MODE _mode) {
    	prMode = _mode;
    	setChanged();
    }
    
    public MODE getMode() {
    	return prMode;
    }
    
    public float[] getRoi() {
    	return prVideoRoi;
    }
    
    //the roi move direction: indicate which corner to move
    public int getMoveDir() {
    	return prMoveDir;
    }
    
    public void setMoveDir(int _dir) {
    	prMoveDir = _dir;
    }
    
    public void setRoiLeft(float _left) {
    	prVideoRoi[1] = _left;
    }
    
    public float getRoiLeft() {
    	return prVideoRoi[1];
    }
    
    public void setRoiTop(float _top) {
    	prVideoRoi[0] = _top;
    }
    
    public float getRoiTop() {
    	return prVideoRoi[0];
    }
    
    public void setRoiRight(float _right) {
    	prVideoRoi[3] = _right;
    }
    
    public float getRoiRight() {
    	return prVideoRoi[3];
    }
    
    public void setRoiBottom(float _bottom) {
    	prVideoRoi[2] = _bottom;
    }
    
    public float getRoiBottom() {
    	return prVideoRoi[2];
    }
    
    public float getPanX() {
        return prPanX;
    }
    public float getPanY() {
        return prPanY;
    }
    public float getZoom() {
        return prZoom;
    }
    
    public float getZoomY(float aspectQuotient) {
        return Math.min(prZoom, prZoom / aspectQuotient);
    }
    
    public float getZoomX(float aspectQuotient) {
        return Math.min(prZoom, prZoom * aspectQuotient);
    }
    
    public void resetZoomState() {
    	prPanX = 0.5f;
    	prPanY = 0.5f;
    	prZoom = 1f;
    	setChanged();
    }
    public void setPanX(float _panX) {
    	if (_panX < PUCMINPAN) {
    		_panX = PUCMINPAN;
    	} else if (_panX > PUCMAXPAN) {
    		_panX = PUCMAXPAN;
    	}
        if (_panX != prPanX) {
            prPanX = _panX;
            setChanged();
       }
    }   
    public void setPanY(float _panY) {
    	if (_panY < PUCMINPAN) {
    		_panY = PUCMINPAN;
    	} else if (_panY > PUCMAXPAN) {
    		_panY = PUCMAXPAN;
    	}
        if (_panY != prPanY) {
            prPanY = _panY;
            setChanged();
        }
    }
    public void setZoom(float _zoom) {
        if (_zoom != prZoom) {
            prZoom = _zoom;
            setChanged();
        }
    }
    
    public int getZoomLevelUpdate() {
    	return prLastZoomLevel-prZoomLevel;
    }
    
    public void setZoomLevel(int _zoomLevel) {
    	prLastZoomLevel = prZoomLevel;
    	prZoomLevel = _zoomLevel;
    	setChanged();
    }
}
