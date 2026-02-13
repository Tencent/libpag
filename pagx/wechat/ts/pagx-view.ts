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

import { RenderCanvas, WxCanvas } from './render-canvas';
import { BackendContext } from './backend-context';
import { destroyVerify } from './decorators';
import type { PAGX, PAGXViewNative } from './types';
import type { wx } from './interfaces';

declare const wx: wx;

export interface PAGXViewOptions {
  /**
   * Auto start rendering loop. default false.
   * When true, the view will automatically start rendering after initialization.
   */
  autoRender?: boolean;
  /**
   * Custom render callback. Called before each frame is drawn.
   * Can be used for performance monitoring or custom rendering logic.
   */
  onBeforeRender?: () => void;
  /**
   * Custom render callback. Called after each frame is drawn.
   * Can be used for performance monitoring or custom rendering logic.
   */
  onAfterRender?: () => void;
  /**
   * Called once when the first frame is rendered successfully.
   * Useful for hiding loading indicators after content becomes visible.
   */
  onFirstFrame?: () => void;
}

/**
 * PAGXView for WeChat MiniProgram.
 * Manages canvas, WebGL context, and C++ PAGXViewWechat instance.
 */
@destroyVerify
export class View {
  /**
   * Create PAGXView.
   * @param module PAGX module instance
   * @param canvas WeChat canvas object
   * @param options PAGXView options
   */
  public static async init(
    module: PAGX,
    canvas: WxCanvas,
    options: PAGXViewOptions = {}
  ): Promise<View> {
    const view = new View(module, canvas);
    view.pagViewOptions = { ...view.pagViewOptions, ...options };

    // Create RenderCanvas
    view.renderCanvas = RenderCanvas.from(module, canvas);
    view.renderCanvas.retain();
    view.backendContext = view.renderCanvas.backendContext;

    if (!view.backendContext) {
      throw new Error('Failed to create backend context');
    }

    // Make context current
    if (!view.backendContext.makeCurrent(module)) {
      throw new Error('Failed to make context current');
    }

    // Create C++ PAGXViewWechat instance
    view.nativeView = module.PAGXView.MakeFrom(canvas.width, canvas.height);

    view.backendContext.clearCurrent(module);

    if (!view.nativeView) {
      throw new Error('Failed to create PAGXViewWechat');
    }

    if (view.pagViewOptions.autoRender) {
      view.startRendering();
    }

    return view;
  }

  private module: PAGX;
  private nativeView: PAGXViewNative | null = null;
  private renderCanvas: RenderCanvas | null = null;
  private backendContext: BackendContext | null = null;
  private canvas: WxCanvas | null = null;
  private pagViewOptions: PAGXViewOptions = {
    autoRender: false,
  };
  private isDestroyed = false;
  private isRendering = false;
  private firstFrameCallbackFired = false;
  private animationFrameId: number = 0;

  private constructor(module: PAGX, canvas: WxCanvas) {
    this.module = module;
    this.canvas = canvas;
  }

  /**
   * Register fallback fonts for text rendering.
   * Must be called before loadPAGX() to ensure fonts are available during layout.
   * @param fontData Font file data (e.g. NotoSansSC-Regular.otf) as Uint8Array.
   * @param emojiFontData Emoji font file data (e.g. NotoColorEmoji.ttf) as Uint8Array.
   */
  public registerFonts(fontData: Uint8Array, emojiFontData: Uint8Array): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.backendContext!.makeCurrent(this.module);
    this.nativeView.registerFonts(fontData, emojiFontData);
    this.backendContext!.clearCurrent(this.module);
  }

  /**
   * Load PAGX file data.
   * @param pagxData PAGX file data
   */
  public loadPAGX(pagxData: Uint8Array): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.firstFrameCallbackFired = false;
    this.nativeView.loadPAGX(pagxData);
  }

  /**
   * Update canvas size.
   * @param width New width
   * @param height New height
   */
  public updateSize(width?: number, height?: number): void {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }

    if (width !== undefined && height !== undefined) {
      this.canvas.width = width;
      this.canvas.height = height;
    }

    if (!this.backendContext) {
      return;
    }

    this.backendContext.makeCurrent(this.module);
    this.nativeView!.updateSize(this.canvas.width, this.canvas.height);
    this.backendContext.clearCurrent(this.module);
  }

  /**
   * Update zoom scale and content offset for display list.
   * @param zoom Zoom scale
   * @param offsetX X offset
   * @param offsetY Y offset
   */
  public updateZoomScaleAndOffset(zoom: number, offsetX: number, offsetY: number): void {
    this.nativeView!.updateZoomScaleAndOffset(zoom, offsetX, offsetY);
  }

  /**
   * Notify that zoom gesture has ended (for adaptive tile refinement).
   * Note: This is now handled automatically by PAGXView internal detection.
   * No need to call manually unless you want to force trigger.
   * @deprecated Automatic detection is enabled, manual call is optional.
   */
  public onZoomEnd(): void {
    this.nativeView!.onZoomEnd();
  }

  /**
   * Get content width.
   */
  public contentWidth(): number {
    return this.nativeView!.contentWidth();
  }

  /**
   * Get content height.
   */
  public contentHeight(): number {
    return this.nativeView!.contentHeight();
  }

  /**
   * Returns true if the first frame has been rendered successfully.
   */
  public isFirstFrameRendered(): boolean {
    return this.nativeView!.firstFrameRendered();
  }

  /**
   * Start the rendering loop.
   * This will continuously call draw() using requestAnimationFrame.
   */
  public startRendering(): void {
    if (this.isRendering) {
      return;
    }
    this.isRendering = true;
    this.renderLoop();
  }

  /**
   * Stop the rendering loop.
   */
  public stopRendering(): void {
    if (!this.isRendering) {
      return;
    }
    this.isRendering = false;
    if (this.animationFrameId) {
      if (this.canvas && (this.canvas as any).cancelAnimationFrame) {
        (this.canvas as any).cancelAnimationFrame(this.animationFrameId);
      } else {
        clearTimeout(this.animationFrameId);
      }
      this.animationFrameId = 0;
    }
  }

  /**
   * Check if the view is currently rendering.
   */
  public isCurrentlyRendering(): boolean {
    return this.isRendering;
  }

  /**
   * Set render callback functions for performance monitoring.
   */
  public setRenderCallbacks(onBeforeRender?: () => void, onAfterRender?: () => void): void {
    this.pagViewOptions.onBeforeRender = onBeforeRender;
    this.pagViewOptions.onAfterRender = onAfterRender;
  }

  /**
   * Destroy PAGXView and release all resources.
   */
  public destroy(): void {
    if (this.isDestroyed) {
      return;
    }

    this.stopRendering();

    if (this.nativeView) {
      if (this.backendContext) {
        this.backendContext.makeCurrent(this.module);
      }
      this.nativeView.delete();
      this.nativeView = null;
      if (this.backendContext) {
        this.backendContext.clearCurrent(this.module);
      }
    }

    if (this.renderCanvas) {
      this.renderCanvas.release();
      this.renderCanvas = null;
    }

    this.backendContext = null;
    this.canvas = null;
    this.isDestroyed = true;
  }

  private resetSize(): void {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }

    // Calculate display size for WeChat MiniProgram
    const displayWidth = (this.canvas as any).displayWidth || this.canvas.width;
    const displayHeight = (this.canvas as any).displayHeight || this.canvas.height;
    const dpr = wx.getSystemInfoSync().pixelRatio;

    this.canvas.width = displayWidth * dpr;
    this.canvas.height = displayHeight * dpr;
  }

  private renderLoop(): void {
    if (!this.isRendering || this.isDestroyed) {
      return;
    }

    if (!this.backendContext) {
      return;
    }

    if (this.pagViewOptions.onBeforeRender) {
      this.pagViewOptions.onBeforeRender();
    }

    this.backendContext.makeCurrent(this.module);
    this.nativeView!.draw();
    this.backendContext.clearCurrent(this.module);

    if (!this.firstFrameCallbackFired && this.nativeView!.firstFrameRendered()) {
      this.firstFrameCallbackFired = true;
      if (this.pagViewOptions.onFirstFrame) {
        this.pagViewOptions.onFirstFrame();
      }
    }

    if (this.pagViewOptions.onAfterRender) {
      this.pagViewOptions.onAfterRender();
    }

    if (this.canvas && (this.canvas as any).requestAnimationFrame) {
      this.animationFrameId = (this.canvas as any).requestAnimationFrame(() => {
        this.renderLoop();
      });
    } else {
      this.animationFrameId = setTimeout(() => {
        this.renderLoop();
      }, 16) as any;
    }
  }
}
