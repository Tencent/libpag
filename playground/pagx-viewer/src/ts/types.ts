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
 * Surface-space bounds of a layer, in backing-store pixels (canvas.width space, includes DPR).
 */
export interface NodeBounds {
  x: number;
  y: number;
  w: number;
  h: number;
}

/**
 * The result of a hit test: the source node under a surface point with its source span and
 * on-screen bounds. startLine/endLine point at the reference node: when the hit lands inside a
 * <Layer composition="@X"> instance they refer to that reference rather than the internal
 * definition node. bounds is the hit instance's surface rect, or null when the resolved layer
 * has no measurable on-screen rect.
 */
export interface HitTestResult {
  /** Index of the source node in the document's node list. */
  index: number;
  /** 1-based source line of the node's start tag; -1 when unavailable. */
  startLine: number;
  /** 1-based source line of the node's end tag; -1 when unavailable. */
  endLine: number;
  bounds: NodeBounds | null;
}

/**
 * One node's source span and incrementable channels, as exported by getNodeSourceMap.
 */
export interface NodeSourceEntry {
  /** Index of the node in the document's node list. */
  index: number;
  /** 1-based source line of the start tag; -1 when unavailable. */
  startLine: number;
  /** 1-based source line of the end tag; -1 when unavailable. */
  endLine: number;
  /** NodeType enum int (see NodeType.h). */
  nodeType: number;
  /** Incrementable channel names (e.g. "alpha", "position.x"). */
  channels: string[];
}

/**
 * A schema-level PAGX diagnostic produced by validatePAGX: valid XML that violates the PAGX
 * schema, such as unknown elements, invalid attribute values, or unresolved references.
 */
export interface PagxSchemaDiagnostic {
  message: string;
  line: number;
  column: number;
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
   * Starts or resumes playback of the default timeline.
   */
  _play(): void;

  /**
   * Pauses playback of the default timeline.
   */
  _pause(): void;

  /**
   * Returns whether the default timeline is currently playing.
   */
  _isPlaying(): boolean;

  /**
   * Returns the current playback time in microseconds.
   */
  _currentTimeMicros(): number;

  /**
   * Returns the total duration in microseconds.
   */
  _durationMicros(): number;

  /**
   * Returns the frame rate of the animation.
   */
  _frameRate(): number;

  /**
   * Sets the current playback time in microseconds.
   * @param micros Time in microseconds
   */
  _setCurrentTimeMicros(micros: number): void;

  /**
   * Sets whether playback loops, overriding the file's loop mode.
   * @param loop true to repeat after each cycle, false to rewind to the first frame and stop after
   *   a single pass
   */
  _setLoop(loop: boolean): void;

  /**
   * Returns whether playback is set to loop.
   */
  _isLoop(): boolean;

  /**
   * Returns the source node under the surface point as a HitTestResult, or null when nothing
   * is hit. Surface coordinates are backing-store pixels (canvas.width space, includes DPR).
   */
  _hitTest(surfaceX: number, surfaceY: number): HitTestResult | null;

  /**
   * Exports every node's source span and incrementable channel list as a JS array of
   * { index, startLine, endLine, nodeType, channels }. Rebuilt by the host after each load.
   * Returns a plain JS value (emscripten::val), no manual delete() needed.
   */
  _getNodeSourceMap(): NodeSourceEntry[];

  /**
   * Returns the current-frame surface bounds { x, y, w, h } of every runtime layer instance
   * built from nodes[index] — one array entry per instance when the node is referenced by
   * several composition layers — or null if index is out of range or has no runtime layer.
   */
  _getNodeBounds(index: number): NodeBounds[] | null;

  /**
   * Validates UTF-8 PAGX XML without replacing the currently loaded document. Returns a plain JS
   * array of { message, line, column } schema diagnostics.
   */
  _validatePAGX(data: Uint8Array): PagxSchemaDiagnostic[];

  /**
   * Sets a channel on nodes[index] from its raw PAGX attribute string and refreshes the scene in
   * place. Returns false when the index is invalid, the channel is unknown for the node type, or
   * the string cannot be parsed (the caller should fall back to a full reparse).
   * @param index Node index from the source map
   * @param channel Channel name (e.g. "alpha", "color", "position.x")
   * @param value Value in PAGX attribute string form
   */
  _setNodeChannel(index: number, channel: string, value: string): boolean;

  /**
   * StateMachine input hooks (playground interaction testing). Return false when the default
   * timeline is not a state machine or the input name/type does not match.
   */
  _setSMInputBool(name: string, value: boolean): boolean;
  _setSMInputNumber(name: string, value: number): boolean;
  _fireSMInputTrigger(name: string): boolean;

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
