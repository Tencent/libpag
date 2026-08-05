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

// PAGXPlayer: the top-level component that stitches together the wasm view, gesture manager,
// playback bar, toolbar and (optionally) source editor. Host applications (pagx-playground,
// pagx-preview) instantiate a single player, subscribe to its events, and feed pagx bytes via
// load(); everything else - keyboard shortcuts, size responsiveness, playback UI - is owned by
// the player.

import type { NodeSourceEntry, PlayerModule, PlayerView } from './pagx-view-types';
import type {
    BackgroundColor,
    EditorCallbacks,
    FrameChangeEventDetail,
    LoadedEventDetail,
    LoadErrorEventDetail,
    PAGXPlayerEventMap,
    PAGXPlayerLoadOptions,
    PAGXPlayerOptions,
    StatusOptions,
    ToolbarSlot,
} from './types';
import { GestureManager, bindCanvasEvents } from './gesture-manager';
import { PlaybackBar } from './playback-bar';
import { buildToolbar, setToolbarVisible } from './toolbar';
import { EditorPanel, EDITOR_STATUS_DURATION_MS } from './editor/index';
import { ensureStylesInjected } from './styles';

/** Canvas element id assigned by the player. Kept stable so external CSS or debug tooling
 *  that targets '#pagx-canvas' keeps working on the (dominant) single-instance case. The
 *  player itself no longer depends on this id for view initialization - initView() hands the
 *  canvas element directly to PAGXView.init - so multiple PAGXPlayer instances on the same
 *  document remain functionally isolated even though their canvases share this id. */
const CANVAS_ID = 'pagx-canvas';

/** Default background: fully transparent so the checkered canvas backdrop shows through for
 *  pagx documents with alpha. */
const DEFAULT_BACKGROUND: BackgroundColor = { r: 0, g: 0, b: 0, a: 0 };

/** One incremental channel write derived from a source-editor edit: set channel on nodes[index]
 *  to the raw attribute string value. */
interface ChannelEdit {
    index: number;
    channel: string;
    value: string;
}

/** Parses a node's source-span fragment and returns its root element, or null when the fragment is
 *  not a single well-formed element (malformed XML, or extra content around the root). */
function parseSpanElement(span: string): Element | null {
    const doc = new DOMParser().parseFromString(span, 'application/xml');
    if (doc.querySelector('parsererror') !== null) {
        return null;
    }
    return doc.documentElement;
}

/** Returns an element's own attributes as a name->value map (values are XML-decoded, matching the
 *  importer, which reads decoded attribute values too). */
function attributeMap(element: Element): Record<string, string> {
    const map: Record<string, string> = {};
    for (let i = 0; i < element.attributes.length; i++) {
        const attr = element.attributes.item(i);
        if (attr !== null) {
            map[attr.name] = attr.value;
        }
    }
    return map;
}

/** One resolved sub-channel write produced by splitting a composite attribute (e.g. position ->
 *  position.x / position.y). channel is the dotted sub-channel name the engine's channel table
 *  understands; value is that component in the same string form the importer would parse. */
interface SubEdit {
    channel: string;
    value: string;
}

/** The composite XML attributes and how each expands into sub-channels. A composite attribute is a
 *  single XML attribute (e.g. position="10 20") that the channel table addresses as several dotted
 *  sub-channels (position.x / position.y). The kind selects the split/expand rule below. left /
 *  right / top / bottom / centerX / centerY and Text's x / y are plain scalars, not composites, and
 *  are handled by the normal whitelist path. matrix / matrix3D have no sub-channels and are
 *  intentionally absent so they keep falling back to a full reparse. */
const COMPOSITE_ATTRIBUTES = new Map<string, 'point' | 'size' | 'padding'>([
    ['position', 'point'],
    ['scale', 'point'],
    ['anchor', 'point'],
    ['startPoint', 'point'],
    ['endPoint', 'point'],
    ['center', 'point'],
    ['baselineOrigin', 'point'],
    ['size', 'size'],
    ['padding', 'padding'],
]);

/** Splits a whitespace/comma-separated numeric string into floats, matching the importer's
 *  ParseFloatList (strtof over each token). Returns null when a token is not a leading-number, so a
 *  malformed composite value forces a full reparse instead of a wrong incremental write. */
function parseFloatTokens(value: string): number[] | null {
    const tokens = value.trim().split(/[\s,]+/).filter((token) => token.length > 0);
    const result: number[] = [];
    for (const token of tokens) {
        // parseFloat mirrors strtof's leading-number semantics (trailing junk after a valid number
        // is ignored by the importer too); a token with no leading number is rejected.
        const parsed = Number.parseFloat(token);
        if (Number.isNaN(parsed)) {
            return null;
        }
        result.push(parsed);
    }
    return result;
}

/** Expands a padding attribute into its four [top, right, bottom, left] components, replicating the
 *  importer's CSS-shorthand rule (PaddingFromString): 1 value = all four; 2 = [v0,v1,v0,v1] as
 *  top/bottom then right/left; 4 = [top,right,bottom,left]. Returns null for any other count so the
 *  edit falls back to a full reparse (the importer rejects those too). Components are emitted as the
 *  original token strings (not reformatted numbers) so an unchanged component is byte-identical to
 *  what setNodeChannel would parse. */
function expandPadding(value: string): { top: string; right: string; bottom: string; left: string } | null {
    const tokens = value.trim().split(/[\s,]+/).filter((token) => token.length > 0);
    if (parseFloatTokens(value) === null) {
        return null;
    }
    if (tokens.length === 1) {
        return { top: tokens[0], right: tokens[0], bottom: tokens[0], left: tokens[0] };
    }
    if (tokens.length === 2) {
        return { top: tokens[0], right: tokens[1], bottom: tokens[0], left: tokens[1] };
    }
    if (tokens.length === 4) {
        return { top: tokens[0], right: tokens[1], bottom: tokens[2], left: tokens[3] };
    }
    return null;
}

/** Splits a composite attribute's old and new values into the sub-channel writes for only the
 *  components that actually changed (e.g. editing just y in position="10 20" -> position="10 30"
 *  emits a single position.y write). attrName is the XML attribute (position / size / padding /
 *  scale / center / ...); kind selects the parse+expand rule. Returns null when either value is
 *  malformed or has the wrong component count, so the caller falls back to a full reparse. An empty
 *  array means the values are equivalent (e.g. padding "10" vs "10 10 10 10"): no write is needed.
 *  Emitting only changed components keeps each sub-channel's own layout/anim flag accurate (a
 *  position.y edit must not redundantly re-drive position.x). */
function splitCompositeChange(
    attrName: string,
    kind: 'point' | 'size' | 'padding',
    oldValue: string,
    newValue: string,
): SubEdit[] | null {
    if (kind === 'padding') {
        const oldPad = expandPadding(oldValue);
        const newPad = expandPadding(newValue);
        if (oldPad === null || newPad === null) {
            return null;
        }
        const edits: SubEdit[] = [];
        const sides: Array<keyof typeof newPad> = ['left', 'top', 'right', 'bottom'];
        for (const side of sides) {
            if (Number.parseFloat(oldPad[side]) !== Number.parseFloat(newPad[side])) {
                edits.push({ channel: `${attrName}.${side}`, value: newPad[side] });
            }
        }
        return edits;
    }
    const oldTokens = pointTokens(oldValue);
    const newTokens = pointTokens(newValue);
    if (oldTokens === null || newTokens === null) {
        return null;
    }
    // point -> .x/.y, size -> .width/.height, matching the channel table's sub-channel names.
    const suffixes = kind === 'size' ? ['width', 'height'] : ['x', 'y'];
    const edits: SubEdit[] = [];
    for (let i = 0; i < suffixes.length; i++) {
        if (Number.parseFloat(oldTokens[i]) !== Number.parseFloat(newTokens[i])) {
            edits.push({ channel: `${attrName}.${suffixes[i]}`, value: newTokens[i] });
        }
    }
    return edits;
}

/** Returns the two component token strings of a point/size value, or null when it does not hold
 *  exactly two numbers (the importer's ParseTwoFloats requires both). Token strings (not reparsed
 *  numbers) are returned so an unchanged component round-trips byte-identically. */
function pointTokens(value: string): [string, string] | null {
    const tokens = value.trim().split(/[\s,]+/).filter((token) => token.length > 0);
    if (tokens.length !== 2 || parseFloatTokens(value) === null) {
        return null;
    }
    return [tokens[0], tokens[1]];
}

/** Result of classifying one node's span: either the incremental channel writes, or a human-
 *  readable reason the edit cannot go incremental (surfaced via console.debug for diagnosing why an
 *  Apply fell back to a full reparse). */
type NodeSpanClassification = { edits: ChannelEdit[] } | { reason: string };

/** Classifies the change to one node's source span into per-attribute channel writes, or a reason
 *  it cannot go incremental. Only pure own-attribute value changes on whitelisted channels qualify:
 *  the tag name and attribute key set must be unchanged, every changed attribute must be an
 *  incrementable channel, and — crucially — after applying those value changes to the old element
 *  it must serialize identically to the new one. That exact-match check guarantees no other
 *  difference (child element, text content, attribute reordering, or a change inside embedded
 *  non-node content like an <svg> subtree) is silently dropped: any residual difference forces a
 *  full reparse. */
function classifyNodeSpan(
    oldSpan: string,
    newSpan: string,
    channels: string[],
): NodeSpanClassification {
    const oldElement = parseSpanElement(oldSpan);
    const newElement = parseSpanElement(newSpan);
    if (oldElement === null || newElement === null) {
        return { reason: 'node span is not a single well-formed element (unparseable fragment)' };
    }
    if (oldElement.tagName !== newElement.tagName) {
        return { reason: `tag name changed (${oldElement.tagName} -> ${newElement.tagName})` };
    }
    const oldAttrs = attributeMap(oldElement);
    const newAttrs = attributeMap(newElement);
    const oldKeys = Object.keys(oldAttrs);
    const newKeys = Object.keys(newAttrs);
    if (oldKeys.length !== newKeys.length) {
        return { reason: `attribute added or removed on <${oldElement.tagName}>` };
    }
    const edits: ChannelEdit[] = [];
    for (const key of newKeys) {
        if (!(key in oldAttrs)) {
            return { reason: `attribute renamed on <${oldElement.tagName}> (new key "${key}")` };
        }
        if (oldAttrs[key] === newAttrs[key]) {
            continue;
        }
        if (channels.includes(key)) {
            edits.push({ index: -1, channel: key, value: newAttrs[key] });
            continue;
        }
        // Composite attribute (position / size / padding / ...): the channel table addresses it as
        // dotted sub-channels, so split the change into per-component writes and keep only the
        // components that actually changed. The value is re-parsed exactly as the importer would, so
        // an incremental sub-channel write reproduces the full-reparse result.
        const compositeKind = COMPOSITE_ATTRIBUTES.get(key);
        if (compositeKind !== undefined) {
            const subEdits = splitCompositeChange(key, compositeKind, oldAttrs[key], newAttrs[key]);
            if (subEdits === null) {
                return {
                    reason: `composite attribute "${key}" on <${oldElement.tagName}> is malformed or has an unexpected component count`,
                };
            }
            for (const subEdit of subEdits) {
                if (!channels.includes(subEdit.channel)) {
                    return {
                        reason: `composite attribute "${key}" maps to sub-channel "${subEdit.channel}" which is not incrementable on <${oldElement.tagName}>`,
                    };
                }
                edits.push({ index: -1, channel: subEdit.channel, value: subEdit.value });
            }
            continue;
        }
        return {
            reason: `attribute "${key}" on <${oldElement.tagName}> is not an incrementable channel (composite or unsupported)`,
        };
    }
    // Round-trip check against the ORIGINAL attribute names (not the split sub-channels): write each
    // changed attribute's new value back onto the old element under its own name, then require a
    // byte-identical serialization. This guarantees the only differences between the spans are the
    // attribute values we accounted for above; any residual difference (child element, text content,
    // attribute reorder, embedded <svg> subtree change) still forces a full reparse. Done with the
    // original names so a composite like position stays a single attribute here even though it was
    // emitted as position.x / position.y sub-channel writes.
    for (const key of newKeys) {
        if (oldAttrs[key] !== newAttrs[key]) {
            oldElement.setAttribute(key, newAttrs[key]);
        }
    }
    const serializer = new XMLSerializer();
    if (serializer.serializeToString(oldElement) !== serializer.serializeToString(newElement)) {
        return {
            reason: `<${oldElement.tagName}> span differs beyond the edited attributes (child/text/embedded content changed)`,
        };
    }
    return { edits };
}

export class PAGXPlayer extends EventTarget {
    private readonly options: PAGXPlayerOptions;
    private readonly gesture: GestureManager;
    private readonly root: HTMLDivElement;
    private readonly canvas: HTMLCanvasElement;
    private readonly sizeContainer: HTMLElement;
    private readonly toolbarRoot: HTMLDivElement;
    private readonly playbackBar: PlaybackBar;
    private readonly editor: EditorPanel | null = null;
    private readonly statusEl: HTMLDivElement;

    private module: PlayerModule | null = null;
    private view: PlayerView | null = null;
    private detachCanvasEvents: (() => void) | null = null;
    private statusHideTimer: number | null = null;
    // Monotonically increasing id for each showStatus call; the caller can hold this token
    // and pass it to hideStatus() to only clear the pill when their own message is still on
    // screen. See showStatus / hideStatus for the full story.
    private statusTokenSeq = 0;
    private currentStatusToken = 0;

    // --- Selection mode (phase 1: canvas<->editor highlighting) ---
    private selectMode = false;
    private hoverIndex = -1;
    // Node index the editor is hovering (editor->canvas direction). Independent of selectMode:
    // hovering an editor line always highlights the corresponding node on the canvas.
    private editorHoverIndex = -1;
    private selectedIndex = -1;
    private sourceMap: NodeSourceEntry[] = [];
    private overlay: HTMLDivElement | null = null;
    // The bounds-bearing node the overlay currently paints (resolved from the hover/select target
    // by climbing to the owning Layer for internal elements), and which visual state to render.
    // Cached by refreshOverlay so the per-frame follow-loop skips the ancestry walk.
    private overlayBoundsIndex = -1;
    private overlayKind: 'hover' | 'select' = 'hover';
    private overlayRaf = 0;
    private hoverRaf = 0;
    private detachHover: (() => void) | null = null;

    // Concurrency + lifetime guards. Every load() captures loadGeneration on entry and
    // re-reads it after each await; a mismatch means a newer load() (or destroy() / non-BFCache
    // pagehide) has superseded this one, so the resumed call bails out before touching the
    // shared view. `destroyed` is checked separately so post-destroy resumptions never dispatch
    // events or interact with a torn-down wasm instance. See load() for the actual gates.
    private destroyed = false;
    private loadGeneration = 0;
    // Separate epoch for the wasm view lifetime. Bumped alongside destroyView() so that an
    // in-flight initView() awaiting moduleFactory() when the view slot is torn down can detect
    // the tear-down after the await and drop the view it was about to create. Without this an
    // initView() call started before pagehide would happily populate `this.view` after
    // pagehide, leaking a document-less view that no subsequent load() could ever reach
    // (because load() bails on the generation mismatch).
    private viewEpoch = 0;
    // In-flight initialization promise, cached so parallel first-time load() calls share a
    // single moduleFactory() + PAGXView.init() pass instead of racing to allocate two views.
    // Reset to null after the promise settles so a post-destroy re-init (should the host
    // ever revive the player) can restart clean.
    private initPromise: Promise<void> | null = null;

    private readonly onVisibilityChange: () => void;
    private readonly onWindowResize: () => void;
    private readonly onKeyDown: (event: KeyboardEvent) => void;
    private readonly onBeforeUnload: () => void;
    private readonly onPageHide: (event: PageTransitionEvent) => void;
    private readonly onPageShow: (event: PageTransitionEvent) => void;
    private resizeObserver: ResizeObserver | null = null;

    constructor(options: PAGXPlayerOptions) {
        super();
        // Validate configuration BEFORE creating any DOM / timers / listeners so an invalid
        // config throws with the player left in a state that requires no cleanup. Callers
        // that catch the throw won't have leaked a partially-constructed player - no wrapper
        // in their container, no dangling setInterval in the playback bar.
        if (options.enableEditor && !options.editorCallbacks) {
            throw new Error('PAGXPlayer: editorCallbacks is required when enableEditor is true');
        }
        this.options = options;
        ensureStylesInjected();

        const container = options.container;

        // Component-owned root wrapper. All player DOM (canvas, status, toolbar, playback bar,
        // editor) lives here so layout state (`.with-editor` class, `--editor-width` variable,
        // z-index stacking, overflow: hidden) is scoped to a node the component fully controls.
        // Hosts only need to size options.container; they don't ship any component-specific CSS.
        this.root = document.createElement('div');
        this.root.className = 'pagx-player-root';
        container.appendChild(this.root);

        // Canvas sizing follows the wrapper so opening the editor (which shrinks the wrapper
        // via `.with-editor`) reflows the canvas in the same frame. Falling back to the host
        // container - like earlier revisions did - meant ResizeObserver never noticed the
        // shrink (only the wrapper's inner width changed, host stayed full-width), so the
        // backing store stayed at full width and got clipped by the wrapper's overflow.
        // Hosts can still override via canvasSizeContainer if they layer additional chrome.
        this.sizeContainer = options.canvasSizeContainer ?? this.root;

        // Canvas
        this.canvas = document.createElement('canvas');
        this.canvas.id = CANVAS_ID;
        this.canvas.className = 'pagx-player-canvas canvas hidden';
        this.root.appendChild(this.canvas);

        // Status pill. Anchored above the canvas area; inherits the wrapper's shrink-with-editor
        // behavior automatically since it's a child of the same wrapper as the toolbar and
        // playback bar. Starts hidden so nothing shows until the host calls showStatus().
        this.statusEl = document.createElement('div');
        this.statusEl.className = 'pagx-player-status hidden';
        this.root.appendChild(this.statusEl);

        // Toolbar (before/after slots come from host; built-in Reset + Source Editor sit
        // between them so their relative order in options.extraMenuItems is preserved).
        this.toolbarRoot = buildToolbar(
            {
                onResetView: () => this.resetView(),
                onToggleEditor: options.enableEditor ? () => this.editor?.toggle() : undefined,
            },
            options.extraMenuItems ?? {},
        );
        this.root.appendChild(this.toolbarRoot);

        // Playback bar
        this.playbackBar = new PlaybackBar({
            parent: this.root,
            iconBaseUrl: options.iconBaseUrl,
            getView: () => this.view,
            callbacks: {
                onFrameChange: (t) => this.emitFrameChange(t),
                onPlay: () => this.dispatchEvent(new CustomEvent('play')),
                onPause: () => this.dispatchEvent(new CustomEvent('pause')),
                onSeek: (t) => this.dispatchEvent(new CustomEvent('seek', { detail: { currentTimeMicros: t } })),
            },
        });

        // Editor (optional). Editor feedback ("Changes applied", validation errors, etc.) is
        // routed into this.showStatus so it lands in the same status slot as load/reload
        // status. Editor keeps calling notify() unaware of the pill; the player controls the
        // visual channel from a single place. canvasContainer is the wrapper root so the
        // editor's `with-editor` class hooks into the component-owned stylesheet.
        if (options.enableEditor) {
            // editorCallbacks presence was verified at constructor entry; non-null assertion is
            // safe here because the throw above happens before this point.
            this.editor = new EditorPanel({
                parent: this.root,
                canvasContainer: this.root,
                callbacks: options.editorCallbacks!,
                onToggleSelect: () => this.toggleSelectMode(),
                onClose: () => {
                    // Exit select mode whenever the editor closes (manual close, document switch,
                    // hide) so the canvas stops driving highlights against a hidden editor.
                    if (this.selectMode) {
                        this.toggleSelectMode();
                    }
                },
                notify: (message, kind, notifyOptions) => {
                    // Sticky messages ("Applying...", "Saving...") don't auto-hide; the editor
                    // will call notify() again with the resolved result and that replaces the
                    // pill in place. Non-sticky messages fall back to the shared timeout. The
                    // returned token flows back to the editor so it can precisely dismiss
                    // stale progress messages without stealing another producer's pill.
                    return this.showStatus(message, {
                        kind,
                        autoHideMs: notifyOptions?.sticky ? 0 : EDITOR_STATUS_DURATION_MS,
                    });
                },
                dismiss: (token) => this.hideStatus(token),
                incrementalApply: (oldXml, newXml) => this.tryIncrementalApply(oldXml, newXml),
            });
        }
        // Wire editor->canvas interactions. Hover highlights the node on the canvas + mirrors the
        // grey highlight on the editor line; double-click unlocks the enclosing node's span for
        // editing. Registered once here; EditorPanel re-applies both across editor rebuilds.
        if (this.editor) {
            this.editor.onHoverLine((line) => this.onEditorHover(line));
            this.editor.onDblClickLine((line) => this.onEditorDblClick(line));
            this.editor.onCursorLine((line) => this.onEditorCursor(line));
        }

        // Gesture manager (view accessor closure keeps working across reloads)
        this.gesture = new GestureManager(() => this.view);
        this.detachCanvasEvents = bindCanvasEvents(this.canvas, this.gesture, (x, y) => {
            if (this.selectMode) {
                this.confirmSelection(x, y);
            } else {
                this.playbackBar.togglePlayback();
            }
        });
        // Hover tracking for selection mode. Bound separately from bindCanvasEvents (which owns
        // tap/drag/wheel) so select-mode hover never interferes with playback gestures.
        this.detachHover = this.bindHover();

        // Lifecycle listeners
        this.onVisibilityChange = () => {
            if (document.hidden) {
                this.view?.stop();
            } else {
                this.view?.start();
            }
        };
        this.onWindowResize = () => this.updateSize();
        this.onKeyDown = (event) => this.handleKeyDown(event);
        this.onBeforeUnload = () => this.view?.stop();
        // Two flavors of pagehide:
        //  - Real unload (event.persisted === false): the page is going away, wasm resources
        //    should be released now. Bump loadGeneration + viewEpoch and destroyView() so any
        //    in-flight load() / initView() bails out on its next await instead of writing to
        //    the torn-down slot.
        //  - BFCache freeze (event.persisted === true): the page is being frozen so the user
        //    can navigate back; DOM state (including this player instance) is preserved. We
        //    only stop() the render loop; keeping the wasm view alive means the same document
        //    resurfaces instantly on pageshow with no reload. destroyed stays false in both
        //    cases so a subsequent explicit load() from the host can still spin up a fresh
        //    view via ensureView(); destroy() is the only path that permanently disables the
        //    player.
        this.onPageHide = (event) => {
            if (event.persisted) {
                this.view?.stop();
                return;
            }
            this.loadGeneration++;
            this.destroyView();
        };
        // Complementary pageshow: BFCache freezes on pagehide leave the render loop stopped,
        // so restart it on restore. `event.persisted` is true when the page came out of BFCache
        // (fresh navigations still fire pageshow but with persisted=false and view already
        // running / not yet loaded, so the guard leaves them alone).
        this.onPageShow = (event) => {
            if (event.persisted && this.view) {
                this.view.start();
            }
        };
        document.addEventListener('visibilitychange', this.onVisibilityChange);
        window.addEventListener('resize', this.onWindowResize);
        document.addEventListener('keydown', this.onKeyDown);
        window.addEventListener('beforeunload', this.onBeforeUnload);
        window.addEventListener('pagehide', this.onPageHide);
        window.addEventListener('pageshow', this.onPageShow);

        // ResizeObserver on the size container: catches side-panel opens (editor) and window
        // resizes that don't trigger the window 'resize' event (rare, but observed on Safari
        // when only the container shrinks while the window itself stays the same size).
        //
        // We sync updateSize() AND draw() in the same rAF callback: PAGXView::draw() detects
        // canvas drawing-buffer size changes and rebuilds its render surface in the same
        // frame, so pairing the two keeps resize + new frame within a single browser paint
        // tick. Without the synchronous draw() the newly resized canvas would paint with the
        // stale surface content until the internal render loop catches up on the next rAF,
        // producing visible flicker when the user drags the editor resizer at ~60Hz. rAF
        // throttle is retained so multiple observer firings per frame collapse into one
        // pipeline pass.
        if (typeof ResizeObserver !== 'undefined') {
            let pending: number | null = null;
            this.resizeObserver = new ResizeObserver(() => {
                if (pending !== null) return;
                pending = window.requestAnimationFrame(() => {
                    pending = null;
                    // updateSize() itself skips zero-sized rects to protect the GL backing
                    // store when a host ancestor is display:none (samples overlay, hidden
                    // tab, etc.), so we don't need a separate guard here.
                    this.updateSize();
                    this.view?.draw();
                });
            });
            this.resizeObserver.observe(this.sizeContainer);
        }
    }

    /** Load a pagx from raw bytes. Handles the full pipeline: ensure wasm module, parse the
     *  document, register fonts (via callback), resolve external resources (via callback),
     *  build layers, first draw. Emits 'loaded' on success and 'loadError' on failure.
     *
     *  Concurrent load() calls, and destroy() during a load(), are safe: this call captures a
     *  generation token on entry and bails out silently after each await when a newer call
     *  has taken over. Only the winning call emits 'loaded' / 'loadError'; older ones return
     *  without dispatching so hosts don't see stale results. */
    public async load(pagxBytes: Uint8Array, options: PAGXPlayerLoadOptions = {}): Promise<void> {
        if (this.destroyed) {
            return;
        }
        const generation = ++this.loadGeneration;
        // [ORT] Optimize Release Test - full-load timing instrumentation. Remove all `[ORT]`
        // lines when the comparison is done.
        const ortT0 = performance.now();
        console.log(`[ORT] load() start: ${pagxBytes.length} bytes`);
        // Snapshots taken while it's still safe to read pre-existing view state; used after
        // the reset() inside parsePAGX() to restore playback position and play/pause state.
        try {
            await this.ensureView();
            console.log(`[ORT] ensureView (wasm ready): +${(performance.now() - ortT0).toFixed(1)}ms`);
            if (!this.isCurrentLoad(generation)) return;
            const view = this.view!;

            const preservedTime = options.preserveCurrentTime
                ? (view.durationMicros() > 0 ? view.currentTimeMicros() : 0)
                : 0;
            const wasPlaying = options.preserveCurrentTime && view.isPlaying();

            const ortParseStart = performance.now();
            view.parsePAGX(pagxBytes);
            console.log(`[ORT] parsePAGX: ${(performance.now() - ortParseStart).toFixed(1)}ms (total +${(performance.now() - ortT0).toFixed(1)}ms)`);

            // Register fonts (host-supplied); the view resets font registration on each load
            // so this must run before buildLayers.
            if (options.registerFonts) {
                await options.registerFonts(view);
                if (!this.isCurrentLoad(generation)) return;
            }

            // External resource resolution: parsePAGX populates the resource path list; each
            // path is fetched and pushed as file data before buildLayers can succeed. Fetch
            // concurrently but push in order so a hypothetical duplicate path list stays
            // deterministic.
            if (options.resolveResource) {
                const ortResStart = performance.now();
                const paths: string[] = view.getExternalFilePaths();
                if (paths && paths.length > 0) {
                    const resolve = options.resolveResource;
                    const items = await Promise.all(
                        paths.map(async (rel: string) => ({
                            rel,
                            buf: await resolve(rel),
                        })),
                    );
                    if (!this.isCurrentLoad(generation)) return;
                    for (const item of items) {
                        if (item.buf) {
                            view.loadFileData(item.rel, item.buf);
                        }
                    }
                    console.log(`[ORT] resolveResource: ${paths.length} file(s) in ${(performance.now() - ortResStart).toFixed(1)}ms (total +${(performance.now() - ortT0).toFixed(1)}ms)`);
                }
            }

            const ortBuildStart = performance.now();
            view.buildLayers();
            console.log(`[ORT] buildLayers: ${(performance.now() - ortBuildStart).toFixed(1)}ms (total +${(performance.now() - ortT0).toFixed(1)}ms)`);

            if (options.forceLoop !== false) {
                view.setLoop(true);
            }

            const bg = this.options.backgroundColor ?? DEFAULT_BACKGROUND;
            // Compose an #RRGGBB hex from the numeric RGB channels so the setBackgroundColor
            // string overload accepts it; alpha is passed separately as a 0..1 float per the
            // pagx-viewer API contract.
            const hex =
                '#' +
                bg.r.toString(16).padStart(2, '0') +
                bg.g.toString(16).padStart(2, '0') +
                bg.b.toString(16).padStart(2, '0');
            view.setBackgroundColor(hex, bg.a);

            if (options.preserveCurrentTime && preservedTime > 0) {
                view.setCurrentTimeMicros(Math.min(preservedTime, view.durationMicros()));
            }
            // Fully mirror the previous play state when the host asked us to preserve it:
            // buildLayers() leaves the view in the auto-play state by default, so a paused
            // reload would otherwise silently start playing again after every SSE tick.
            if (options.preserveCurrentTime) {
                if (wasPlaying) {
                    view.play();
                } else {
                    view.pause();
                }
            } else {
                // A non-preserving load means the host is showing a different pagx document
                // (open-new-file, sample switch), not a live-reload of the current one. Reset
                // the zoom/pan transform so the new file lands centered at identity instead
                // of inheriting the previous document's user gestures. When preserveCurrentTime
                // is true we intentionally leave the transform alone: users mid-scrub during
                // an SSE reload expect their viewport to stay put.
                this.gesture.resetTransform();
            }

            // Force a synchronous first frame so the canvas reflects the newly built document
            // before we unhide it below; without this the view is drawn one rAF later and the
            // previous document's last frame briefly ghosts through when hidden -> visible.
            const ortDrawStart = performance.now();
            view.draw();
            console.log(`[ORT] first draw: ${(performance.now() - ortDrawStart).toFixed(1)}ms (total +${(performance.now() - ortT0).toFixed(1)}ms)`);

            this.canvas.classList.remove('hidden');
            setToolbarVisible(this.toolbarRoot, true);
            const hasAnimation = view.durationMicros() > 0;
            this.playbackBar.setVisible(hasAnimation);

            // Feed the editor with the freshly loaded XML. If the host pre-decoded it, we use
            // that; otherwise we decode the bytes so the editor always has something to show
            // when the user hits L. Encoding is assumed UTF-8, which is what pagx serializes to.
            let xmlText: string | undefined = options.xmlText;
            if (this.editor && !xmlText) {
                xmlText = new TextDecoder('utf-8').decode(pagxBytes);
            }
            this.editor?.setDocumentXml(xmlText ?? null);

            // Refresh the source map (node index -> source span) for selection highlighting.
            // Rebuilt on every load since a new document renumbers nodes.
            const ortMapStart = performance.now();
            this.sourceMap = view.getNodeSourceMap();
            console.log(`[ORT] getNodeSourceMap: ${(performance.now() - ortMapStart).toFixed(1)}ms, ${this.sourceMap.length} node(s) (total +${(performance.now() - ortT0).toFixed(1)}ms)`);
            this.hoverIndex = -1;
            this.editorHoverIndex = -1;
            this.selectedIndex = -1;
            this.editor?.clearHighlight();
            this.refreshOverlay();

            const detail: LoadedEventDetail = {
                duration: view.durationMicros(),
                frameRate: view.frameRate(),
                hasAnimation,
                xmlText,
            };
            this.dispatchEvent(new CustomEvent('loaded', { detail }));
            console.log(`[ORT] load() done (full load path): total ${(performance.now() - ortT0).toFixed(1)}ms`);
        } catch (err) {
            // Only the currently active load reports the failure. Superseded loads dropping
            // out via isCurrentLoad() never enter this branch since generation checks come
            // before every await point that can throw synchronously afterwards; a lingering
            // error from an older call would just be swallowed here.
            if (!this.isCurrentLoad(generation)) return;
            // Failure-recovery: parsePAGX() at the top of the pipeline already dropped the
            // previous document, and buildLayers() may have run against a half-populated
            // scene, so keeping any of the pre-failure UI visible would misrepresent the
            // actual state (playback bar showing the old duration, canvas ghosting the old
            // frame, editor keeping its previous baseline as the "last accepted" xml). We
            // deliberately do NOT try to restore the previous document: the byte source is
            // owned by the host, and recomputing it here would either need a second round
            // trip or a stashed copy that itself becomes stale on the next successful load.
            // Instead we clear back to the initial empty state: destroy the failed view so
            // the next load() re-inits from scratch, hide the canvas / toolbar / playback
            // bar, and clear the editor baseline. Hosts can retry load() with the previous
            // (or a new) byte buffer and the player will start fresh.
            this.canvas.classList.add('hidden');
            setToolbarVisible(this.toolbarRoot, false);
            this.playbackBar.setVisible(false);
            this.editor?.setDocumentXml(null);
            this.destroyView();
            const error = err instanceof Error ? err : new Error(String(err));
            const detail: LoadErrorEventDetail = { error };
            this.dispatchEvent(new CustomEvent('loadError', { detail }));
            throw error;
        }
    }

    /** Show the player DOM subtree (canvas / toolbar / playback bar). Editor visibility is
     *  driven independently by user action. */
    public show(): void {
        this.canvas.classList.remove('hidden');
        setToolbarVisible(this.toolbarRoot, true);
        if (this.view && this.view.durationMicros() > 0) {
            this.playbackBar.setVisible(true);
        }
    }

    /** Hide the player DOM subtree without destroying the view. Used by hosts that route
     *  between a home page and the player (e.g. playground's goHome). Every user-facing
     *  surface owned by the player is hidden (canvas, toolbar, playback bar, editor panel,
     *  status pill) and playback is paused so an off-screen document doesn't keep advancing
     *  frames. Hosts that want playback to resume after a subsequent show() should call
     *  play() themselves - show() intentionally does not auto-play, so a user who paused
     *  before goHome() stays paused on return. */
    public hide(): void {
        this.canvas.classList.add('hidden');
        setToolbarVisible(this.toolbarRoot, false);
        this.playbackBar.setVisible(false);
        this.editor?.close();
        this.hideStatus();
        this.view?.pause();
    }

    /** Restore identity transform (zoom 1.0, offset 0,0). Also fired by the toolbar Reset button. */
    public resetView(): void {
        this.gesture.resetTransform();
    }

    /** Toggle inspect (selection) mode. When on, canvas hover/click drive editor line
     *  highlighting instead of toggling playback. Auto-opens the editor so the canvas<->XML
     *  hover highlight is visible (DevTools-like). */
    public toggleSelectMode(): void {
        if (this.selectMode) {
            this.exitSelectMode();
        } else {
            this.selectMode = true;
            this.editor?.setSelectMode(true);
            this.editor?.open();
            this.canvas.style.cursor = 'crosshair';
            this.refreshOverlay();
        }
    }

    /** Leaves inspect mode: stops canvas-driven hover hit-testing and clears the transient grey
     *  hover highlight. The blue selection (from a canvas click or editor double-click) and
     *  editor-hover highlighting are independent and stay live. */
    private exitSelectMode(): void {
        if (!this.selectMode) {
            return;
        }
        this.selectMode = false;
        this.editor?.setSelectMode(false);
        this.canvas.style.cursor = '';
        this.hoverIndex = -1;
        this.syncEditorHover();
        this.refreshOverlay();
    }

    // --- Selection (phase 1) private helpers ---

    /** Binds a mousemove listener (rAF-throttled) for select-mode hover hit-testing. Returns a
     *  cleanup function. */
    private bindHover(): () => void {
        const handler = (event: MouseEvent) => {
            if (!this.selectMode || this.gesture.isCurrentlyDragging()) {
                return;
            }
            const cx = event.clientX;
            const cy = event.clientY;
            if (this.hoverRaf !== 0) {
                cancelAnimationFrame(this.hoverRaf);
            }
            this.hoverRaf = requestAnimationFrame(() => {
                this.hoverRaf = 0;
                this.handleHover(cx, cy);
            });
        };
        this.canvas.addEventListener('mousemove', handler);
        return () => this.canvas.removeEventListener('mousemove', handler);
    }

    private handleHover(clientX: number, clientY: number): void {
        if (!this.view || !this.selectMode) {
            return;
        }
        const { surfaceX, surfaceY } = this.clientToSurface(clientX, clientY);
        const idx = this.view.hitTest(surfaceX, surfaceY);
        if (idx !== this.hoverIndex) {
            this.hoverIndex = idx;
            this.refreshOverlay();
            this.syncEditorHover('start');
        }
    }

    /** Selects the node under the click point (inspect-mode click). Hit-tests the live click
     *  coordinates rather than relying on the cached hover index, which may be stale or unset when
     *  the pointer barely moved before the click (the rAF-throttled hover may not have run). */
    private confirmSelection(clientX: number, clientY: number): void {
        if (!this.view) {
            return;
        }
        const { surfaceX, surfaceY } = this.clientToSurface(clientX, clientY);
        const idx = this.view.hitTest(surfaceX, surfaceY);
        // Missed the geometry (clicked empty canvas): keep the inspector armed so the user can
        // try again rather than silently dropping out of inspect mode.
        if (idx < 0) {
            return;
        }
        this.selectedIndex = idx;
        // DevTools-style: picking a layer deactivates the inspector. exitSelectMode() clears the
        // transient hover and repaints the overlay from the now-set selection, so only the sticky
        // blue outline remains; it stays highlighted on the canvas and mirrored in the editor
        // until the user's next action.
        this.exitSelectMode();
        this.syncEditorSelect('start');
    }

    /** Editor line hover -> canvas overlay + grey editor-line highlight (editor->canvas direction).
     *  Always active while the editor is open, independent of selectMode. line <= 0 clears. */
    private onEditorHover(line: number): void {
        const idx = line > 0 ? this.findNodeIndexForLine(line) : -1;
        if (idx === this.editorHoverIndex) {
            return;
        }
        this.editorHoverIndex = idx;
        this.refreshOverlay();
        this.syncEditorHover();
    }

    /** Editor double-click -> select the enclosing node (blue) and unlock its source span for
     *  editing. */
    private onEditorDblClick(line: number): void {
        const idx = this.findNodeIndexForLine(line);
        if (idx < 0) {
            return;
        }
        this.selectedIndex = idx;
        const entry = this.sourceMap[idx];
        const end = entry.endLine > 0 ? entry.endLine : entry.startLine;
        this.editor?.enterEditRange(entry.startLine, end);
        this.refreshOverlay();
        this.syncEditorSelect();
    }

    /** Editor caret moved (while a span is unlocked for editing) -> re-scope the editable span
     *  and blue selection to the node the caret now sits in. Fired only in edit mode (the editor
     *  gates the callback on the unlocked range), so browsing/read-only caret moves never reach
     *  here. No-op while the caret stays inside the current node's span. */
    private onEditorCursor(line: number): void {
        const idx = this.findNodeIndexForLine(line);
        if (idx < 0 || idx === this.selectedIndex) {
            return;
        }
        this.selectedIndex = idx;
        const entry = this.sourceMap[idx];
        const end = entry.endLine > 0 ? entry.endLine : entry.startLine;
        this.editor?.updateEditRange(entry.startLine, end);
        this.syncEditorSelect();
        this.refreshOverlay();
    }

    /** Returns the index of the innermost node whose [startLine, endLine] span contains the given
     *  1-based line, or -1 if none. endLine < 0 (programmatic nodes) falls back to a single-line
     *  match at startLine. */
    private findNodeIndexForLine(line: number): number {
        let bestIndex = -1;
        let bestSpan = Number.POSITIVE_INFINITY;
        for (const entry of this.sourceMap) {
            const start = entry.startLine;
            if (start <= 0 || start > line) {
                continue;
            }
            const end = entry.endLine > 0 ? entry.endLine : start;
            if (line > end) {
                continue;
            }
            const span = end - start;
            if (span < bestSpan) {
                bestSpan = span;
                bestIndex = entry.index;
            }
        }
        return bestIndex;
    }

    /** Attempts to apply the edit from oldXml to newXml incrementally, in place, via setNodeChannel
     *  instead of a full reparse+rebuild. Returns true only when the whole edit is a set of pure
     *  attribute-value changes on incrementable channels; false otherwise, signalling the editor to
     *  fall back to the full onApply pipeline. Failing (even part-way) is safe: the fallback reparse
     *  of newXml is the authoritative final state regardless of any channel writes already applied
     *  here. */
    private tryIncrementalApply(oldXml: string, newXml: string): boolean {
        // [ORT] Optimize Release Test - incremental-apply timing. This is the fast path the
        // optimization adds; compare its total against buildLayers on the full-load path.
        const ortT0 = performance.now();
    if (!this.view || this.sourceMap.length === 0) {
      return false;
        }
        const edits = this.classifyEdits(oldXml, newXml);
        if (edits === null) {
            console.log(`[ORT] incremental: NOT applicable, fall back to full reparse (+${(performance.now() - ortT0).toFixed(1)}ms)`);
            return false;
        }
        if (edits.length === 0) {
            console.log(`[ORT] incremental: no-op (0 edits) in ${(performance.now() - ortT0).toFixed(1)}ms`);
            return true;
        }
        for (const edit of edits) {
            if (!this.view.setNodeChannel(edit.index, edit.channel, edit.value)) {
                console.debug(
                    `[pagx] full reparse: engine rejected setNodeChannel #${edit.index}.${edit.channel}="${edit.value}" (unknown channel or unparseable value)`,
                );
                console.log(`[ORT] incremental: setNodeChannel rejected, fall back to full reparse (+${(performance.now() - ortT0).toFixed(1)}ms)`);
                return false;
            }
        }
        this.view.draw();
        const summary = edits
            .map((edit) => `#${edit.index}.${edit.channel}="${edit.value}"`)
            .join(', ');
           console.log(`[pagx] incremental subtree update applied: ${summary}`);
        console.log(`[ORT] incremental apply done (fast path): ${edits.length} edit(s) in ${(performance.now() - ortT0).toFixed(1)}ms`);
        return true;
    }

    /** Classifies a text edit into a flat list of incremental channel writes, or null when it
     *  cannot go incremental. First-version constraints (each a null-return): the line count
     *  changed (would shift the cached source spans), a changed line lies outside any node, or a
     *  node's own change is not a pure whitelisted-channel value edit (see classifyNodeSpan). */
    private classifyEdits(oldXml: string, newXml: string): ChannelEdit[] | null {
        const oldLines = oldXml.split('\n');
        const newLines = newXml.split('\n');
        if (oldLines.length !== newLines.length) {
            console.debug(
                `[pagx] full reparse: line count changed (${oldLines.length} -> ${newLines.length})`,
            );
            return null;
        }
        const affected = new Set<number>();
        for (let i = 0; i < oldLines.length; i++) {
            if (oldLines[i] === newLines[i]) {
                continue;
            }
            const idx = this.findNodeIndexForLine(i + 1);
            if (idx < 0) {
                console.debug(`[pagx] full reparse: changed line ${i + 1} lies outside any node`);
                return null;
            }
            affected.add(idx);
        }
        const edits: ChannelEdit[] = [];
        for (const idx of affected) {
            const entry = this.sourceMap[idx];
            const start = entry.startLine;
            const end = entry.endLine > 0 ? entry.endLine : start;
            if (start <= 0) {
                console.debug(`[pagx] full reparse: node #${idx} has no source span`);
                return null;
            }
            const oldSpan = oldLines.slice(start - 1, end).join('\n');
            const newSpan = newLines.slice(start - 1, end).join('\n');
            const result = classifyNodeSpan(oldSpan, newSpan, entry.channels);
            if ('reason' in result) {
                console.debug(`[pagx] full reparse: node #${idx} - ${result.reason}`);
                return null;
            }
            for (const nodeEdit of result.edits) {
                edits.push({ index: idx, channel: nodeEdit.channel, value: nodeEdit.value });
            }
        }
        return edits;
    }

    /** The transient hover target: editor-hover wins over canvas-hover (mouse can only be in one). */
    private hoverTarget(): number {
        return this.editorHoverIndex >= 0 ? this.editorHoverIndex : this.hoverIndex;
    }

    /** Mirrors the transient hover target onto the editor as the grey hover line highlight. */
    private syncEditorHover(align: 'none' | 'nearest' | 'start' = 'none'): void {
        const idx = this.hoverTarget();
        if (idx < 0 || idx >= this.sourceMap.length || this.sourceMap[idx].startLine <= 0) {
            this.editor?.clearHover();
            return;
        }
        const entry = this.sourceMap[idx];
        const end = entry.endLine > 0 ? entry.endLine : entry.startLine;
        this.editor?.highlightHover(entry.startLine, end);
        if (align !== 'none') {
            this.editor?.scrollToLine(entry.startLine, align);
        }
    }

    /** Mirrors the sticky selection onto the editor as the blue select line highlight. */
    private syncEditorSelect(align: 'none' | 'nearest' | 'start' = 'none'): void {
        const idx = this.selectedIndex;
        if (idx < 0 || idx >= this.sourceMap.length || this.sourceMap[idx].startLine <= 0) {
            this.editor?.clearSelect();
            return;
        }
        const entry = this.sourceMap[idx];
        const end = entry.endLine > 0 ? entry.endLine : entry.startLine;
        this.editor?.highlightSelect(entry.startLine, end);
        if (align !== 'none') {
            this.editor?.scrollToLine(entry.startLine, align);
        }
    }

    private clientToSurface(clientX: number, clientY: number): { surfaceX: number; surfaceY: number } {
        const rect = this.canvas.getBoundingClientRect();
        // hitTest takes surface (backing-store) coordinates; canvas.width is the backing width so
        // this naturally absorbs DPR.
        const scaleX = this.canvas.width / rect.width;
        const scaleY = this.canvas.height / rect.height;
        return {
            surfaceX: (clientX - rect.left) * scaleX,
            surfaceY: (clientY - rect.top) * scaleY,
        };
    }

    private ensureOverlay(): void {
        if (this.overlay !== null) {
            return;
        }
        const overlay = document.createElement('div');
        overlay.className = 'pagx-select-overlay';
        overlay.style.pointerEvents = 'none';
        this.root.appendChild(overlay);
        this.overlay = overlay;
    }

    /** Overlay target: a transient hover (grey) takes priority over the sticky selection (blue),
     *  so hovering temporarily previews another node then reverts to the selection on mouse-out.
     *  Returns the node index and which visual state to render. */
    private currentOverlayTarget(): { index: number; kind: 'hover' | 'select' } {
        const hover = this.hoverTarget();
        if (hover >= 0) {
            return { index: hover, kind: 'hover' };
        }
        return { index: this.selectedIndex, kind: 'select' };
    }

    /** Returns the index of the innermost source node that strictly encloses the given node's
     *  span, or -1 if none. Used to climb from a Layer-internal element (Fill/Rectangle/... which
     *  has no independent canvas outline) up to the Layer that owns it. */
    private enclosingNodeIndex(index: number): number {
        const child = this.sourceMap[index];
        if (!child || child.startLine <= 0) {
            return -1;
        }
        const childStart = child.startLine;
        const childEnd = child.endLine > 0 ? child.endLine : childStart;
        let bestIndex = -1;
        let bestSpan = Number.POSITIVE_INFINITY;
        for (const entry of this.sourceMap) {
            if (entry.index === index || entry.startLine <= 0) {
                continue;
            }
            const start = entry.startLine;
            const end = entry.endLine > 0 ? entry.endLine : start;
            const encloses = start <= childStart && end >= childEnd && (start < childStart || end > childEnd);
            if (!encloses) {
                continue;
            }
            const span = end - start;
            if (span < bestSpan) {
                bestSpan = span;
                bestIndex = entry.index;
            }
        }
        return bestIndex;
    }

    /** Resolves the node whose bounds the overlay should paint. A node with its own bounds (a
     *  Layer) resolves to itself; a Layer-internal element resolves to the nearest ancestor that
     *  does have bounds - i.e. its owning Layer - so hovering/selecting an internal row still
     *  outlines the visible Layer. Returns -1 when nothing in the ancestry has bounds. */
    private resolveBoundsIndex(index: number): number {
        if (index < 0 || !this.view) {
            return -1;
        }
        let cursor = index;
        let guard = 0;
        while (cursor >= 0 && guard++ < 64) {
            if (this.view.getNodeBounds(cursor) !== null) {
                return cursor;
            }
            cursor = this.enclosingNodeIndex(cursor);
        }
        return -1;
    }

    /** Repaints the overlay and starts/stops the follow-loop based on whether any highlight target
     *  exists. Decoupled from selectMode so editor-hover highlighting works with inspect off. The
     *  bounds-bearing index is resolved once here (climbing to the owning Layer for internal
     *  elements) and cached so the per-frame follow-loop doesn't repeat the ancestry walk. */
    private refreshOverlay(): void {
        const raw = this.currentOverlayTarget();
        this.overlayBoundsIndex = raw.index >= 0 ? this.resolveBoundsIndex(raw.index) : -1;
        this.overlayKind = raw.kind;
        if (this.overlayBoundsIndex >= 0) {
            this.startOverlayLoop();
        } else {
            this.stopOverlayLoop();
        }
        this.updateOverlay();
    }

    private updateOverlay(): void {
        this.ensureOverlay();
        const overlay = this.overlay!;
        if (this.overlayBoundsIndex < 0 || !this.view) {
            overlay.style.display = 'none';
            return;
        }
        const bounds = this.view.getNodeBounds(this.overlayBoundsIndex);
        if (bounds === null) {
            overlay.style.display = 'none';
            return;
        }
        const rect = this.canvas.getBoundingClientRect();
        // Surface (backing) -> CSS pixels.
        const scaleX = rect.width / this.canvas.width;
        const scaleY = rect.height / this.canvas.height;
        overlay.style.display = 'block';
        overlay.style.left = bounds.x * scaleX + 'px';
        overlay.style.top = bounds.y * scaleY + 'px';
        overlay.style.width = bounds.w * scaleX + 'px';
        overlay.style.height = bounds.h * scaleY + 'px';
        overlay.classList.toggle('is-selected', this.overlayKind === 'select');
        overlay.classList.toggle('is-hover', this.overlayKind === 'hover');
    }

    private startOverlayLoop(): void {
        if (this.overlayRaf !== 0) {
            return;
        }
        const tick = () => {
            this.updateOverlay();
            this.overlayRaf = requestAnimationFrame(tick);
        };
        this.overlayRaf = requestAnimationFrame(tick);
    }

    private stopOverlayLoop(): void {
        if (this.overlayRaf !== 0) {
            cancelAnimationFrame(this.overlayRaf);
            this.overlayRaf = 0;
        }
    }

    public play(): void {
        const view = this.view;
        if (!view) {
            return;
        }
        const duration = view.durationMicros();
        if (duration <= 0) {
            return;
        }
        // Wrap from the tail back to the start so play() after a single non-looping playback
        // ended restarts the animation instead of no-op'ing. Matches togglePlayback() in the
        // playback bar so keyboard/API/toolbar all agree.
        if (view.currentTimeMicros() >= duration) {
            view.setCurrentTimeMicros(0);
        }
        view.play();
        this.dispatchEvent(new CustomEvent('play'));
    }

    public pause(): void {
        this.view?.pause();
        this.dispatchEvent(new CustomEvent('pause'));
    }

    public togglePlayback(): void {
        this.playbackBar.togglePlayback();
    }

    public openEditor(): void {
        this.editor?.open();
    }

    public closeEditor(): void {
        this.editor?.close();
    }

    /** Show a transient status message centered above the canvas area. Replaces any prior
     *  message; there is only one status slot per player. Call with `autoHideMs` for a timed
     *  message or omit it for a sticky message that only clears when replaced or hidden.
     *
     *  Returns an opaque token identifying this message. The token can be passed to
     *  {@link hideStatus} for scoped cleanup: `hideStatus(token)` only clears the pill when
     *  the token still identifies the currently displayed message, and no-ops when a later
     *  showStatus() has already replaced it. This lets multiple asynchronous producers
     *  (editor progress messages, external reload status, etc.) coexist without each other's
     *  cleanup racing to hide a message that isn't theirs anymore. */
    public showStatus(text: string, options: StatusOptions = {}): number {
        if (this.statusHideTimer !== null) {
            window.clearTimeout(this.statusHideTimer);
            this.statusHideTimer = null;
        }
        const token = ++this.statusTokenSeq;
        this.currentStatusToken = token;
        this.statusEl.textContent = text;
        this.statusEl.classList.toggle('success', options.kind === 'success');
        this.statusEl.classList.toggle('error', options.kind === 'error');
        this.statusEl.classList.remove('hidden');
        const autoHideMs = options.autoHideMs ?? 0;
        if (autoHideMs > 0) {
            this.statusHideTimer = window.setTimeout(() => {
                this.statusHideTimer = null;
                // The timer only fires when no newer showStatus() has replaced the message
                // (each showStatus clears the pending timer above), so we don't re-check the
                // token here.
                this.currentStatusToken = 0;
                this.statusEl.classList.add('hidden');
            }, autoHideMs);
        }
        return token;
    }

    /** Hide the status pill. If `token` is provided, only clears when it still identifies the
     *  currently displayed message; otherwise clears unconditionally. */
    public hideStatus(token?: number): void {
        if (token !== undefined && token !== this.currentStatusToken) {
            return;
        }
        if (this.statusHideTimer !== null) {
            window.clearTimeout(this.statusHideTimer);
            this.statusHideTimer = null;
        }
        this.currentStatusToken = 0;
        this.statusEl.classList.add('hidden');
    }

    /** Escape hatch: exposes the underlying view for hosts that need low-level access
     *  (Performance measurement, custom playback probing, etc.). Prefer the higher-level API
     *  whenever possible so this doesn't turn into a stable extension surface. */
    public getView(): PlayerView | null {
        return this.view;
    }

    // Typed addEventListener / removeEventListener overloads. Backed by the standard
    // EventTarget methods (so no runtime cost); the extra signatures narrow the event object
    // for well-known player events, catching typos like 'loded' at compile time and letting
    // hosts write `e.detail.duration` without casts.
    public addEventListener<K extends keyof PAGXPlayerEventMap>(
        type: K,
        listener: (event: PAGXPlayerEventMap[K]) => void,
        options?: boolean | AddEventListenerOptions,
    ): void;
    public addEventListener(
        type: string,
        listener: EventListenerOrEventListenerObject | null,
        options?: boolean | AddEventListenerOptions,
    ): void;
    public addEventListener(
        type: string,
        listener: EventListenerOrEventListenerObject | null,
        options?: boolean | AddEventListenerOptions,
    ): void {
        super.addEventListener(type, listener, options);
    }

    public removeEventListener<K extends keyof PAGXPlayerEventMap>(
        type: K,
        listener: (event: PAGXPlayerEventMap[K]) => void,
        options?: boolean | EventListenerOptions,
    ): void;
    public removeEventListener(
        type: string,
        listener: EventListenerOrEventListenerObject | null,
        options?: boolean | EventListenerOptions,
    ): void;
    public removeEventListener(
        type: string,
        listener: EventListenerOrEventListenerObject | null,
        options?: boolean | EventListenerOptions,
    ): void {
        super.removeEventListener(type, listener, options);
    }

    /** Full teardown: detach every listener, destroy the wasm view, remove the DOM subtree.
     *  Safe to call from pagehide handlers or router unmount hooks. Increments loadGeneration
     *  so any in-flight load() bails out on its next await checkpoint before touching the
     *  torn-down view. */
    public destroy(): void {
        this.destroyed = true;
        this.loadGeneration++;
        this.detachCanvasEvents?.();
        this.detachCanvasEvents = null;
        this.detachHover?.();
        this.detachHover = null;
        this.stopOverlayLoop();
        if (this.hoverRaf !== 0) {
            cancelAnimationFrame(this.hoverRaf);
            this.hoverRaf = 0;
        }
        if (this.overlay !== null) {
            this.overlay.remove();
            this.overlay = null;
        }
        this.resizeObserver?.disconnect();
        this.resizeObserver = null;
        document.removeEventListener('visibilitychange', this.onVisibilityChange);
        window.removeEventListener('resize', this.onWindowResize);
        document.removeEventListener('keydown', this.onKeyDown);
        window.removeEventListener('beforeunload', this.onBeforeUnload);
        window.removeEventListener('pagehide', this.onPageHide);
        window.removeEventListener('pageshow', this.onPageShow);
        if (this.statusHideTimer !== null) {
            window.clearTimeout(this.statusHideTimer);
            this.statusHideTimer = null;
        }
        this.playbackBar.destroy();
        this.editor?.destroy();
        this.destroyView();
        // Removing the wrapper root cascades child removals (canvas, status, toolbar, playback
        // bar); we still call the child destroy()s above for the ones that own their own
        // listeners / timers.
        this.root.remove();
    }

    // --- private ---

    /** True when the caller's generation is still the active load. False means a newer load()
     *  has taken over or destroy() has been called; the caller should return without touching
     *  shared state so its resumption doesn't clobber the newer pipeline. */
    private isCurrentLoad(generation: number): boolean {
        return !this.destroyed && generation === this.loadGeneration;
    }

    /** Instantiates the wasm module and PAGXView on demand. Idempotent AND race-free: parallel
     *  load() calls that both find `view === null` share the same in-flight promise instead of
     *  each calling moduleFactory() and init() separately (which would leak the loser's view
     *  and destabilize the wasm heap). The promise is cached in `initPromise` until it settles;
     *  post-pagehide the next call finds `view === null` again and starts a fresh init. */
    private async ensureView(): Promise<void> {
        if (this.view) {
            return;
        }
        if (this.initPromise) {
            await this.initPromise;
            return;
        }
        this.initPromise = this.initView();
        try {
            await this.initPromise;
        } finally {
            this.initPromise = null;
        }
    }

    private async initView(): Promise<void> {
        // Snapshot the view epoch on entry. destroyView() bumps this on real pagehide or
        // destroy(); a mismatch after the await means the slot we were about to fill was
        // already torn down, so we must not create (or immediately-destroy) a fresh view
        // that no subsequent load() can reach.
        const epoch = this.viewEpoch;
        if (!this.module) {
            this.module = await this.options.moduleFactory();
        }
        // A destroy() or non-BFCache pagehide during the moduleFactory() await may have
        // flipped the destroyed flag or torn view state down while we were waiting; abandon
        // this init in that case so the caller's isCurrentLoad() check afterwards bails out
        // cleanly instead of resuming against an orphaned view.
        if (this.destroyed || this.viewEpoch !== epoch) {
            return;
        }
        // Re-check `this.view` after the await: a concurrent ensureView() that resolved the
        // shared initPromise first may have already populated it, in which case creating
        // another view here would leak the earlier one.
        if (this.view) {
            return;
        }
        // Pass the canvas element directly rather than a `#pagx-canvas` selector. Selector
        // form would fail as soon as two PAGXPlayer instances share the same document: their
        // canvases both carry id=pagx-canvas, and querySelector picks the first one, so the
        // second player would try to init on a canvas it doesn't own. Handing the element in
        // decouples init from any document-global lookup and keeps multi-instance use safe.
        const view = this.module.PAGXView.init(this.canvas);
        if (!view) {
            throw new Error('PAGXView.init returned null');
        }
        this.view = view;
        this.updateSize();
        // Seed the view with identity transform + zero background so first draw doesn't flash
        // an unpainted GL surface. background is finalized in load() once the host picks a color.
        view.updateZoomScaleAndOffset(1.0, 0, 0);
        view.setBackgroundColor('#000000', 0);
        view.start();
    }

    private destroyView(): void {
        // Bump the view epoch first so any in-flight initView() awaiting moduleFactory()
        // detects the tear-down on resume and drops its would-be view.
        this.viewEpoch++;
        try {
            this.view?.destroy();
        } catch (_) {
            // View may already be destroyed by an earlier pagehide; ignore.
        }
        this.view = null;
    }

    /** Sync the canvas backing store size to the current DPR-scaled container rect and notify
     *  the wasm view. Called on window resize, ResizeObserver ticks, and after ensureView().
     *  Bails out on zero rects (typically when a host ancestor is display:none, e.g. a route
     *  overlay covering the player): resizing the canvas to 0x0 would destroy its GL drawing
     *  buffer and force a full redraw on return, so we keep the last known good dimensions
     *  and let the next non-zero tick reconcile. */
    private updateSize(): void {
        if (!this.view) {
            return;
        }
        const rect = this.sizeContainer.getBoundingClientRect();
        if (rect.width === 0 || rect.height === 0) {
            return;
        }
        const scaleFactor = window.devicePixelRatio;
        this.canvas.width = rect.width * scaleFactor;
        this.canvas.height = rect.height * scaleFactor;
        this.canvas.style.width = rect.width + 'px';
        this.canvas.style.height = rect.height + 'px';
        this.view.updateSize();
    }

    private emitFrameChange(currentTimeMicros: number): void {
        const view = this.view;
        if (!view) return;
        const rate = view.frameRate();
        const frame = rate > 0 ? Math.round(Math.max(0, currentTimeMicros) * rate / 1_000_000) : 0;
        const totalFrames = rate > 0 ? Math.ceil(view.durationMicros() * rate / 1_000_000) : 0;
        const detail: FrameChangeEventDetail = {
            currentTimeMicros,
            currentFrame: frame,
            totalFrames,
        };
        this.dispatchEvent(new CustomEvent('framechange', { detail }));
    }

    private handleKeyDown(event: KeyboardEvent): void {
        // Space toggles play/pause; ArrowLeft/Right step one frame. Guards mirror playground's
        // keydown handler: text-entry inputs, range slider (arrows scrub natively), and hidden
        // canvas all short-circuit here.
        const isPlayPause = event.code === 'Space';
        const stepDirection = event.code === 'ArrowLeft' ? -1 : event.code === 'ArrowRight' ? 1 : 0;
        if (!isPlayPause && stepDirection === 0) return;
        const target = event.target;
        const isTextInput =
            (target instanceof HTMLInputElement && target.type !== 'range') ||
            target instanceof HTMLTextAreaElement ||
            (target instanceof HTMLElement && target.isContentEditable);
        if (isTextInput) return;
        const isRangeSlider = target instanceof HTMLInputElement && target.type === 'range';
        if (isRangeSlider && !isPlayPause) return;
        if (this.canvas.classList.contains('hidden')) return;
        // Also short-circuit when an ancestor is display:none (e.g. the host toggled its own
        // container off while routing to a full-page overlay like a samples list). Without
        // this check the canvas can be off-screen yet its .hidden class is untouched, so the
        // shortcuts would still drive the invisible player behind the overlay. offsetParent
        // is null for elements whose computed display is none anywhere on the ancestor chain
        // (except the body, which the player is not).
        if (this.canvas.offsetParent === null) return;
        if (!this.playbackBar.isVisible()) return;
        event.preventDefault();
        if (isPlayPause) {
            this.playbackBar.togglePlayback();
        } else {
            this.playbackBar.stepFrame(stepDirection);
        }
    }
}

// Re-export public types for host consumers who imported PAGXPlayer from the package root.
export type {
    BackgroundColor,
    EditorCallbacks,
    FrameChangeEventDetail,
    LoadedEventDetail,
    LoadErrorEventDetail,
    PAGXPlayerLoadOptions,
    PAGXPlayerOptions,
    StatusOptions,
    ToolbarSlot,
};
