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
    /** Loads a PAGX file from the given data. */
    loadPAGX: (data: Uint8Array) => boolean;
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
    /** Releases the native C++ object. */
    delete: () => void;
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

