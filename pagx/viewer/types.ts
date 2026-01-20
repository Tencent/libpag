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

export interface EmscriptenGLContext {
    handle: number;
    GLctx: WebGLRenderingContext;
    attributes: EmscriptenGLContextAttributes;
    initExtensionsDone: boolean;
    version: number;
}

export type EmscriptenGLContextAttributes =
    { majorVersion: number; minorVersion: number }
    & WebGLContextAttributes;

export interface EmscriptenGL {
    contexts: (EmscriptenGLContext | null)[];
    createContext: (
        canvas: HTMLCanvasElement | OffscreenCanvas,
        webGLContextAttributes: EmscriptenGLContextAttributes,
    ) => number;
    currentContext?: EmscriptenGLContext;
    deleteContext: (contextHandle: number) => void;
    framebuffers: (WebGLFramebuffer | null)[];
    getContext: (contextHandle: number) => EmscriptenGLContext;
    getNewId: (array: any[]) => number;
    makeContextCurrent: (contextHandle: number) => boolean;
    registerContext: (ctx: WebGLRenderingContext, webGLContextAttributes: EmscriptenGLContextAttributes) => number;
    textures: (WebGLTexture | null)[];
}

export interface PAGXModule extends EmscriptenModule {
    GL: EmscriptenGL;
    PAGXView: {
        MakeFrom: (canvasID: string) => PAGXView | null;
    };
    [key: string]: any;
}

export interface PAGXView {
    loadPAGX: (pagxData: Uint8Array) => void;
    updateSize: () => void;
    updateZoomScaleAndOffset: (zoom: number, offsetX: number, offsetY: number) => void;
    draw: () => void;
    contentWidth: () => number;
    contentHeight: () => number;
}
