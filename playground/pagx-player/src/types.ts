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

// Public type definitions for the pagx-player component package.
//
// The player operates against a structural PlayerView/PlayerModule contract declared inside
// this package (see pagx-view-types.ts) rather than importing types from pagx-viewer. That
// keeps the emitted .d.ts self-contained, avoids a hard peer dependency on an unpublished
// package, and lets hosts inject any backend implementing the same shape. pagx-viewer's
// PAGXView is structurally assignable to PlayerView so existing hosts keep working with no
// adapter.

import type { PlayerModule, PlayerView } from './pagx-view-types';

/** Menu item slot for the toolbar. External hosts (playground homepage buttons, etc.) inject
 *  their own entries around the built-in Reset / Source Editor buttons. */
export interface ToolbarSlot {
    /** Stable id assigned to the rendered button (also used as the <button id="..."> attribute so
     *  external CSS / DevTools inspection keeps working). Must be unique within a toolbar. */
    id: string;

    /** Full inner HTML of the button (icons, spans, whatever). The component wraps it in a
     *  <button class="toolbar-btn"> element. Ignored when `divider` is true. */
    html?: string;

    /** Optional title attribute (tooltip). */
    title?: string;

    /** Optional click handler. */
    onClick?: (event: MouseEvent) => void;

    /** Optional extra CSS classes to add to the button element. */
    extraClass?: string;

    /** Render as a vertical divider instead of a button. Ignores html/onClick. */
    divider?: boolean;
}

/** Callbacks the host must provide when the source editor is enabled. Return empty string on
 *  success, any non-empty string is treated as an error message and surfaced through the
 *  player's unified status pill (see PAGXPlayer.showStatus). The return may be sync or async;
 *  when a Promise is returned the editor disables its buttons and shows an "Applying..." /
 *  "Saving..." status until the promise settles, so hosts running real async pipelines don't
 *  need to build their own progress UI. Feedback strings ("Changes applied", "Changes
 *  discarded", etc.) are emitted by the editor itself into the same status slot on completion. */
export interface EditorCallbacks {
    /** Called when the user clicks Apply. Should parse `xmlText` and rebuild the view. Return
     *  '' (or a Promise resolving to '') for success, any other string signals an error. */
    onApply: (xmlText: string) => string | Promise<string>;

    /** Called when the user clicks Save. Same string convention as onApply. The component only
     *  persists the source text through this callback and does NOT automatically apply the
     *  same text to the view - hosts are expected to either (a) also call onApply themselves
     *  from within onSave, or (b) rely on their own reload mechanism (SSE file watcher, etc.)
     *  to re-render after the persistence step. This split lets pure "download" hosts (like
     *  the official playground offering a .pagx download) skip the render pipeline entirely. */
    onSave: (xmlText: string) => string | Promise<string>;
}

/** Solid background color used for the pagx compositing base. r/g/b are 0..255 integers, a is
 *  a 0..1 float. Alpha 0 keeps the checkered CSS backdrop of the canvas visible for
 *  transparent regions. */
export interface BackgroundColor {
    r: number;
    g: number;
    b: number;
    a: number;
}

export interface PAGXPlayerOptions {
    /** The DOM element that hosts the player. The component appends canvas / toolbar / playback
     *  bar / editor panel as children. The container is expected to occupy the viewport region
     *  the player is meant to fill; layout / sizing is the host's responsibility. */
    container: HTMLElement;

    /** Factory that instantiates the PAGX wasm module. The host controls where the wasm binary
     *  is fetched from and how it's cached; the player just awaits the resulting module. The
     *  return only needs to satisfy {@link PlayerModule}; pagx-viewer's PAGXInit result
     *  already does structurally. */
    moduleFactory: () => Promise<PlayerModule>;

    /** Base URL used to build the <img src> for playback bar icons (play/pause/previous/next).
     *  A trailing slash is optional. Example: './' for local resources next to index.html, or
     *  '/static/icons/' when served by a dev server. */
    iconBaseUrl: string;

    /** Enable the built-in Source Editor button and panel. CodeMirror is bundled inside the
     *  component's shipped ESM, so hosts don't need to install any editor dependency
     *  themselves. Default false. */
    enableEditor?: boolean;

    /** Editor callbacks (required when enableEditor is true). */
    editorCallbacks?: EditorCallbacks;

    /** Extra items injected into the toolbar. `before` is prepended, `after` is appended; the
     *  built-in Reset View + Source Editor buttons sit between them. */
    extraMenuItems?: {
        before?: ToolbarSlot[];
        after?: ToolbarSlot[];
    };

    /** Compositing background color. Default: (0,0,0,0) (fully transparent) matching the
     *  playground / preview default. */
    backgroundColor?: BackgroundColor;

    /** Optional container element used to compute canvas backing-store size. Defaults to the
     *  component's internal root wrapper (a child of `options.container` that shrinks when the
     *  source editor is open), so canvas dimensions naturally follow the visible player region.
     *  Provided as an escape hatch for hosts that layer additional chrome (side panels, etc.)
     *  and need canvas sizing to track a different element than the player's own wrapper. */
    canvasSizeContainer?: HTMLElement;
}

/** Options for a single `load()` call. */
export interface PAGXPlayerLoadOptions {
    /** Resolve an external resource path (image / nested pagx) to raw bytes. Called for every
     *  path returned by `PAGXView.getExternalFilePaths()` after `parsePAGX`. Return null to
     *  skip a missing resource (the render pipeline will use fallback data). */
    resolveResource?: (relPath: string) => Promise<Uint8Array | null>;

    /** Register fonts on the newly loaded view. Called after `parsePAGX` and before
     *  `buildLayers`. The host is responsible for the font byte source (static asset, service
     *  endpoint, etc). The view is typed as {@link PlayerView} which lists only the methods
     *  the player itself uses; hosts calling `view.registerFonts(...)` (present on
     *  pagx-viewer's PAGXView but not on the structural contract) can cast the argument to
     *  their own view type if strict typing is required. */
    registerFonts?: (view: PlayerView) => Promise<void>;

    /** Preserve the current playback time across the load. Used by the preview server to make
     *  SSE-triggered reloads feel continuous. Default: false (reset to 0). */
    preserveCurrentTime?: boolean;

    /** Force loop=true on the view after buildLayers. Default: true (matches playground
     *  behavior). Set to false to skip the forced-on write and preserve whatever loop mode
     *  the parsed document declares. Note the option only ever *forces on*; there is no
     *  built-in "force off" flavor, and the naming reflects that. */
    forceLoop?: boolean;

    /** Pre-decoded XML text used to seed the source editor. When omitted, the editor decodes
     *  from bytes via TextDecoder. */
    xmlText?: string;
}

/** Detail payload emitted with the 'loaded' CustomEvent. */
export interface LoadedEventDetail {
    /** Animation duration in microseconds (0 for static pagx). */
    duration: number;
    /** Animation frame rate; 0 when the document has no animation. */
    frameRate: number;
    /** Convenience flag: duration > 0. */
    hasAnimation: boolean;
    /** XML source of the loaded pagx (only present when the editor is enabled or xmlText was
     *  provided; hosts can use this to feed the editor or dev tools). */
    xmlText?: string;
}

/** Detail payload for the 'loadError' CustomEvent. */
export interface LoadErrorEventDetail {
    error: Error;
}

/** Detail payload for the 'framechange' CustomEvent. Fires once per playback UI tick (~100ms)
 *  while playing, plus once on stop transition; not on every rAF. */
export interface FrameChangeEventDetail {
    currentTimeMicros: number;
    currentFrame: number;
    totalFrames: number;
}

/** Detail payload for the 'seek' CustomEvent, emitted after any user- or programmatic-driven
 *  jump to a new playback position (slider drag settle, frame-step buttons, keyboard arrows). */
export interface SeekEventDetail {
    currentTimeMicros: number;
}

/** Options for {@link PAGXPlayer.showStatus}. */
export interface StatusOptions {
    /** Visual style. 'info' (default) uses a subtle dark pill for passive updates (loading,
     *  reloading), 'success' uses a green pill for confirmations of explicit user actions
     *  (apply, discard, save), 'error' uses a red pill for anything that needs attention. */
    kind?: 'info' | 'success' | 'error';
    /** Auto-hide delay in ms. Omit (or set to 0) for a sticky message that only hides when
     *  another showStatus() call replaces it or hideStatus() is invoked explicitly. */
    autoHideMs?: number;
}

/** Map of event name -> concrete Event subtype dispatched on the PAGXPlayer instance. Used by
 *  the PAGXPlayer.addEventListener / removeEventListener overloads to make listeners fully
 *  typed: `player.addEventListener('loaded', e => e.detail.duration)` compiles, while
 *  `player.addEventListener('loded', ...)` (typo) errors at compile time. Events without a
 *  payload (`play`, `pause`) map to a plain Event so `.detail` isn't offered on hover. */
export interface PAGXPlayerEventMap {
    loaded: CustomEvent<LoadedEventDetail>;
    loadError: CustomEvent<LoadErrorEventDetail>;
    framechange: CustomEvent<FrameChangeEventDetail>;
    seek: CustomEvent<SeekEventDetail>;
    play: Event;
    pause: Event;
}

/** Union of custom event names dispatched by the player on its EventTarget. Kept as a public
 *  export for hosts that build event-name-driven abstractions on top of PAGXPlayer. */
export type PAGXPlayerEventName = keyof PAGXPlayerEventMap;

/** Re-exported for host convenience: hosts writing custom `registerFonts` / `resolveResource`
 *  callbacks or reading `player.getView()` can annotate against these shapes without importing
 *  pagx-viewer. Any concrete PAGXView instance from pagx-viewer is structurally assignable to
 *  PlayerView. */
export type { PlayerModule, PlayerView };
