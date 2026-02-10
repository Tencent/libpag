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

import { BackendContext } from './backend-context';
import type { PAGX } from './types';

export type WxCanvas = (HTMLCanvasElement | OffscreenCanvas) & {
  requestAnimationFrame?: (callback: () => void) => number;
  cancelAnimationFrame?: (requestID: number) => void;
};

const renderCanvasList: RenderCanvas[] = [];

/**
 * RenderCanvas manages Canvas and WebGL context lifecycle.
 */
export class RenderCanvas {
  /**
   * Get or create RenderCanvas from canvas object.
   * Reuses existing RenderCanvas if the same canvas is provided.
   */
  public static from(module: PAGX, canvas: WxCanvas): RenderCanvas {
    let renderCanvas = renderCanvasList.find((rc) => rc.canvas === canvas);
    if (renderCanvas) {
      return renderCanvas;
    }
    renderCanvas = new RenderCanvas(module, canvas);
    renderCanvasList.push(renderCanvas);
    return renderCanvas;
  }

  private _canvas: WxCanvas | null = null;
  private _backendContext: BackendContext | null = null;
  private retainCount = 0;
  private module: PAGX;

  private constructor(module: PAGX, canvas: WxCanvas) {
    this.module = module;
    this._canvas = canvas;

    const contextAttributes: WebGLContextAttributes = {
      alpha: true,
      depth: false,
      stencil: false,
      antialias: true,
      powerPreference: 'high-performance',
      preserveDrawingBuffer: false,
      failIfMajorPerformanceCaveat: false,
    };

    // Try standard WebGL 2 context first
    let gl = canvas.getContext('webgl2', contextAttributes) as WebGL2RenderingContext | null;

    if (!gl) {
      throw new Error(
        'Failed to get WebGL 2 context from canvas. ' +
          'Please ensure your browser/environment supports WebGL 2.',
      );
    }

    // Register context to Emscripten
    this._backendContext = BackendContext.from(module, gl);
  }

  /**
   * Increase retain count.
   */
  public retain(): void {
    this.retainCount += 1;
  }

  /**
   * Decrease retain count and release resources when count reaches zero.
   */
  public release(): void {
    this.retainCount -= 1;
    if (this.retainCount === 0) {
      // Remove from global list
      const index = renderCanvasList.indexOf(this);
      if (index >= 0) {
        renderCanvasList.splice(index, 1);
      }

      // Clean up
      if (this._backendContext) {
        this._backendContext.destroy(this.module);
        this._backendContext = null;
      }
      this._canvas = null;
    }
  }

  public get canvas(): WxCanvas | null {
    return this._canvas;
  }

  public get backendContext(): BackendContext | null {
    return this._backendContext;
  }
}
