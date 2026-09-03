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
    hasTimeline(): boolean;
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

    // Animation-unit tree export (diagnostics / future timeline panel). Definitions plus <Timelines>
    // mount points, nested by composition reference; see TimelineTreeNode.
    getTimelineTree(): TimelineTreeNode[];

    // Solo-preview selection: playback values route to the selected animation while every other
    // clock freezes; a selected state machine freezes the clocks and exposes no time axis. Empty
    // id clears the selection.
    selectTimelineUnit(kind: string, id: string): boolean;
    getSelectedTimelineUnit(): { kind: string; id: string } | null;

    // Live { regionName: currentStateName } of the default state machine timeline (empty object
    // when the default timeline is not a state machine). Polling endpoint for the blueprint view.
    getSMCurrentStates(): Record<string, string>;

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

/** One entry of the animation-unit tree exported by getTimelineTree. `path` encodes the nesting
 *  ("1", "1/0"): nodes sharing a prefix are siblings under the same parent. durationUs: >0 known,
 *  0 none (empty animation / empty state), -1 unresolvable (state machines, dangling refs). */
export interface TimelineTreeNode {
    /** Position in the tree, e.g. "1" or "1/0". */
    path: string;
    /** "animation" | "stateMachine" (top-level definitions) | "mount" (<Timelines> mount point) |
     *  "compositionGroup" (synthetic parent wrapping a layer that has no drivers of its own but
     *  references a composition containing mounts — non-clickable, just a visual grouping). */
    kind: 'animation' | 'stateMachine' | 'mount' | 'compositionGroup';
    /** Definition id / referenced id. */
    id: string;
    /** Display name: definition id, or the mounting layer id for mounts. */
    name: string;
    /** Duration in microseconds; 0 = none, -1 = unresolvable. */
    durationUs: number;
    /** Whether this definition is the default timeline (first top-level entry). */
    isDefault?: boolean;
    /** Animation definition frame rate. */
    frameRate?: number;
    /** Animation loop mode: "once" | "loop" | "pingPong". */
    loop?: string;
    /** Mount only: what the mount references. */
    refKind?: 'animation' | 'stateMachine';
    /** Mount (animation) only: initial playing flag. */
    playing?: boolean;
    /** Mount (animation) only: evaluationOffset in frames. */
    offsetFrames?: number;
    /** Mount only: id of the layer carrying the <Timelines>. */
    layerId?: string;
    /** stateMachine only: declared inputs. */
    inputs?: { name: string; type: string }[];
    /** stateMachine only: regions with their states, transitions, and the runtime current state
     *  (empty string when the runtime instance is unreachable, e.g. a nested mount). */
    regions?: {
        name: string;
        initial: string;
        current: string;
        states: {
            name: string;
            animationId: string;
            durationUs: number;
            /** Whether the bound animation can be solo-previewed (targets resolvable in the root
             *  binding scope); false for empty states and dangling/nested-only definitions. */
            previewSupported?: boolean;
        }[];
        transitions?: {
            from: string;
            to: string;
            fromAny: boolean;
            /** AND-joined condition summary, e.g. "speed > 0.5", or "always" when unconditional. */
            conditions: string;
        }[];
    }[];
    /** Nested mount nodes (composition-reference nesting). */
    children: TimelineTreeNode[];
}

/** Static shape of the wasm module returned by the host-supplied `moduleFactory`. The player
 *  only calls `PAGXView.init(selector)` on the module, so hosts feeding in adapters just need
 *  to expose the same shape. */
export interface PlayerModule {
    PAGXView: {
        init(canvas: string | HTMLCanvasElement): PlayerView | null;
    };
}
