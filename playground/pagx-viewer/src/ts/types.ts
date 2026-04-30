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

import { TGFX } from '@tgfx/types';
import { PAGXView } from './pagx-view';

/**
 * A vector of strings returned from the WASM module.
 * Must call delete() when done to free memory.
 */
export interface StringVector {
  /**
   * Returns the number of elements.
   */
  size(): number;

  /**
   * Returns the element at the given index.
   */
  get(index: number): string;

  /**
   * Frees the memory. Must be called when done.
   */
  delete(): void;
}

/**
 * The native PAGX View instance bound from C++.
 */
export interface _PAGXView {
  /**
   * Registers fallback fonts for text rendering.
   * @param fontData Primary font data (e.g., NotoSansSC)
   * @param emojiFontData Emoji font data (e.g., NotoColorEmoji)
   */
  _registerFonts(fontData: Uint8Array, emojiFontData: Uint8Array): void;

  /**
   * Loads a PAGX document (no external files).
   * This is a convenience method that calls parsePAGX() and buildLayers().
   * @param data PAGX file binary data
   */
  _loadPAGX(data: Uint8Array): void;

  /**
   * Parses PAGX data without building layers.
   * Call getExternalFilePaths() and loadFileData() after this to load external resources,
   * then call buildLayers() to complete the loading.
   * @param data PAGX file binary data
   */
  _parsePAGX(data: Uint8Array): void;

  /**
   * Returns the list of external file paths referenced by the PAGX document.
   * Must call delete() on the returned vector when done.
   */
  _getExternalFilePaths(): StringVector;

  /**
   * Loads external file data referenced by the PAGX document.
   * @param path The file path as returned by getExternalFilePaths()
   * @param data The file binary data
   * @returns true if successful
   */
  _loadFileData(path: string, data: Uint8Array): boolean;

  /**
   * Builds the layer tree after parsing and loading external files.
   * Must be called after parsePAGX() and any loadFileData() calls.
   */
  _buildLayers(): void;

  /**
   * Updates the view size to match the canvas dimensions.
   * Call this after canvas resize.
   */
  _updateSize(): void;

  /**
   * Updates the zoom scale and content offset.
   * @param zoom Zoom scale (1.0 = 100%)
   * @param offsetX Horizontal offset in pixels
   * @param offsetY Vertical offset in pixels
   */
  _updateZoomScaleAndOffset(zoom: number, offsetX: number, offsetY: number): void;

  /**
   * Sets a solid background color. When set, the solid color will be used instead of the default
   * checkerboard pattern.
   * @param red Red component (0.0 - 1.0)
   * @param green Green component (0.0 - 1.0)
   * @param blue Blue component (0.0 - 1.0)
   * @param alpha Alpha component (0.0 - 1.0)
   */
  _setBackgroundColor(red: number, green: number, blue: number, alpha: number): void;

  /**
   * Clears the custom background color and reverts to the default checkerboard pattern.
   */
  _clearBackgroundColor(): void;

  /**
   * Draws the current frame. Call this in your render loop.
   */
  _draw(): void;

  /**
   * Returns the original width of the PAGX content.
   */
  _contentWidth(): number;

  /**
   * Returns the original height of the PAGX content.
   */
  _contentHeight(): number;

  /**
   * Releases the native resources. Must be called when done.
   */
  delete(): void;
}

/**
 * The PAGX Viewer WebAssembly module.
 */
export interface PAGXModule extends TGFX {
  _PAGXView: {
    /**
     * Creates a native PAGXView instance from a canvas element.
     * @param canvasID CSS selector for the canvas element (e.g., '#my-canvas')
     * @returns A native PAGXView instance, or null if creation failed
     */
    _MakeFrom: (canvasID: string) => _PAGXView | null;
  };
  PAGXView: typeof PAGXView;
}
