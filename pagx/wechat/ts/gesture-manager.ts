/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

const MIN_ZOOM = 0.1;
const MAX_ZOOM = 10.0;
const TAP_TIMEOUT = 300;
const TAP_DISTANCE_THRESHOLD = 50;

// Offset limits to prevent content from moving too far outside viewport
// These are in content space units (larger values allow more pan freedom)
const MAX_OFFSET_MULTIPLIER = 5.0;  // Allow panning up to 5x content size in each direction

export interface GestureState {
  action: 'none' | 'update' | 'reset';
  zoom: number;
  offsetX: number;
  offsetY: number;
}

export class WXGestureManager {
  // Current state
  private zoom: number = 1.0;
  private offsetX: number = 0;  // offset passed to C++: offset = contentOffset / zoom
  private offsetY: number = 0;
  
  // Content space offset (physical pixels / canvasToContentScale)
  // This represents the offset in content coordinate space, maintaining visual position
  private contentOffsetX: number = 0;
  private contentOffsetY: number = 0;
  
  // Content dimensions for offset limiting
  private contentWidth: number = 0;
  private contentHeight: number = 0;
  
  // Canvas dimensions in physical pixels and scale factor
  private canvasWidth: number = 0;   // physical pixels (rect.width * dpr)
  private canvasHeight: number = 0;  // physical pixels (rect.height * dpr)
  private canvasToContentScale: number = 1.0;  // canvasPixels / contentPixels
  
  // Single-finger drag state
  private lastTouchX: number = 0;
  private lastTouchY: number = 0;
  
  // Pinch zoom state
  private initialDistance: number = 0;
  private initialZoom: number = 1.0;
  private initialContentOffsetX: number = 0;
  private initialContentOffsetY: number = 0;
  private pinchCenterX: number = 0;  // physical pixels
  private pinchCenterY: number = 0;  // physical pixels
  
  // Double-tap detection
  private lastTapTime: number = 0;
  private lastTapX: number = 0;
  private lastTapY: number = 0;
  
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
      console.error(
        `Invalid dimensions: canvas=${canvasWidth}x${canvasHeight}, ` +
        `content=${contentWidth}x${contentHeight}`
      );
      return null;
    }
    
    this.canvasWidth = canvasWidth;
    this.canvasHeight = canvasHeight;
    this.contentWidth = contentWidth;
    this.contentHeight = contentHeight;
    this.canvasToContentScale = Math.min(
      canvasWidth / contentWidth,
      canvasHeight / contentHeight
    );
    
    // Reset all transforms when (re)initializing
    this.zoom = 1.0;
    this.offsetX = 0;
    this.offsetY = 0;
    this.contentOffsetX = 0;
    this.contentOffsetY = 0;
    
    return {
      action: 'reset',
      zoom: this.zoom,
      offsetX: this.offsetX,
      offsetY: this.offsetY
    };
  }
  
  /**
   * Update canvas size (e.g., after orientation change or window resize).
   * 
   * NOTE: When canvas size changes, canvasToContentScale and centerOffset both change,
   * making it impossible to maintain the exact visual position. The simplest
   * and most predictable approach is to reset all transforms.
   */
  public updateSize(
    canvasWidth: number,
    canvasHeight: number,
    contentWidth: number,
    contentHeight: number
  ): void {
    const oldScale = this.canvasToContentScale;
    
    this.canvasWidth = canvasWidth;
    this.canvasHeight = canvasHeight;
    this.contentWidth = contentWidth;
    this.contentHeight = contentHeight;
    this.canvasToContentScale = Math.min(
      canvasWidth / contentWidth,
      canvasHeight / contentHeight
    );
    
    if (oldScale > 0 && Math.abs(oldScale - this.canvasToContentScale) > 0.001) {
      // Reset all transforms when scale changes
      this.zoom = 1.0;
      this.offsetX = 0;
      this.offsetY = 0;
      this.contentOffsetX = 0;
      this.contentOffsetY = 0;
    }
  }
  
  /**
   * Get current gesture state.
   */
  public getState(): GestureState {
    return {
      action: 'none',
      zoom: this.zoom,
      offsetX: this.offsetX,
      offsetY: this.offsetY
    };
  }
  
  /**
   * Reset to initial centered state.
   */
  public reset(): GestureState {
    this.zoom = 1.0;
    this.offsetX = 0;
    this.offsetY = 0;
    this.contentOffsetX = 0;
    this.contentOffsetY = 0;
    return {
      action: 'reset',
      zoom: this.zoom,
      offsetX: this.offsetX,
      offsetY: this.offsetY
    };
  }
  
  /**
   * Clamp content offset to prevent extreme values that cause rendering issues.
   * Limits offset to a reasonable range based on content size.
   */
  private clampContentOffset(): void {
    if (this.contentWidth <= 0 || this.contentHeight <= 0) {
      return;
    }
    
    // Allow panning up to MAX_OFFSET_MULTIPLIER times the content size
    const maxOffsetX = this.contentWidth * MAX_OFFSET_MULTIPLIER;
    const maxOffsetY = this.contentHeight * MAX_OFFSET_MULTIPLIER;
    
    this.contentOffsetX = Math.max(-maxOffsetX, Math.min(maxOffsetX, this.contentOffsetX));
    this.contentOffsetY = Math.max(-maxOffsetY, Math.min(maxOffsetY, this.contentOffsetY));
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
      
      // IMPORTANT: Save current content offsets before zoom starts
      this.initialContentOffsetX = this.contentOffsetX;
      this.initialContentOffsetY = this.contentOffsetY;
      
      // Record pinch center in physical pixels (touches are in logical pixels)
      this.pinchCenterX = (touches[0].x + touches[1].x) * 0.5 * dpr;
      this.pinchCenterY = (touches[0].y + touches[1].y) * 0.5 * dpr;
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
      
      // Accumulate content space offset
      // deltaX is in physical pixels, canvasToContentScale is physicalPixels/contentPixels
      // so deltaX / canvasToContentScale gives us content space units
      if (this.canvasToContentScale > 0) {
        this.contentOffsetX += deltaX / this.canvasToContentScale;
        this.contentOffsetY += deltaY / this.canvasToContentScale;
      }
      
      // Clamp offset to prevent extreme values
      this.clampContentOffset();
      
      // Convert to C++ offset space: offset = contentOffset / zoom
      if (this.zoom > 0) {
        this.offsetX = this.contentOffsetX / this.zoom;
        this.offsetY = this.contentOffsetY / this.zoom;
      }
      
      this.lastTouchX = touches[0].x;
      this.lastTouchY = touches[0].y;
      
      return {
        action: 'update',
        zoom: this.zoom,
        offsetX: this.offsetX,
        offsetY: this.offsetY
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
      const newZoom = Math.max(
        MIN_ZOOM,
        Math.min(MAX_ZOOM, this.initialZoom * scaleChange)
      );
      
      // Focal point calculation in content space:
      // Canvas position (physical pixels): C = centerOffset + canvasToContentScale * zoom * (offset + P)
      // where P is point in content space
      // To find P: P = (C - centerOffset) / (canvasToContentScale * zoom) - offset
      //             = (C - centerOffset) / canvasToContentScale / zoom - contentOffset / zoom
      //             = ((C - centerOffset) / canvasToContentScale - contentOffset) / zoom
      // At zoom start: P = ((pinchCenter - center) / canvasToContentScale - initialContentOffset) / initialZoom
      
      // Convert focal point from physical pixels to content space
      if (this.canvasToContentScale > 0 && this.initialZoom > 0) {
        const focalContent = (this.pinchCenterX - this.canvasWidth * 0.5) / this.canvasToContentScale;
        const focalContentY = (this.pinchCenterY - this.canvasHeight * 0.5) / this.canvasToContentScale;
        
        // Content point at focal: P = (focalContent - initialContentOffset) / initialZoom
        // Keep P fixed: focalContent - initialContentOffset = P * initialZoom
        //               newContentOffset = focalContent - P * newZoom
        //                                = focalContent - (focalContent - initialContentOffset) * (newZoom / initialZoom)
        this.contentOffsetX = focalContent - (focalContent - this.initialContentOffsetX) * (newZoom / this.initialZoom);
        this.contentOffsetY = focalContentY - (focalContentY - this.initialContentOffsetY) * (newZoom / this.initialZoom);
      }
      
      // Clamp offset to prevent extreme values
      this.clampContentOffset();
      
      this.zoom = newZoom;
      if (this.zoom > 0) {
        this.offsetX = this.contentOffsetX / this.zoom;
        this.offsetY = this.contentOffsetY / this.zoom;
      }
      
      return {
        action: 'update',
        zoom: this.zoom,
        offsetX: this.offsetX,
        offsetY: this.offsetY
      };
    }
    
    return this.getState();
  }
  
  /**
   * Handle touch end event.
   * @param remainingTouches - Array of touches that are still on screen (e.touches, not e.changedTouches)
   */
  public onTouchEnd(remainingTouches: any[]): GestureState {
    // Reset drag state if all fingers lifted
    if (remainingTouches.length === 0) {
      this.lastTouchX = 0;
      this.lastTouchY = 0;
      this.initialDistance = 0;
    }
    return this.getState();
  }
}
