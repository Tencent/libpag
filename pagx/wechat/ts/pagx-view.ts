/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
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
import type { PAGX, PAGXViewNative } from './types';
import type { wx } from './interfaces';

declare const wx: wx;

export interface PAGXViewOptions {
  /**
   * Use style to scale canvas. default false.
   * When target canvas is offscreen canvas, useScale is false.
   */
  useScale?: boolean;
  /**
   * Render first frame when view init. default true.
   */
  firstFrame?: boolean;
}

/**
 * PAGXView for WeChat MiniProgram.
 * Manages canvas, WebGL context, and C++ PAGXViewWechat instance.
 */
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

    // Reset canvas size if needed
    view.resetSize(view.pagViewOptions.useScale);

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

    return view;
  }

  private module: PAGX;
  private nativeView: PAGXViewNative | null = null;
  private renderCanvas: RenderCanvas | null = null;
  private backendContext: BackendContext | null = null;
  private canvas: WxCanvas | null = null;
  private pagViewOptions: PAGXViewOptions = {
    useScale: false,
    firstFrame: true,
  };
  private isDestroyed = false;

  private constructor(module: PAGX, canvas: WxCanvas) {
    this.module = module;
    this.canvas = canvas;
  }

  /**
   * Load PAGX file data.
   * @param pagxData PAGX file data
   */
  public loadPAGX(pagxData: Uint8Array): void {
    this.checkDestroyed();
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.nativeView.loadPAGX(pagxData);
  }

  /**
   * Update canvas size.
   * @param width New width
   * @param height New height
   */
  public updateSize(width?: number, height?: number): void {
    this.checkDestroyed();

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
    this.checkDestroyed();
    this.nativeView!.updateZoomScaleAndOffset(zoom, offsetX, offsetY);
  }

  /**
   * Notify that zoom gesture has ended (for adaptive tile refinement).
   */
  public onZoomEnd(): void {
    this.checkDestroyed();
    this.nativeView!.onZoomEnd();
  }

  /**
   * Draw current frame.
   */
  public draw(): void {
    this.checkDestroyed();

    if (!this.backendContext) {
      return;
    }

    this.backendContext.makeCurrent(this.module);
    this.nativeView!.draw();
    this.backendContext.clearCurrent(this.module);
  }

  /**
   * Get content width.
   */
  public contentWidth(): number {
    this.checkDestroyed();
    return this.nativeView!.contentWidth();
  }

  /**
   * Get content height.
   */
  public contentHeight(): number {
    this.checkDestroyed();
    return this.nativeView!.contentHeight();
  }

  /**
   * Destroy PAGXView and release all resources.
   */
  public destroy(): void {
    if (this.isDestroyed) {
      return;
    }

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

  private resetSize(useScale = false): void {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }

    if (!useScale) {
      return;
    }

    // Calculate display size for WeChat MiniProgram
    const displayWidth = (this.canvas as any).displayWidth || this.canvas.width;
    const displayHeight = (this.canvas as any).displayHeight || this.canvas.height;
    const dpr = wx.getSystemInfoSync().pixelRatio;

    this.canvas.width = displayWidth * dpr;
    this.canvas.height = displayHeight * dpr;
  }

  private checkDestroyed(): void {
    if (this.isDestroyed) {
      throw new Error('PAGXView has been destroyed');
    }
  }
}
