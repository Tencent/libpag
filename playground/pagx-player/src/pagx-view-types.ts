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

    // Selection (phase 1, read-only canvas<->editor queries)
    hitTest(surfaceX: number, surfaceY: number): HitTestResult | null;
    getNodeSourceMap(): NodeSourceEntry[];
    getNodeBounds(index: number): NodeBounds[] | null;
    validatePAGX(data: Uint8Array): PagxSchemaDiagnostic[];

    // Incremental edit (phase 2, source-editor attribute edits). Sets a channel on nodes[index]
    // from its raw PAGX attribute string and refreshes the scene in place. Returns false when the
    // edit cannot go incremental (invalid index, unknown channel, or unparseable value), signalling
    // the caller to fall back to a full reparse.
    setNodeChannel(index: number, channel: string, value: string): boolean;

    // StateMachine input hooks (playground interaction testing). All three return false when
    // the default timeline is not a state machine or the input name/type does not match.
    setSMInputBool(name: string, value: boolean): boolean;
    setSMInputNumber(name: string, value: number): boolean;
    fireSMInputTrigger(name: string): boolean;

    // Lifetime
    destroy(): void;
}

/** One node's source span and incrementable channels, exported by getNodeSourceMap. */
export interface NodeSourceEntry {
    index: number;
    startLine: number;   // 1-based source line of the start tag; -1 = unavailable
    endLine: number;     // 1-based source line of the end tag; -1 = unavailable
    nodeType: number;    // NodeType enum int (see NodeType.h)
    channels: string[];  // incrementable channel names (phase-2 reuse)
}

/** Surface-space bounds of one runtime layer instance, returned by getNodeBounds. A source node
 *  referenced by multiple composition layers yields one array entry per instance. */
export interface NodeBounds {
    x: number;
    y: number;
    w: number;
    h: number;
}

/** Hit-test result returned by hitTest. The span points at the *reference* node — when the click
 *  lands inside a <Layer composition="@X"> instance, startLine/endLine refer to that <Layer>
 *  reference rather than the internal definition node. bounds is the on-screen rect of the clicked
 *  instance itself, so the overlay outlines exactly what the user clicked. bounds is null when the
 *  resolved layer has no measurable on-screen rect (e.g. empty bounds). */
export interface HitTestResult {
    index: number;
    startLine: number;
    endLine: number;
    bounds: NodeBounds | null;
}

/** A schema-level PAGX diagnostic exported by the WASM importer. XML well-formedness diagnostics
 *  are produced separately in SourceEditor with DOMParser; these cover valid XML that violates the
 *  PAGX schema, such as unknown elements, invalid attribute values, and unresolved references. */
export interface PagxSchemaDiagnostic {
    message: string;
    line: number;
    column: number;
}

/** Static shape of the wasm module returned by the host-supplied `moduleFactory`. The player
 *  only calls `PAGXView.init(selector)` on the module, so hosts feeding in adapters just need
 *  to expose the same shape. */
export interface PlayerModule {
    PAGXView: {
        init(canvas: string | HTMLCanvasElement): PlayerView | null;
    };
}
