/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

const TAP_TIMEOUT = 300;
const TAP_DISTANCE_THRESHOLD = 50;
const ZOOM_STABLE_THRESHOLD_MS = 200;

export interface GestureState {
  action: 'none' | 'update' | 'reset' | 'pan' | 'zoom' | 'zoomEnd';
  zoom: number;
  offsetX: number;
  offsetY: number;
  isZooming?: boolean;
}

type EventListener = (data: GestureState) => void;

export class WXGestureManager {
  // Current state
  private zoom: number = 1.0;
  private offsetX: number = 0;  // Physical pixel offset passed to C++ DisplayList
  private offsetY: number = 0;
  
  // Canvas dimensions in physical pixels
  private canvasWidth: number = 0;   // physical pixels (rect.width * dpr)
  private canvasHeight: number = 0;  // physical pixels (rect.height * dpr)
  
  // Single-finger drag state
  private lastTouchX: number = 0;
  private lastTouchY: number = 0;
  
  // Pinch zoom state
  private initialDistance: number = 0;
  private initialZoom: number = 1.0;
  
  // Double-tap detection
  private lastTapTime: number = 0;
  private lastTapX: number = 0;
  private lastTapY: number = 0;
  
  // Zoom stability tracking
  private lastZoomChangeTime: number = 0;
  private zoomStableCheckTimer: number | null = null;
  private isZooming: boolean = false;
  
  // Event listeners
  private listeners: Record<string, EventListener[]> = {};
  
  /**
   * Initialize gesture manager with canvas and content dimensions.
   * Must be called after loading PAGX file.
   * 
   * NOTE: This method resets all transforms (zoom and offsets) to initial state.
   * Returns the reset state that should be applied to the view.
   * 
   * @param canvasWidth - Canvas width in physical pixels (rect.width * dpr)
   * @param canvasHeight - Canvas height in physical pixels (rect.height * dpr)
   * @param contentWidth - PAGX content width in content pixels
   * @param contentHeight - PAGX content height in content pixels
   */
  public init(
    canvasWidth: number,
    canvasHeight: number,
    contentWidth: number,
    contentHeight: number
  ): GestureState | null {
    if (canvasWidth <= 0 || canvasHeight <= 0 || 
        contentWidth <= 0 || contentHeight <= 0) {
      return null;
    }
    
    this.canvasWidth = canvasWidth;
    this.canvasHeight = canvasHeight;
    
    // Reset all transforms when (re)initializing
    this.zoom = 1.0;
    this.offsetX = 0;
    this.offsetY = 0;
    
    // Reset zoom state
    this.isZooming = false;
    this.lastZoomChangeTime = 0;
    
    return {
      action: 'reset',
      zoom: this.zoom,
      offsetX: this.offsetX,
      offsetY: this.offsetY,
      isZooming: false
    };
  }
  
  /**
   * Update canvas size (e.g., after orientation change or window resize).
   */
  public updateSize(
    canvasWidth: number,
    canvasHeight: number,
    contentWidth: number,
    contentHeight: number
  ): void {
    this.canvasWidth = canvasWidth;
    this.canvasHeight = canvasHeight;
  }
  
  /**
   * Get current gesture state.
   */
  public getState(): GestureState {
    return {
      action: 'none',
      zoom: this.zoom,
      offsetX: this.offsetX,
      offsetY: this.offsetY,
      isZooming: this.isZooming
    };
  }
  
  /**
   * Reset to initial centered state.
   */
  public reset(): GestureState {
    this.zoom = 1.0;
    this.offsetX = 0;
    this.offsetY = 0;
    
    // Clear zoom state
    this.isZooming = false;
    this.lastZoomChangeTime = 0;
    this.clearZoomStableTimer();
    
    return {
      action: 'reset',
      zoom: this.zoom,
      offsetX: this.offsetX,
      offsetY: this.offsetY,
      isZooming: false
    };
  }
  
  /**
   * Handle touch start event.
   * @param touches - Touch array from WeChat event (coordinates in logical pixels)
   * @param dpr - Device pixel ratio for converting to physical pixels
   */
  public onTouchStart(touches: any[], dpr: number): GestureState {
    if (touches.length === 1) {
      // Single finger: prepare for drag
      this.lastTouchX = touches[0].x;
      this.lastTouchY = touches[0].y;
      
      // Double-tap detection (in logical pixels)
      const now = Date.now();
      const distance = Math.hypot(
        touches[0].x - this.lastTapX,
        touches[0].y - this.lastTapY
      );
      
      if (now - this.lastTapTime < TAP_TIMEOUT && 
          distance < TAP_DISTANCE_THRESHOLD) {
        this.lastTapTime = 0;
        return this.reset();
      }
      
      this.lastTapX = touches[0].x;
      this.lastTapY = touches[0].y;
      this.lastTapTime = now;
      
    } else if (touches.length === 2) {
      // Two fingers: prepare for pinch zoom
      const dx = touches[1].x - touches[0].x;
      const dy = touches[1].y - touches[0].y;
      this.initialDistance = Math.hypot(dx, dy);
      this.initialZoom = this.zoom;
      
      // Mark zoom started
      if (!this.isZooming) {
        this.isZooming = true;
        this.emit('zoomStart', this.getState());
      }
    }
    
    return this.getState();
  }
  
  /**
   * Handle touch move event.
   * @param touches - Touch array from WeChat event (coordinates in logical pixels)
   * @param dpr - Device pixel ratio for converting to physical pixels
   */
  public onTouchMove(touches: any[], dpr: number): GestureState {
    if (touches.length === 1) {
      // Single-finger drag
      // Guard against finger count transition: if we just went from 2+ fingers to 1,
      // reset lastTouch to current position to avoid delta spike
      if (this.initialDistance > 0) {
        // We were in pinch mode, now transitioning to drag
        this.lastTouchX = touches[0].x;
        this.lastTouchY = touches[0].y;
        this.initialDistance = 0;
        return this.getState();
      }
      
      // Convert logical pixel delta to physical pixel delta
      const deltaX = (touches[0].x - this.lastTouchX) * dpr;
      const deltaY = (touches[0].y - this.lastTouchY) * dpr;
      
      // Accumulate offset directly in physical pixels
      this.offsetX += deltaX;
      this.offsetY += deltaY;
      
      this.lastTouchX = touches[0].x;
      this.lastTouchY = touches[0].y;
      
      return {
        action: 'pan',
        zoom: this.zoom,
        offsetX: this.offsetX,
        offsetY: this.offsetY,
        isZooming: false
      };
      
    } else if (touches.length === 2) {
      // Pinch zoom
      // Ensure initialized (guard against gesture transition)
      if (this.initialDistance <= 0) {
        return this.getState();
      }
      
      const dx = touches[1].x - touches[0].x;
      const dy = touches[1].y - touches[0].y;
      const currentDistance = Math.hypot(dx, dy);
      
      const scaleChange = currentDistance / this.initialDistance;
      const oldZoom = this.zoom;
      const newZoom = this.initialZoom * scaleChange;
      
      // Calculate current pinch center (may have moved during zoom)
      const currentPinchCenterX = (touches[0].x + touches[1].x) * 0.5 * dpr;
      const currentPinchCenterY = (touches[0].y + touches[1].y) * 0.5 * dpr;
      
      // Standard zoom-around-point formula using CURRENT zoom (not initial):
      // This allows for smooth continuous zooming even if fingers move
      // offset_new = (offset_old - focal) * (zoom_new / zoom_old) + focal
      this.offsetX = (this.offsetX - currentPinchCenterX) * (newZoom / this.zoom) + currentPinchCenterX;
      this.offsetY = (this.offsetY - currentPinchCenterY) * (newZoom / this.zoom) + currentPinchCenterY;
      this.zoom = newZoom;
      
      // Track zoom change and setup stability check
      if (Math.abs(newZoom - oldZoom) > 0.001) {
        this.lastZoomChangeTime = Date.now();
        
        // Clear existing timer
        this.clearZoomStableTimer();
        
        // Start new timer to check stability
        this.zoomStableCheckTimer = setTimeout(() => {
          this.checkZoomStability();
        }, ZOOM_STABLE_THRESHOLD_MS) as any;
      }
      
      return {
        action: 'zoom',
        zoom: this.zoom,
        offsetX: this.offsetX,
        offsetY: this.offsetY,
        isZooming: true
      };
    }
    
    return this.getState();
  }
  
  /**
   * Handle touch end event.
   * @param remainingTouches - Array of touches that are still on screen (e.touches, not e.changedTouches)
   */
  public onTouchEnd(remainingTouches: any[]): GestureState {
    // Handle zoom end on finger lift
    if (remainingTouches.length < 2 && this.isZooming) {
      this.clearZoomStableTimer();
      this.isZooming = false;
      this.lastZoomChangeTime = 0;
      
      // Emit zoomEnd event
      this.emit('zoomEnd', {
        action: 'zoomEnd',
        zoom: this.zoom,
        offsetX: this.offsetX,
        offsetY: this.offsetY,
        isZooming: false
      });
    }
    
    // Reset drag state if all fingers lifted
    if (remainingTouches.length === 0) {
      this.lastTouchX = 0;
      this.lastTouchY = 0;
      this.initialDistance = 0;
    }
    return this.getState();
  }
  
  /**
   * Check if zoom has stabilized.
   */
  private checkZoomStability(): void {
    const now = Date.now();
    const timeSinceLastChange = now - this.lastZoomChangeTime;
    
    if (timeSinceLastChange >= ZOOM_STABLE_THRESHOLD_MS && this.isZooming) {
      this.isZooming = false;
      
      this.emit('zoomEnd', {
        action: 'zoomEnd',
        zoom: this.zoom,
        offsetX: this.offsetX,
        offsetY: this.offsetY,
        isZooming: false
      });
    }
  }
  
  /**
   * Clear stability check timer.
   */
  private clearZoomStableTimer(): void {
    if (this.zoomStableCheckTimer !== null) {
      clearTimeout(this.zoomStableCheckTimer);
      this.zoomStableCheckTimer = null;
    }
  }
  
  /**
   * Register event listener.
   */
  public on(event: string, callback: EventListener): void {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event].push(callback);
  }
  
  /**
   * Unregister event listener.
   */
  public off(event: string, callback: EventListener): void {
    if (!this.listeners[event]) return;
    const index = this.listeners[event].indexOf(callback);
    if (index > -1) {
      this.listeners[event].splice(index, 1);
    }
  }
  
  /**
   * Emit event to all listeners.
   */
  private emit(event: string, data: GestureState): void {
    if (!this.listeners[event]) return;
    this.listeners[event].forEach(callback => {
      try {
        callback(data);
      } catch (error) {
        // Silently ignore errors
      }
    });
  }
  
  /**
   * Cleanup resources and remove all listeners.
   */
  public destroy(): void {
    this.clearZoomStableTimer();
    this.listeners = {};
  }
}
