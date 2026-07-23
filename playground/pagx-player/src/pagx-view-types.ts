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

// Structural view/module interfaces the player operates against. Declaring them here (instead
// of importing from pagx-viewer) means the emitted .d.ts is self-contained and the package can
// be installed without a resolved pagx-viewer copy on disk; hosts inject the runtime
// implementation via `moduleFactory` and only need to satisfy this shape. pagx-viewer's actual
// PAGXView class is structurally assignable to PlayerView so hosts don't need any adapter.

/** Subset of PAGXView the player and its subsystems (gestures, playback bar, editor) actually
 *  call. Kept intentionally narrow so any future backend that speaks the same shape can be
 *  plugged in without touching the component. */
export interface PlayerView {
    // Document lifecycle
    parsePAGX(data: Uint8Array): void;
    getExternalFilePaths(): string[];
    loadFileData(path: string, data: Uint8Array): boolean;
    buildLayers(): void;

    // Rendering / sizing
    draw(): void;
    updateSize(): void;
    updateZoomScaleAndOffset(zoom: number, offsetX: number, offsetY: number): void;
    setBackgroundColor(color: string, alpha?: number): void;

    // Playback
    play(): void;
    pause(): void;
    isPlaying(): boolean;
    start(): void;
    stop(): void;
    currentTimeMicros(): number;
    setCurrentTimeMicros(micros: number): void;
    durationMicros(): number;
    frameRate(): number;
    setLoop(loop: boolean): void;
    isLoop(): boolean;

    // Lifetime
    destroy(): void;
}

/** Static shape of the wasm module returned by the host-supplied `moduleFactory`. The player
 *  only calls `PAGXView.init(selector)` on the module, so hosts feeding in adapters just need
 *  to expose the same shape. */
export interface PlayerModule {
    PAGXView: {
        init(canvas: string | HTMLCanvasElement): PlayerView | null;
    };
}
