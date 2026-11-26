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

import type { HitTestResult, NodeBounds, NodeSourceEntry, PlayerModule, PlayerView } from './pagx-view-types';
import type {
    BackgroundColor,
    EditorCallbacks,
    FrameChangeEventDetail,
    LoadedEventDetail,
    LoadErrorEventDetail,
    PAGXPlayerEventMap,
    PAGXPlayerLoadOptions,
    PAGXPlayerOptions,
    SourceDiagnostic,
    SourceDiagnosticProvider,
    StatusOptions,
    ToolbarSlot,
} from './types';
import { GestureManager, bindCanvasEvents } from './gesture-manager';
import { PlaybackBar } from './playback-bar';
import { buildToolbar, setToolbarVisible } from './toolbar';
import { EditorPanel, EDITOR_STATUS_DURATION_MS, type LineRange } from './editor/index';
import { applyDraftLineChange, classifyEdits, findNodeIndexForLine, hasMatchingSpanBoundaries,
    SourceMapIndex, type DraftLineChange } from './incremental-apply';
import { ensureStylesInjected } from './styles';
import { StatusPill } from './status-pill';

/** Canvas element id assigned by the player. Kept stable so external CSS or debug tooling
 *  that targets '#pagx-canvas' keeps working on the (dominant) single-instance case. The
 *  player itself no longer depends on this id for view initialization - initView() hands the
 *  canvas element directly to PAGXView.init - so multiple PAGXPlayer instances on the same
 *  document remain functionally isolated even though their canvases share this id. */
const CANVAS_ID = 'pagx-canvas';

/** Default background: fully transparent so the checkered canvas backdrop shows through for
 *  pagx documents with alpha. */
const DEFAULT_BACKGROUND: BackgroundColor = { r: 0, g: 0, b: 0, a: 0 };

/** Owns the document-side half of PAGXPlayer.load(): parse, host-supplied fonts and resources,
 *  background / loop / playback-state restoration and the first draw. It is deliberately stateless
 *  across loads — the generation guard stays in the caller, which owns load ordering and UI state —
 *  so the phases can be tested or reordered without touching the player's DOM and event wiring. */
class DocumentLoader {
    /** Runs the full document pipeline against an already-initialized view. `isCurrent` is polled
     *  after each await so a superseded load aborts before touching the view again. Returns false
     *  when aborted, true when the document is built and the first frame drawn. */
    static async load(view: PlayerView, pagxBytes: Uint8Array, options: PAGXPlayerLoadOptions,
                      backgroundColor: BackgroundColor, isCurrent: () => boolean,
                      resetTransform: () => void): Promise<boolean> {
        const preservedTime = options.preserveCurrentTime
            ? (view.durationMicros() > 0 ? view.currentTimeMicros() : 0)
            : 0;
        const wasPlaying = options.preserveCurrentTime && view.isPlaying();

        view.parsePAGX(pagxBytes);

        // The view resets font registration on each load, so host fonts must be re-registered
        // before buildLayers().
        if (options.registerFonts) {
            await options.registerFonts(view);
            if (!isCurrent()) return false;
        }

        // parsePAGX populates the resource path list; fetch concurrently but push in path order so
        // duplicate paths stay deterministic.
        if (options.resolveResource) {
            const paths: string[] = view.getExternalFilePaths();
            if (paths && paths.length > 0) {
                const resolve = options.resolveResource;
                const items = await Promise.all(
                    paths.map(async (rel: string) => ({
                        rel,
                        buf: await resolve(rel),
                    })),
                );
                if (!isCurrent()) return false;
                for (const item of items) {
                    if (item.buf) {
                        view.loadFileData(item.rel, item.buf);
                    }
                }
            }
        }

        view.buildLayers();

        if (options.forceLoop !== false) {
            view.setLoop(true);
        }
        const bg = backgroundColor;
        const hex =
            '#' +
            bg.r.toString(16).padStart(2, '0') +
            bg.g.toString(16).padStart(2, '0') +
            bg.b.toString(16).padStart(2, '0');
        view.setBackgroundColor(hex, bg.a);

        if (options.preserveCurrentTime && preservedTime > 0) {
            view.setCurrentTimeMicros(Math.min(preservedTime, view.durationMicros()));
        }
        // buildLayers leaves the view auto-playing; mirror the previous play state when asked, so a
        // paused reload does not silently resume. A non-preserving load shows a different document,
        // so reset the gesture transform unless the host asked to keep the viewport.
        if (options.preserveCurrentTime) {
            if (wasPlaying) {
                view.play();
            } else {
                view.pause();
            }
        } else {
            resetTransform();
        }

        // Force a synchronous first frame so the old document cannot ghost through when the canvas
        // is unhidden in the caller.
        view.draw();
        return true;
    }
}

/** Owns the selection-side half of PAGXPlayer.load(): rebuild the source map from the new document
 *  and clear every handle that pointed into the previous runtime tree. State stays on the player so
 *  the hover/select/overlay methods can keep using it; this class only sequences the reset. */
class SelectionController {
    static reset(view: PlayerView, editor: EditorPanel | null,
                 setSourceMap: (map: readonly NodeSourceEntry[]) => void,
                 clearSelection: () => void, refreshOverlay: () => void): void {
        setSourceMap(view.getNodeSourceMap());
        clearSelection();
        editor?.clearHighlight();
        refreshOverlay();
    }
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
    private readonly statusPill: StatusPill;

    private module: PlayerModule | null = null;
    private view: PlayerView | null = null;
    private detachCanvasEvents: (() => void) | null = null;

    // --- Selection mode (phase 1: canvas<->editor highlighting) ---
    private selectMode = false;
    private hoverIndex = -1;
    // Full hitTest result from the last canvas hover/select. Carries the reference node's span
    // (so the editor highlights the <Layer composition="@X"> reference, not the internal
    // definition) and the clicked instance's on-screen bounds (so the overlay outlines exactly
    // what the user clicked, even when the same source node has multiple instances). Cleared by
    // editor-direction selection (onEditorDblClick) sets selectedIndex via
    // sourceMap line lookup and have no hitTest result to mirror.
    private hoverHit: HitTestResult | null = null;
    private selectHit: HitTestResult | null = null;
    // Node index the editor is hovering (editor->canvas direction). Independent of selectMode:
    // hovering an editor line always highlights the corresponding node on the canvas.
    private editorHoverIndex = -1;
    // The physical tag line highlighted in the editor. It is intentionally separate from the node's
    // full source span so parents never swallow their children during source browsing.
    private editorHoverLine = -1;
    private selectedIndex = -1;
    // Runtime spans are refreshed only by load(). Draft spans are a line-number projection used
    // while unsaved source edits insert or remove lines, then reset from runtime spans on Apply/Discard.
    private sourceMap: NodeSourceEntry[] = [];
    private draftSourceMap: NodeSourceEntry[] = [];
    // Lazy index over draftSourceMap. applyDraftLineChange mutates entries in place, so any line
    // change invalidates the sorted spans and the index must be rebuilt; hover/select/overlay lookups
    // reuse it between edits instead of scanning the map linearly each time.
    private draftIndex: SourceMapIndex | null = null;
    // Selection boxes painted over the canvas, one per runtime layer instance built from the
    // bounds-bearing node — a source node referenced by N composition layers builds N instances,
    // all of which get outlined. Kept as a reusable pool so the per-frame follow-loop only
    // toggles styles instead of churning DOM nodes.
    private overlayBoxes: HTMLDivElement[] = [];
    // The bounds-bearing node the overlay currently paints (resolved from the hover/select target
    // by climbing to the owning Layer for internal elements), and which visual state to render.
    // Cached by refreshOverlay so the per-frame follow-loop skips the ancestry walk.
    private overlayBoundsIndex = -1;
    private overlayKind: 'hover' | 'select' = 'hover';
    // Bounds queried on the previous overlay tick, one entry per instance. PAGXView.draw()
    // double-buffers recordings, so during playback the canvas presents the frame recorded one
    // tick earlier than the scene state getNodeBounds reads; presenting the previous tick's
    // bounds keeps the outline on the pixels.
    private overlayPreviousBounds: NodeBounds[] | null = null;
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
        this.statusPill = new StatusPill(this.root);

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
                diagnosticProviders: this.createDiagnosticProviders(options.diagnosticProviders ?? []),
                incrementalApply: (oldXml, newXml) => this.tryIncrementalApply(oldXml, newXml),
            });
        }
        // Wire editor->canvas interactions. Hover highlights the node on the canvas + mirrors the
        // grey highlight on the editor line; double-click unlocks the enclosing node's span for
        // editing. Registered once here; EditorPanel re-applies both across editor rebuilds.
        if (this.editor) {
            this.editor.onHoverLine((line) => this.onEditorHover(line));
            this.editor.onDblClickLine((line) => this.onEditorDblClick(line));
            this.editor.onDraftLineChanges((changes) => this.onDraftLineChanges(changes));
            this.editor.setTagSpanResolver((line) => this.resolveTagSpan(line));
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

    /** Creates the built-in importer provider followed by host-provided providers. The importer
     *  validates only well-formed XML and never replaces the view's current document; the editor
     *  handles XML syntax separately before invoking this provider. */
    private createDiagnosticProviders(
        providers: readonly SourceDiagnosticProvider[],
    ): SourceDiagnosticProvider[] {
        return [
            {
                id: 'pagx-importer',
                validate: this.validateWithImporter.bind(this),
            },
            ...providers,
        ];
    }

    /** Converts the WASM importer's line-based errors into generic source diagnostics. Importer
     *  errors currently identify an entire element line rather than an attribute span, therefore
     *  endColumn uses a large value that SourceEditor clamps to the end of that line. */
    private validateWithImporter(xmlText: string): SourceDiagnostic[] {
        if (this.view === null) {
            return [];
        }
        return this.view.validatePAGX(new TextEncoder().encode(xmlText)).map((diagnostic) => ({
            owner: 'pagx-importer',
            severity: 'error',
            message: diagnostic.message,
            startLine: diagnostic.line,
            startColumn: 1,
            endLine: diagnostic.line,
            endColumn: Number.MAX_SAFE_INTEGER,
        }));
    }

    /** Load a pagx from raw bytes. Orchestrates DocumentLoader (parse + fonts + external resources
     *  + playback restore + first draw), the editor sync, SelectionController (source map + cleared
     *  handles) and the DOM/bar/event wiring; failure recovery stays here because it owns the UI.
     *  Emits 'loaded' on success and 'loadError' on failure.
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
        // Snapshots taken while it's still safe to read pre-existing view state; used after
        // the reset() inside parsePAGX() to restore playback position and play/pause state.
        try {
            await this.ensureView();
            if (!this.isCurrentLoad(generation)) return;
            const view = this.view!;

            const loaded = await DocumentLoader.load(
                view, pagxBytes, options, this.options.backgroundColor ?? DEFAULT_BACKGROUND,
                () => this.isCurrentLoad(generation), () => this.gesture.resetTransform(),
            );
            if (!loaded || !this.isCurrentLoad(generation)) return;

            this.canvas.classList.remove('hidden');
            setToolbarVisible(this.toolbarRoot, true);
            // A document without a queryable duration still gets the bar in its greyed-out
            // fallback mode, as long as it has a default timeline at all.
            this.updatePlaybackBarMode();

            // Feed the editor with the freshly loaded XML. If the host pre-decoded it, we use
            // that; otherwise we decode the bytes so the editor always has something to show
            // when the user hits L. Encoding is assumed UTF-8, which is what pagx serializes to.
            let xmlText: string | undefined = options.xmlText;
            if (this.editor && !xmlText) {
                xmlText = new TextDecoder('utf-8').decode(pagxBytes);
            }
            this.editor?.setDocumentXml(xmlText ?? null);

            // A new document renumbers nodes, so rebuild the source map and drop every handle that
            // pointed into the previous runtime tree.
            SelectionController.reset(view, this.editor,
                (map) => {
                    this.sourceMap = map;
                    this.draftSourceMap = this.cloneSourceMap(map);
                    this.draftIndex = null;
                },
                () => this.clearSelectionState(),
                () => this.refreshOverlay());

            const detail: LoadedEventDetail = {
                duration: view.durationMicros(),
                frameRate: view.frameRate(),
                hasAnimation: view.hasTimeline(),
                xmlText,
            };
            this.dispatchEvent(new CustomEvent('loaded', { detail }));
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
        this.updatePlaybackBarMode();
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
        this.hoverHit = null;
        this.syncEditorHighlight('hover');
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
        const hit = this.view.hitTest(surfaceX, surfaceY);
        const idx = hit !== null ? hit.index : -1;
        if (idx !== this.hoverIndex) {
            this.hoverIndex = idx;
            this.hoverHit = hit;
            this.refreshOverlay();
            this.syncEditorHighlight('hover', 'center');
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
        const hit = this.view.hitTest(surfaceX, surfaceY);
        // Missed the geometry (clicked empty canvas): keep the inspector armed so the user can
        // try again rather than silently dropping out of inspect mode.
        if (hit === null) {
            return;
        }
        this.selectedIndex = hit.index;
        this.selectHit = hit;
        // DevTools-style: picking a layer deactivates the inspector. exitSelectMode() clears the
        // transient hover and repaints the overlay from the now-set selection, so only the sticky
        // blue outline remains; it stays highlighted on the canvas and mirrored in the editor
        // until the user's next action.
        this.exitSelectMode();
        this.syncEditorHighlight('select', 'center');
    }

    /** Editor hover resolves only a physical XML tag line. The full node index remains available for
     *  canvas overlay bounds, but the grey source decoration never expands to the node's children. */
    private onEditorHover(line: number): void {
        const tagLine = this.editor?.getDraftTagLine(line) ?? null;
        const idx = tagLine === null ? -1 : findNodeIndexForLine(this.draftSourceMap, tagLine);
        const nextLine = tagLine ?? -1;
        if (idx === this.editorHoverIndex && nextLine === this.editorHoverLine) {
            return;
        }
        this.editorHoverIndex = idx;
        this.editorHoverLine = nextLine;
        this.refreshOverlay();
        this.syncEditorHighlight('hover');
    }

    /** Double-clicking any physical line unlocks the whole document for editing; the amber edit
     *  decoration marks the double-clicked line itself. There is no "is this a valid tag" gate:
     *  whether one can edit depends only on which line was clicked, never on that line's content
     *  (a line whose tag markup was deleted mid-edit must still be re-enterable). The source map
     *  lookup is a pure line-number query (not a content check) and only drives the canvas selection
     *  sync; when the line falls outside every node's span (e.g. the XML declaration), it simply
     *  returns -1 and no canvas node gets selected. */
    private onEditorDblClick(line: number): void {
        const idx = findNodeIndexForLine(this.draftSourceMap, line);
        this.selectedIndex = idx;
        // Editor-direction selection has no hitTest result, so updateOverlay falls back to the
        // source-map node bounds when one exists. Lines with no runtime node clear the canvas box.
        this.selectHit = null;
        this.editor?.clearHover();
        this.editor?.enterEditMode(line);
        this.refreshOverlay();
        this.syncEditorHighlight('select');
    }

    private cloneSourceMap(entries: readonly NodeSourceEntry[]): NodeSourceEntry[] {
        return entries.map((entry) => ({ ...entry, channels: [...entry.channels] }));
    }

    /** Drops every hover/selection handle that pointed into the previous runtime tree. */
    private clearSelectionState(): void {
        this.hoverIndex = -1;
        this.hoverHit = null;
        this.selectHit = null;
        this.editorHoverIndex = -1;
        this.editorHoverLine = -1;
        this.selectedIndex = -1;
    }

    /** Looks up a draft source entry by its native Node index. getNodeSourceMap skips mounted
     *  composition wrappers, so the sourceMap array offset is not the node index and a positional
     *  read would silently mis-target. Returns null when the index has no live entry. */
    private draftEntryByIndex(index: number): NodeSourceEntry | null {
        if (index < 0) {
            return null;
        }
        return this.getDraftIndex().entryByIndex(index);
    }

    /** Returns the current draft index, rebuilding it when a line change has invalidated it. */
    private getDraftIndex(): SourceMapIndex {
        if (this.draftIndex === null) {
            this.draftIndex = new SourceMapIndex(this.draftSourceMap);
        }
        return this.draftIndex;
    }

    /** Applies accepted draft line deltas to a projection of the runtime source map. Entries wholly
     *  deleted or partially cut by a multi-line replacement become unavailable rather than pointing
     *  at an unrelated surviving tag. Apply/Discard/reset restores a fresh projection. */
    private onDraftLineChanges(
        changes: readonly DraftLineChange[] | null,
    ): void {
        if (changes === null) {
            this.draftSourceMap = this.cloneSourceMap(this.sourceMap);
            this.draftIndex = null;
            return;
        }
        for (const change of changes) {
            applyDraftLineChange(this.draftSourceMap, change);
        }
        // applyDraftLineChange mutates entry spans in place, invalidating the sorted index.
        this.draftIndex = null;
        // The pointer may now refer to a shifted source line; wait for the next mouse move instead
        // of leaving a stale editor-originated hover decoration behind.
        this.editorHoverLine = -1;
        this.editorHoverIndex = -1;
    }

    /** Resolves the line span of the innermost tag enclosing the given 1-based source line, for
     *  the editor's tag-block copy/delete context menu actions. The last valid source map supplies
     *  the structural span; only that node's own boundary tags are re-validated against the draft.
     *  A malformed child therefore does not prevent deleting its intact parent, while a damaged
     *  parent boundary still falls back safely to the current line. */
    private resolveTagSpan(line: number): LineRange | null {
        const index = findNodeIndexForLine(this.draftSourceMap, line);
        if (index < 0 || this.editor === null) {
            return null;
        }
        const entry = this.draftEntryByIndex(index);
        if (entry === null) {
            return null;
        }
        const start = entry.startLine;
        if (start <= 0) {
            return null;
        }
        const end = entry.endLine > 0 ? entry.endLine : start;
        if (end > this.editor.getLineCount()) {
            return null;
        }
        // Only the mapped span is read back. Splitting the whole document here would copy the
        // entire text (hundreds of MB for a large PAGX with inline images) to check one tag block.
        if (!hasMatchingSpanBoundaries(this.editor.getLines(start, end))) {
            return null;
        }
        return { startLine: start, endLine: end };
    }

    /** Attempts to apply the edit from oldXml to newXml incrementally, in place, via setNodeChannel
     *  instead of a full reparse+rebuild. Returns true only when the whole edit is a set of pure
     *  attribute-value changes on incrementable channels; false otherwise, signalling the editor to
     *  fall back to the full onApply pipeline. Failing part-way leaves channel writes applied until
     *  that fallback replaces the runtime; the full apply is authoritative and load()'s failure
     *  path clears the UI if it also fails. */
    private tryIncrementalApply(oldXml: string, newXml: string): boolean {
        if (!this.view || this.draftSourceMap.length === 0) {
            return false;
        }
        const edits = classifyEdits(oldXml, newXml, this.draftSourceMap);
        if (edits === null) {
            return false;
        }
        if (edits.length === 0) {
            return true;
        }
        for (const edit of edits) {
            const ok = this.view.setNodeChannel(edit.index, edit.channel, edit.value);
            if (!ok) {
                // Let Apply fall through to the full onApply pipeline. Do not re-parse oldXml here:
                // parsePAGX creates a fresh document and would drop the external resources that
                // load() replays, plus the playback position, loop and background colour. The full
                // apply is authoritative and restores all of that; on failure load() clears the UI.
                return false;
            }
        }
        this.view.draw();
        return true;
    }

    /** The transient hover target: editor-hover wins over canvas-hover (mouse can only be in one). */
    private hoverTarget(): number {
        return this.editorHoverIndex >= 0 ? this.editorHoverIndex : this.hoverIndex;
    }

    /** Mirrors the hover or selection target onto the editor as a line highlight. Hover spans come
     *  from the hitTest result (the <Layer composition="@X"> reference, not its internal definition)
     *  unless the hover originated in the editor; selection uses the sticky selectedIndex. */
    private syncEditorHighlight(target: 'hover' | 'select',
                                align: 'none' | 'nearest' | 'start' | 'center' = 'none'): void {
        const hover = target === 'hover';
        if (hover && this.editorHoverLine > 0) {
            this.editor?.highlightHover(this.editorHoverLine, this.editorHoverLine);
            if (align !== 'none') {
                this.editor?.scrollToLine(this.editorHoverLine, align);
            }
            return;
        }
        const hit = hover ? this.hoverHit : this.selectHit;
        if (hit !== null) {
            const projected = this.draftEntryByIndex(hit.index);
            const start = projected !== null && projected.startLine > 0
                ? projected.startLine
                : hit.startLine;
            if (hover) {
                this.editor?.highlightHover(start, start);
            } else {
                this.editor?.highlightSelect(start, start);
            }
            if (align !== 'none') {
                this.editor?.scrollToLine(start, align);
            }
            return;
        }
        const idx = hover ? this.hoverTarget() : this.selectedIndex;
        const entry = this.draftEntryByIndex(idx);
        if (entry === null || entry.startLine <= 0) {
            if (hover) {
                this.editor?.clearHover();
            } else {
                this.editor?.clearSelect();
            }
            return;
        }
        if (hover) {
            this.editor?.highlightHover(entry.startLine, entry.startLine);
        } else {
            this.editor?.highlightSelect(entry.startLine, entry.startLine);
        }
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

    /** Grows or shrinks the pool of selection boxes so updateOverlay can paint one per runtime
     *  instance; surplus boxes are hidden but kept for reuse. */
    private setOverlayBoxCount(count: number): void {
        while (this.overlayBoxes.length < count) {
            const overlay = document.createElement('div');
            overlay.className = 'pagx-select-overlay';
            overlay.style.pointerEvents = 'none';
            this.root.appendChild(overlay);
            this.overlayBoxes.push(overlay);
        }
        for (let i = count; i < this.overlayBoxes.length; i++) {
            this.overlayBoxes[i].style.display = 'none';
        }
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
        const child = this.draftEntryByIndex(index);
        if (child === null || child.startLine <= 0) {
            return -1;
        }
        const childStart = child.startLine;
        const childEnd = child.endLine > 0 ? child.endLine : childStart;
        let bestIndex = -1;
        let bestSpan = Number.POSITIVE_INFINITY;
        for (const entry of this.draftSourceMap) {
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
        // The previous-tick bounds belong to the old target; carrying them over would flash the
        // old outline for one frame when the target switches mid-playback.
        this.overlayPreviousBounds = null;
        if (this.overlayBoundsIndex >= 0) {
            this.startOverlayLoop();
        } else {
            this.stopOverlayLoop();
        }
        this.updateOverlay();
    }

    private updateOverlay(): void {
        if (this.overlayBoundsIndex < 0 || !this.view) {
            this.setOverlayBoxCount(0);
            return;
        }
        // Every frame queries getNodeBounds so the overlay tracks zoom and animation in real time.
        // hitTest deliberately omits bounds: a point-in-time snapshot would not follow zoom or
        // animation and caused the overlay to freeze and disappear. getNodeBounds returns one rect
        // per runtime instance, so a source node referenced by several composition layers gets
        // every instance outlined.
        const bounds = this.view.getNodeBounds(this.overlayBoundsIndex);
        if (bounds === null || bounds.length === 0) {
            this.setOverlayBoxCount(0);
            this.overlayPreviousBounds = null;
            return;
        }
        // PAGXView.draw() double-buffers recordings: while the timeline is playing, the canvas
        // presents the frame recorded one rAF earlier than the scene state getNodeBounds reads,
        // so applying the current bounds would lead the pixels by one frame. Present the previous
        // tick's bounds instead. Paused interactions (scrub/zoom/edit) set presentImmediately in
        // draw(), which presents the current frame, so the current bounds are correct there.
        const shown = this.view.isPlaying() && this.overlayPreviousBounds !== null
            ? this.overlayPreviousBounds
            : bounds;
        this.overlayPreviousBounds = bounds;
        const rect = this.canvas.getBoundingClientRect();
        // Surface (backing) -> CSS pixels.
        const scaleX = rect.width / this.canvas.width;
        const scaleY = rect.height / this.canvas.height;
        this.setOverlayBoxCount(shown.length);
        for (let i = 0; i < shown.length; i++) {
            const overlay = this.overlayBoxes[i];
            overlay.style.display = 'block';
            overlay.style.left = shown[i].x * scaleX + 'px';
            overlay.style.top = shown[i].y * scaleY + 'px';
            overlay.style.width = shown[i].w * scaleX + 'px';
            overlay.style.height = shown[i].h * scaleY + 'px';
            overlay.classList.toggle('is-selected', this.overlayKind === 'select');
            overlay.classList.toggle('is-hover', this.overlayKind === 'hover');
        }
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

    /** Single source of truth for the playback bar's mode after a load: a document with a
     *  queryable duration gets the normal bar, other timelines keep the greyed untimed bar, and
     *  static documents hide the bar. */
    private updatePlaybackBarMode(): void {
        const view = this.view;
        if (!view) {
            return;
        }
        if (view.durationMicros() > 0) {
            this.playbackBar.setVisible(true);
        } else if (view.hasTimeline()) {
            this.playbackBar.showUntimed();
        } else {
            this.playbackBar.setVisible(false);
        }
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
        return this.statusPill.show(text, options);
    }

    /** Hide the status pill. If `token` is provided, only clears when it still identifies the
     *  currently displayed message; otherwise clears unconditionally. */
    public hideStatus(token?: number): void {
        this.statusPill.hide(token);
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
        for (const overlay of this.overlayBoxes) {
            overlay.remove();
        }
        this.overlayBoxes = [];
        this.resizeObserver?.disconnect();
        this.resizeObserver = null;
        document.removeEventListener('visibilitychange', this.onVisibilityChange);
        window.removeEventListener('resize', this.onWindowResize);
        document.removeEventListener('keydown', this.onKeyDown);
        window.removeEventListener('beforeunload', this.onBeforeUnload);
        window.removeEventListener('pagehide', this.onPageHide);
        window.removeEventListener('pageshow', this.onPageShow);
        this.statusPill.destroy();
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
        if (!this.playbackBar.isVisible()) {
            return;
        }
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
    SourceDiagnostic,
    SourceDiagnosticProvider,
    StatusOptions,
    ToolbarSlot,
};
