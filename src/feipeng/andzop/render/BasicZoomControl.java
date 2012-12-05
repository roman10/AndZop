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

import java.util.Observable;
import java.util.Observer;

/**
 * The BasicZoomControl is responsible for controlling a ZoomState. It makes
 * sure that pan movement follows the finger, that limits are satisfied and that
 * we can zoom into specific positions.
 * 
 * In order to implement these control mechanisms access to certain content and
 * view state data is required which is made possible through the
 * ZoomContentViewState.
 */
public class BasicZoomControl implements Observer {

    /** Minimum zoom level limit */
    private static final float MIN_ZOOM = 1;

    /** Maximum zoom level limit */
    private static final float MAX_ZOOM = 16;

    /** Zoom state under control */
    private final ZoomState mState = new ZoomState();

    /** Object holding aspect quotient of view and content */
    private AspectQuotient mAspectQuotient;

    /**
     * Set reference object holding aspect quotient
     * 
     * @param aspectQuotient Object holding aspect quotient
     */
    public void setAspectQuotient(AspectQuotient aspectQuotient) {
        if (mAspectQuotient != null) {
            mAspectQuotient.deleteObserver(this);
        }

        mAspectQuotient = aspectQuotient;
        mAspectQuotient.addObserver(this);
    }

    /**
     * Get zoom state being controlled
     * 
     * @return The zoom state
     */
    public ZoomState getZoomState() {
        return mState;
    }

    /**
     * Zoom
     * 
     * @param f Factor of zoom to apply
     * @param x X-coordinate of invariant position
     * @param y Y-coordinate of invariant position
     */
    public void zoom(float f, float x, float y) {
        final float aspectQuotient = mAspectQuotient.get();

        final float prevZoomX = mState.getZoomX(aspectQuotient);
        final float prevZoomY = mState.getZoomY(aspectQuotient);

        mState.setZoom(mState.getZoom() * f);
        limitZoom();

        final float newZoomX = mState.getZoomX(aspectQuotient);
        final float newZoomY = mState.getZoomY(aspectQuotient);

        // Pan to keep x and y coordinate invariant
        mState.setPanX(mState.getPanX() + (x - .5f) * (1f / prevZoomX - 1f / newZoomX));
        mState.setPanY(mState.getPanY() + (y - .5f) * (1f / prevZoomY - 1f / newZoomY));

        limitPan();

        mState.notifyObservers();
    }

    /**
     * Pan
     * 
     * @param dx Amount to pan in x-dimension
     * @param dy Amount to pan in y-dimension
     */
    public void pan(float dx, float dy) {
        final float aspectQuotient = mAspectQuotient.get();

        mState.setPanX(mState.getPanX() + dx / mState.getZoomX(aspectQuotient));
        mState.setPanY(mState.getPanY() + dy / mState.getZoomY(aspectQuotient));

        limitPan();

        mState.notifyObservers();
    }

    /**
     * Help function to figure out max delta of pan from center position.
     * 
     * @param zoom Zoom value
     * @return Max delta of pan
     */
    private float getMaxPanDelta(float zoom) {
        return Math.max(0f, .5f * ((zoom - 1) / zoom));
    }

    /**
     * Force zoom to stay within limits
     */
    private void limitZoom() {
        if (mState.getZoom() < MIN_ZOOM) {
            mState.setZoom(MIN_ZOOM);
        } else if (mState.getZoom() > MAX_ZOOM) {
            mState.setZoom(MAX_ZOOM);
        }
    }

    /**
     * Force pan to stay within limits
     */
    private void limitPan() {
        final float aspectQuotient = mAspectQuotient.get();

        final float zoomX = mState.getZoomX(aspectQuotient);
        final float zoomY = mState.getZoomY(aspectQuotient);

        final float panMinX = .5f - getMaxPanDelta(zoomX);
        final float panMaxX = .5f + getMaxPanDelta(zoomX);
        final float panMinY = .5f - getMaxPanDelta(zoomY);
        final float panMaxY = .5f + getMaxPanDelta(zoomY);

        if (mState.getPanX() < panMinX) {
            mState.setPanX(panMinX);
        }
        if (mState.getPanX() > panMaxX) {
            mState.setPanX(panMaxX);
        }
        if (mState.getPanY() < panMinY) {
            mState.setPanY(panMinY);
        }
        if (mState.getPanY() > panMaxY) {
            mState.setPanY(panMaxY);
        }
    }

    // Observable interface implementation

    public void update(Observable observable, Object data) {
        limitZoom();
        limitPan();
    }

}
