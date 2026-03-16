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

import type {TGFX} from '@tgfx/types';

/**
 * Native C++ PAGXView interface exposed via Emscripten bindings.
 */
export interface PAGXViewNative {
    /** Returns the canvas width in pixels. */
    width: () => number;
    /** Returns the canvas height in pixels. */
    height: () => number;
    /** Registers fallback fonts for text rendering. */
    registerFonts: (fontData: Uint8Array, emojiFontData: Uint8Array) => void;
    /** Loads a PAGX file and builds the layer tree in one step. */
    loadPAGX: (data: Uint8Array) => boolean;
    /** Parses a PAGX file without building layers. Use with getExternalFilePaths(), loadFileData(), and buildLayers(). */
    parsePAGX: (data: Uint8Array) => void;
    /** Returns external file paths referenced by Image nodes that need to be fetched. */
    getExternalFilePaths: () => StringVector;
    /** Loads external file data for an Image node matching the given path. Returns true on success. */
    loadFileData: (filePath: string, fileData: Uint8Array) => boolean;
    /** Builds the layer tree from the previously parsed document. */
    buildLayers: () => void;
    /** Updates the canvas size and recreates the surface. */
    updateSize: (width: number, height: number) => void;
    /** Updates the zoom scale and content offset for the display list. */
    updateZoomScaleAndOffset: (zoom: number, offsetX: number, offsetY: number) => void;
    /** Notifies that zoom gesture has ended for adaptive tile refinement. */
    onZoomEnd: () => void;
    /** Renders the current frame to the canvas. Returns true if rendering succeeds. */
    draw: () => boolean;
    /** Returns true if the first frame has been rendered successfully. */
    firstFrameRendered: () => boolean;
    /** Returns the width of the PAGX content in content pixels. */
    contentWidth: () => number;
    /** Returns the height of the PAGX content in content pixels. */
    contentHeight: () => number;
    /** Overrides bounds origin read from the PAGX file. Values can be negative. */
    setBoundsOrigin: (x: number, y: number) => void;
    /** Returns content transform parameters for mapping cocraft coordinates to canvas positions. */
    getContentTransform: () => ContentTransform;
    /** Releases the native C++ object. */
    delete: () => void;
}

/**
 * Emscripten register_vector<std::string> wrapper, used to access string vectors returned from C++.
 */
export interface StringVector {
    /** Returns the number of elements. */
    size: () => number;
    /** Returns the element at the given index. */
    get: (index: number) => string;
    /** Releases the underlying C++ object. Must be called after use to avoid memory leaks. */
    delete: () => void;
}

/**
 * Content transform parameters for mapping cocraft canvas coordinates to canvas pixel positions.
 * Returned by View.getContentTransform(). These values are static per file load and canvas size.
 */
export interface ContentTransform {
    /** X coordinate of the PAGX content bounds origin in cocraft canvas coordinates. */
    boundsOriginX: number;
    /** Y coordinate of the PAGX content bounds origin in cocraft canvas coordinates. */
    boundsOriginY: number;
    /** Scale factor applied to fit PAGX content into the canvas (contain mode). */
    fitScale: number;
    /** Horizontal pixel offset for centering the scaled content in the canvas. */
    centerOffsetX: number;
    /** Vertical pixel offset for centering the scaled content in the canvas. */
    centerOffsetY: number;
}

/**
 * PAGX WebAssembly module interface extending TGFX with PAGXView support.
 */
export interface PAGX extends TGFX {
    /** Factory for creating native PAGXView instances. */
    PAGXView: {
        /** Creates a PAGXView instance with the given canvas dimensions. */
        MakeFrom: (width: number, height: number) => PAGXViewNative | null;
    };
    VectorString: any;
    module: PAGX;
}

