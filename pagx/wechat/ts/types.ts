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

import type {View} from './pagx-view';

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
    /**
     * Attaches a host-decoded native image to Image nodes matching the given path under a
     * specific quality tier. The image is queued for GPU texture upload; actual texImage2D
     * happens during the next draw(). The qualityRaw argument is the integer value of
     * ImageQuality (Thumbnail = 0, Full = 1).
     */
    attachNativeImage: (filePath: string, nativeImage: any, qualityRaw: number) => boolean;
    /**
     * Provides the original (full-resolution) pixel dimensions of an external image so that
     * the new-format PAGX ImagePattern matrix can be corrected when the host attaches a
     * downscaled variant. New-format PAGX bakes the pattern matrix in the original image's
     * pixel coordinate system; without this hint the SDK falls back to the attached image's
     * own dimensions, which produces a misaligned fill. Call right after parsePAGX() and
     * before buildLayers() / attachNativeImage(). The width/height must be > 0; parsePAGX()
     * clears the entire table.
     */
    setImageOriginalSize: (filePath: string, width: number, height: number) => void;
    /**
     * Registers a JavaScript object whose onTextureRequest / onTextureEvict methods receive
     * backend-texture lifecycle events. Pass null/undefined to clear a previously registered
     * handler. The handler survives across parsePAGX() calls.
     */
    setTextureEventHandler: (handler: any) => void;
    /**
     * Returns true when externalTexturesTotalBytes has crossed fullBudgetHardCap. Hosts running
     * a progressive thumbnail-to-full upgrade flow should consult this at the top of every
     * upgrade pass and skip new full-quality uploads while it returns true. Reactive responses
     * to onTextureRequest are still safe (each one is a 1:1 replacement for an evicted entry,
     * so totalBytes stays net-flat). The flag clears automatically on the next draw whose
     * eviction sweep reduces totalBytes back below the hard cap.
     */
    isFullBudgetSaturated: () => boolean;
    /**
     * Returns root-space (canvas-pixel) bounds for each filePath in the provided list. Values
     * are either { unionBounds, largestBounds } objects or null when the filePath is not
     * referenced by any layer. Must be called after buildLayers(). The first call triggers
     * lazy localBounds computation inside tgfx and is noticeably heavier than later calls.
     */
    getImageBounds: (filePathList: string[]) => Record<string, ImageBoundsEntry | null>;
    /**
     * Returns image metadata (original dimensions + per-usage node dimensions and scaleMode)
     * for every externally referenced image in the document. JS callers use this to pick the
     * right thumbnail size and compute display scale without having to re-parse the PAGX XML.
     * Must be called after parsePAGX().
     */
    getImageMetadata: () => ImageMetadataEntry[];
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
    /** Drop intermediate snapshots captured before gesture-state init, reset first-frame flag. */
    resetForFreshCapture: () => void;
    /** Returns the width of the PAGX content in content pixels. */
    contentWidth: () => number;
    /** Returns the height of the PAGX content in content pixels. */
    contentHeight: () => number;
    /** Overrides bounds origin read from the PAGX file. Values can be negative. */
    setBoundsOrigin: (x: number, y: number) => void;
    /** Toggles gesture-freeze rendering; true during active pan/zoom, false otherwise. */
    setGestureActive: (active: boolean) => void;
    /** Toggles the fitSnapshot fast path. Default true. Disable to force full render every frame. */
    setSnapshotEnabled: (enabled: boolean) => void;
    /** Returns content transform parameters for mapping cocraft coordinates to canvas positions. */
    getContentTransform: () => ContentTransform;
    /** Looks up a node by ID and returns its position relative to the canvas. */
    getNodePosition: (nodeId: string) => NodePosition;
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
 * Result of looking up a node's position relative to the canvas.
 * Returned by View.getNodePosition().
 */
export interface NodePosition {
    /** Whether the node was found and has valid position data. */
    found: boolean;
    /** The x coordinate of the node relative to the canvas (0 if not found). */
    x: number;
    /** The y coordinate of the node relative to the canvas (0 if not found). */
    y: number;
}

/**
 * Root-space axis-aligned rectangle. Coordinates are in canvas pixels, already accounting for
 * the content fit scale and centering offset applied to contentLayer.
 */
export interface ImageRect {
    x: number;
    y: number;
    w: number;
    h: number;
}

/**
 * Bounds result for a single filePath returned by View.getImageBounds().
 */
export interface ImageBoundsEntry {
    /** Union of the bounds of every layer referencing the filePath. Used for viewport tests. */
    unionBounds: ImageRect;
    /** Bounds of the single layer with the largest display area. Used for focus distance. */
    largestBounds: ImageRect;
}

/**
 * Per-usage ImagePattern parameters surfaced to JS by View.getImageMetadata().
 */
export interface ImageUsage {
    /** Target node width in design-space pixels (may be 0 when the exporter omitted it). */
    nodeWidth: number;
    /** Target node height in design-space pixels (may be 0 when the exporter omitted it). */
    nodeHeight: number;
    /** ImageScaleMode encoded as the PAGX integer: 0=FILL, 1=FIT, 2=STRETCH, 3=TILE. */
    scaleMode: number;
    /** Scale factor used by TILE patterns (ignored for non-TILE modes). */
    scaleFactor: number;
}

/**
 * Metadata entry returned for each externally referenced image by View.getImageMetadata().
 */
export interface ImageMetadataEntry {
    /** External file path, used as the key when calling attachNativeImage() etc. */
    filePath: string;
    /** Original image width in source pixels (0 when the exporter omitted it). */
    origWidth: number;
    /** Original image height in source pixels (0 when the exporter omitted it). */
    origHeight: number;
    /** Every ImagePattern that references this filePath, one entry per usage. */
    usages: ImageUsage[];
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
    /**
     * High-level View class bound onto the module by binding(); only available after
     * PAGXInit() resolves. Use View.init(module, canvas, options?) to create an instance.
     */
    View: typeof View;
    VectorString: any;
    module: PAGX;
}

