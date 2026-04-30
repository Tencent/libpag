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

import type { _PAGXView } from './types';
import { getPAGXModule } from './pagx-module';
import { destroyVerify } from './decorators';

/**
 * PAGXView wraps the native PAGXView and manages the render loop.
 *
 * @example
 * ```typescript
 * const PAGX = await PAGXInit({ locateFile: () => '/wasm/pagx-viewer.wasm' });
 * const view = PAGX.PAGXView.init('#canvas');
 *
 * view.registerFonts(fontData, emojiFontData);
 * view.parsePAGX(pagxData);
 * view.buildLayers();
 * view.updateSize();
 *
 * // Start render loop
 * view.start();
 *
 * // Update zoom/pan
 * view.updateZoomScaleAndOffset(2.0, 100, 50);
 *
 * // Stop and cleanup
 * view.stop();
 * view.destroy();
 * ```
 */
@destroyVerify
export class PAGXView {
  /**
   * Creates a PAGXView from a canvas element or a CSS selector.
   * @param canvas Either a CSS selector string (e.g., `'#my-canvas'`) or an `HTMLCanvasElement`.
   *               When an element is passed it must have a non-empty `id` attribute; the id is
   *               used to build the CSS selector required by the underlying WebGL binding.
   * @returns PAGXView instance or null if creation failed
   */
  public static init(canvas: string | HTMLCanvasElement): PAGXView | null {
    const module = getPAGXModule();
    if (!module) {
      console.error('PAGXView: Module not initialized. Call PAGXInit() first.');
      return null;
    }

    let canvasID: string;
    if (typeof canvas === 'string') {
      if (!canvas) {
        console.error('PAGXView: canvas selector must be a non-empty string.');
        return null;
      }
      canvasID = canvas;
    } else {
      if (!canvas.id) {
        console.error('PAGXView: Canvas element must have a non-empty id attribute.');
        return null;
      }
      canvasID = `#${canvas.id}`;
    }

    const nativeView = module._PAGXView._MakeFrom(canvasID);
    if (!nativeView) {
      console.error(`PAGXView: Failed to create PAGXView from canvas "${canvasID}".`);
      return null;
    }

    return new PAGXView(nativeView);
  }

  /**
   * Whether the render loop is running.
   */
  public get isRunning(): boolean {
    return this._isRunning;
  }

  /**
   * Whether the view has been destroyed.
   */
  public get isDestroyed(): boolean {
    return this._isDestroyed;
  }

  /**
   * The original width of the PAGX content.
   */
  public get contentWidth(): number {
    return this.nativeView._contentWidth();
  }

  /**
   * The original height of the PAGX content.
   */
  public get contentHeight(): number {
    return this.nativeView._contentHeight();
  }

  private nativeView: _PAGXView;
  private _isRunning = false;
  private _isDestroyed = false;
  private animationFrameId: number | null = null;

  private constructor(nativeView: _PAGXView) {
    this.nativeView = nativeView;
  }

  /**
   * Registers fallback fonts for text rendering.
   * @param fontData Primary font data (e.g., NotoSansSC)
   * @param emojiFontData Emoji font data (e.g., NotoColorEmoji)
   */
  public registerFonts(fontData: Uint8Array, emojiFontData: Uint8Array): void {
    this.nativeView._registerFonts(fontData, emojiFontData);
  }

  /**
   * Loads a PAGX document (no external files).
   * @param data PAGX file binary data
   */
  public loadPAGX(data: Uint8Array): void {
    this.nativeView._loadPAGX(data);
  }

  /**
   * Clears the currently loaded PAGX content and releases associated resources. After this call
   * the view will render as empty until a new document is loaded.
   */
  public clear(): void {
    this.nativeView._loadPAGX(new Uint8Array(0));
  }

  /**
   * Parses PAGX data without building layers.
   * Call getExternalFilePaths() and loadFileData() after this to load external resources,
   * then call buildLayers() to complete the loading.
   * @param data PAGX file binary data
   */
  public parsePAGX(data: Uint8Array): void {
    this.nativeView._parsePAGX(data);
  }

  /**
   * Returns the list of external file paths referenced by the PAGX document.
   */
  public getExternalFilePaths(): string[] {
    const vector = this.nativeView._getExternalFilePaths();
    const paths: string[] = [];
    try {
      const count = vector.size();
      for (let i = 0; i < count; i++) {
        paths.push(vector.get(i));
      }
    } finally {
      vector.delete();
    }
    return paths;
  }

  /**
   * Loads external file data referenced by the PAGX document.
   * @param path The file path as returned by getExternalFilePaths()
   * @param data The file binary data
   * @returns true if successful
   */
  public loadFileData(path: string, data: Uint8Array): boolean {
    return this.nativeView._loadFileData(path, data);
  }

  /**
   * Builds the layer tree after parsing and loading external files.
   * Must be called after parsePAGX() and any loadFileData() calls.
   */
  public buildLayers(): void {
    this.nativeView._buildLayers();
  }

  /**
   * Updates the view size to match the canvas dimensions.
   * Call this after canvas resize.
   */
  public updateSize(): void {
    this.nativeView._updateSize();
  }

  /**
   * Updates the zoom scale and content offset.
   * @param zoom Zoom scale (1.0 = 100%)
   * @param offsetX Horizontal offset in pixels
   * @param offsetY Vertical offset in pixels
   */
  public updateZoomScaleAndOffset(zoom: number, offsetX: number, offsetY: number): void {
    this.nativeView._updateZoomScaleAndOffset(zoom, offsetX, offsetY);
  }

  /**
   * Sets a solid background color. When set, the solid color will be used instead of the default
   * checkerboard pattern.
   * @param color Hex color string (#RGB, #RRGGBB, #RGBA, or #RRGGBBAA)
   * @param alpha Optional alpha override (0.0 - 1.0). If provided, overrides any alpha in the color string.
   *
   * @example
   * ```typescript
   * view.setBackgroundColor('#ff0000');       // Opaque red
   * view.setBackgroundColor('#f00');          // Opaque red (shorthand)
   * view.setBackgroundColor('#ff000080');     // 50% transparent red
   * view.setBackgroundColor('#ffffff', 0.5);  // 50% transparent white
   * view.setBackgroundColor('#fff', 0.8);     // 80% opaque white
   * ```
   */
  public setBackgroundColor(color: string, alpha?: number): void {
    let parsed = PAGXView.parseColor(color);
    if (!parsed) {
      console.warn(`PAGXView: Invalid color format "${color}". Use #RGB, #RGBA, #RRGGBB, or #RRGGBBAA. Falling back to opaque white.`);
      parsed = { r: 1.0, g: 1.0, b: 1.0, a: 1.0 };
    }
    const finalAlpha = alpha !== undefined ? alpha : parsed.a;
    this.nativeView._setBackgroundColor(finalAlpha, parsed.r, parsed.g, parsed.b);
  }

  /**
   * Clears the custom background color and reverts to the default checkerboard pattern.
   */
  public clearBackgroundColor(): void {
    this.nativeView._clearBackgroundColor();
  }

  /**
   * Parses a hex color string into RGBA components (0.0 - 1.0).
   * Supports #RGB, #RGBA, #RRGGBB, #RRGGBBAA formats.
   */
  private static parseColor(color: string): { r: number; g: number; b: number; a: number } | null {
    if (!color.startsWith('#')) {
      return null;
    }
    const hex = color.slice(1);
    let r: number, g: number, b: number, a: number;

    if (hex.length === 3) {
      // #RGB
      r = parseInt(hex[0] + hex[0], 16) / 255;
      g = parseInt(hex[1] + hex[1], 16) / 255;
      b = parseInt(hex[2] + hex[2], 16) / 255;
      a = 1.0;
    } else if (hex.length === 4) {
      // #RGBA
      r = parseInt(hex[0] + hex[0], 16) / 255;
      g = parseInt(hex[1] + hex[1], 16) / 255;
      b = parseInt(hex[2] + hex[2], 16) / 255;
      a = parseInt(hex[3] + hex[3], 16) / 255;
    } else if (hex.length === 6) {
      // #RRGGBB
      r = parseInt(hex.slice(0, 2), 16) / 255;
      g = parseInt(hex.slice(2, 4), 16) / 255;
      b = parseInt(hex.slice(4, 6), 16) / 255;
      a = 1.0;
    } else if (hex.length === 8) {
      // #RRGGBBAA
      r = parseInt(hex.slice(0, 2), 16) / 255;
      g = parseInt(hex.slice(2, 4), 16) / 255;
      b = parseInt(hex.slice(4, 6), 16) / 255;
      a = parseInt(hex.slice(6, 8), 16) / 255;
    } else {
      return null;
    }

    if (isNaN(r) || isNaN(g) || isNaN(b) || isNaN(a)) {
      return null;
    }
    return { r, g, b, a };
  }

  /**
   * Draws the current frame immediately.
   */
  public draw(): void {
    this.nativeView._draw();
  }

  /**
   * Starts the render loop.
   */
  public start(): void {
    if (this._isRunning) return;
    this._isRunning = true;
    this.renderLoop();
  }

  /**
   * Stops the render loop.
   */
  public stop(): void {
    if (!this._isRunning) return;
    this._isRunning = false;
    if (this.animationFrameId !== null) {
      cancelAnimationFrame(this.animationFrameId);
      this.animationFrameId = null;
    }
  }

  /**
   * Destroys the view and releases resources.
   */
  public destroy(): void {
    this.stop();
    this.nativeView.delete();
    this._isDestroyed = true;
  }

  private renderLoop(): void {
    if (!this._isRunning) return;
    this.nativeView._draw();
    this.animationFrameId = requestAnimationFrame(() => this.renderLoop());
  }
}
