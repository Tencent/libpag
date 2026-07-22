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

import type { PlayerModule, PlayerView } from './pagx-view-types';
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

/** Canvas element id assigned by the player. Kept stable so external CSS or debug tooling that
 *  targets '#pagx-canvas' keeps working. Assumes a single player instance per page. */
const CANVAS_ID = 'pagx-canvas';

/** Default background: fully transparent so the checkered canvas backdrop shows through for
 *  pagx documents with alpha. */
const DEFAULT_BACKGROUND: BackgroundColor = { r: 0, g: 0, b: 0, a: 0 };

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
            });
        }

        // Gesture manager (view accessor closure keeps working across reloads)
        this.gesture = new GestureManager(() => this.view);
        this.detachCanvasEvents = bindCanvasEvents(this.canvas, this.gesture, () => {
            this.playbackBar.togglePlayback();
        });

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
        // Snapshots taken while it's still safe to read pre-existing view state; used after
        // the reset() inside parsePAGX() to restore playback position and play/pause state.
        try {
            await this.ensureView();
            if (!this.isCurrentLoad(generation)) return;
            const view = this.view!;

            const preservedTime = options.preserveCurrentTime
                ? (view.durationMicros() > 0 ? view.currentTimeMicros() : 0)
                : 0;
            const wasPlaying = options.preserveCurrentTime && view.isPlaying();

            view.parsePAGX(pagxBytes);

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
                }
            }

            view.buildLayers();

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
            view.draw();

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

            const detail: LoadedEventDetail = {
                duration: view.durationMicros(),
                frameRate: view.frameRate(),
                hasAnimation,
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
        const view = this.module.PAGXView.init('#' + CANVAS_ID);
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
     *  the wasm view. Called on window resize, ResizeObserver ticks, and after ensureView(). */
    private updateSize(): void {
        if (!this.view) {
            return;
        }
        const rect = this.sizeContainer.getBoundingClientRect();
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
