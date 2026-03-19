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

import PAGXWasm from '../wasm/pagx-playground';
import { TGFXBind } from '@tgfx/binding';
import { getCached, putCache, clearStaleCache } from './cache-manager';

interface PAGXModule {
    GL: any;
    PAGXView: {
        MakeFrom: (canvasID: string) => PAGXView | null;
    };
    [key: string]: any;
}

interface PAGXView {
    registerFonts: (fontData: Uint8Array, emojiFontData: Uint8Array) => void;
    loadPAGX: (pagxData: Uint8Array) => void;
    parsePAGX: (pagxData: Uint8Array) => void;
    getExternalFilePaths: () => StringVector;
    loadFileData: (filePath: string, data: Uint8Array) => boolean;
    buildLayers: () => void;
    updateSize: () => void;
    updateZoomScaleAndOffset: (zoom: number, offsetX: number, offsetY: number) => void;
    draw: () => void;
    contentWidth: () => number;
    contentHeight: () => number;
    delete?: () => void;
}

interface StringVector {
    size: () => number;
    get: (index: number) => string;
    delete: () => void;
}

const MIN_ZOOM = 0.001;
const MAX_ZOOM = 1000.0;

// Resource URLs
const FONT_URL = chrome.runtime.getURL('fonts/NotoSansSC-Regular.otf');
const EMOJI_FONT_URL = chrome.runtime.getURL('fonts/NotoColorEmoji.ttf');
const WASM_URL = chrome.runtime.getURL('wasm/pagx-playground.wasm');

const ESTIMATED_WASM_SIZE = 2400000;
const ESTIMATED_FONT_SIZE = 8800000;
const ESTIMATED_EMOJI_FONT_SIZE = 10300000;

class PlaygroundState {
    module: PAGXModule | null = null;
    pagxView: PAGXView | null = null;
    animationFrameId: number | null = null;
    isPageVisible: boolean = true;
    resized: boolean = false;
    isLoading: boolean = false;
    zoom: number = 1.0;
    offsetX: number = 0;
    offsetY: number = 0;
    fontData: Uint8Array | null = null;
    emojiFontData: Uint8Array | null = null;
    pagxSourceUrl: string = '';
    pagxText: string = '';
    codeViewVisible: boolean = false;
}

class LoadingProgress {
    wasmLoaded: number = 0;
    wasmTotal: number = ESTIMATED_WASM_SIZE;
    wasmDone: boolean = false;
    fontLoaded: number = 0;
    fontTotal: number = ESTIMATED_FONT_SIZE;
    fontDone: boolean = false;
    emojiFontLoaded: number = 0;
    emojiFontTotal: number = ESTIMATED_EMOJI_FONT_SIZE;
    emojiFontDone: boolean = false;

    isAllResourcesCached(): boolean {
        return this.wasmDone && this.fontDone && this.emojiFontDone;
    }

    getOverallProgress(): number {
        let totalSize = 0;
        let loadedSize = 0;
        if (!this.wasmDone) {
            totalSize += this.wasmTotal;
            loadedSize += this.wasmLoaded;
        }
        if (!this.fontDone) {
            totalSize += this.fontTotal;
            loadedSize += this.fontLoaded;
        }
        if (!this.emojiFontDone) {
            totalSize += this.emojiFontTotal;
            loadedSize += this.emojiFontLoaded;
        }
        if (totalSize === 0) {
            return 99;
        }
        return Math.min(99, Math.round((loadedSize / totalSize) * 100));
    }
}

enum ErrorType {
    WASM = 'wasm',
    FONT = 'font',
    FORMAT = 'format',
    NETWORK = 'network',
}

class PlaygroundError extends Error {
    type: ErrorType;
    constructor(type: ErrorType, message?: string) {
        super(message);
        this.type = type;
    }
}

// Gesture Manager (simplified from playground)
class GestureManager implements GestureManagerInterface {
    private scaleY = 1.0;
    private pinchTimeout = 150;
    private timer: number | undefined;
    private scaleStartZoom = 1.0;
    private lastEventTime = 0;
    private lastDeltaY = 0;
    private timeThreshold = 50;
    private deltaYThreshold = 50;
    private deltaYChangeThreshold = 10;
    private mouseWheelRatio = 800;
    private touchWheelRatio = 100;

    private isDragging = false;
    private dragStartX = 0;
    private dragStartY = 0;
    private dragStartOffsetX = 0;
    private dragStartOffsetY = 0;

    private startTouchDistance = 0;
    private lastTouchCenterX = 0;
    private lastTouchCenterY = 0;
    private isTouchPanning = false;
    private isTouchZooming = false;

    public zoom = 1.0;
    public offsetX = 0;
    public offsetY = 0;

    private handleScrollEvent(
        event: WheelEvent,
        state: number,
        playgroundState: PlaygroundState,
    ) {
        if (state === 1) {
            this.scaleStartZoom = this.zoom;
            this.scaleY = 1.0;
            if (event.shiftKey && event.deltaX === 0 && event.deltaY !== 0) {
                this.offsetX -= event.deltaY * window.devicePixelRatio;
            } else {
                this.offsetX -= event.deltaX * window.devicePixelRatio;
                this.offsetY -= event.deltaY * window.devicePixelRatio;
            }
            playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
        }
    }

    private handleScaleEvent(
        event: WheelEvent,
        state: number,
        canvas: HTMLElement,
        playgroundState: PlaygroundState,
    ) {
        if (state === 0) {
            this.scaleY = 1.0;
            this.scaleStartZoom = this.zoom;
        }
        if (state === 1) {
            const rect = canvas.getBoundingClientRect();
            const pixelX = (event.clientX - rect.left) * window.devicePixelRatio;
            const pixelY = (event.clientY - rect.top) * window.devicePixelRatio;
            const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * this.scaleY));
            this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
            this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
            this.zoom = newZoom;
        }
        if (state === 2) {
            this.scaleY = 1.0;
            this.scaleStartZoom = this.zoom;
        }
        playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    public clearState() {
        this.scaleY = 1.0;
        this.timer = undefined;
    }

    public resetTransform(playgroundState: PlaygroundState) {
        this.zoom = 1.0;
        this.offsetX = 0;
        this.offsetY = 0;
        this.clearState();
        playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    private resetScrollTimeout(
        event: WheelEvent,
        playgroundState: PlaygroundState,
    ) {
        clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            this.handleScrollEvent(event, 2, playgroundState);
            this.clearState();
        }, this.pinchTimeout);
    }

    private resetScaleTimeout(
        event: WheelEvent,
        canvas: HTMLElement,
        playgroundState: PlaygroundState,
    ) {
        clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            this.handleScaleEvent(event, 2, canvas, playgroundState);
            this.clearState();
        }, this.pinchTimeout);
    }

    private getDeviceType(event: WheelEvent): number {
        const now = Date.now();
        const timeDifference = now - this.lastEventTime;
        const deltaYChange = Math.abs(event.deltaY - this.lastDeltaY);
        let isTouchpad = false;
        if (event.deltaMode === event.DOM_DELTA_PIXEL && timeDifference < this.timeThreshold) {
            if (Math.abs(event.deltaY) < this.deltaYThreshold && deltaYChange < this.deltaYChangeThreshold) {
                isTouchpad = true;
            }
        }
        this.lastEventTime = now;
        this.lastDeltaY = event.deltaY;
        return isTouchpad ? 0 : 1;
    }

    public onWheel(event: WheelEvent, canvas: HTMLElement, playgroundState: PlaygroundState) {
        const deviceType = this.getDeviceType(event);
        const wheelRatio = (deviceType === 1 ? this.mouseWheelRatio : this.touchWheelRatio);
        if (!event.deltaY || (!event.ctrlKey && !event.metaKey)) {
            this.resetScrollTimeout(event, playgroundState);
            this.handleScrollEvent(event, 1, playgroundState);
        } else {
            this.scaleY *= Math.exp(-(event.deltaY) / wheelRatio);
            if (!this.timer) {
                this.resetScaleTimeout(event, canvas, playgroundState);
                this.handleScaleEvent(event, 0, canvas, playgroundState);
            } else {
                this.resetScaleTimeout(event, canvas, playgroundState);
                this.handleScaleEvent(event, 1, canvas, playgroundState);
            }
        }
    }

    public onMouseDown(event: MouseEvent, canvas: HTMLElement) {
        if (event.button !== 0) {
            return;
        }
        this.isDragging = true;
        this.dragStartX = event.clientX;
        this.dragStartY = event.clientY;
        this.dragStartOffsetX = this.offsetX;
        this.dragStartOffsetY = this.offsetY;
        canvas.style.cursor = 'grabbing';
    }

    public onMouseMove(event: MouseEvent, playgroundState: PlaygroundState) {
        if (!this.isDragging) {
            return;
        }
        const deltaX = (event.clientX - this.dragStartX) * window.devicePixelRatio;
        const deltaY = (event.clientY - this.dragStartY) * window.devicePixelRatio;
        this.offsetX = this.dragStartOffsetX + deltaX;
        this.offsetY = this.dragStartOffsetY + deltaY;
        playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    public onMouseUp(canvas: HTMLElement) {
        this.isDragging = false;
        canvas.style.cursor = 'grab';
    }

    private getTouchDistance(touches: TouchList): number {
        if (touches.length < 2) {
            return 0;
        }
        const dx = touches[0].clientX - touches[1].clientX;
        const dy = touches[0].clientY - touches[1].clientY;
        return Math.sqrt(dx * dx + dy * dy);
    }

    private getTouchCenter(touches: TouchList): { x: number; y: number } {
        if (touches.length < 2) {
            return { x: touches[0].clientX, y: touches[0].clientY };
        }
        return {
            x: (touches[0].clientX + touches[1].clientX) / 2,
            y: (touches[0].clientY + touches[1].clientY) / 2,
        };
    }

    public onTouchStart(event: TouchEvent, canvas: HTMLElement) {
        if (event.touches.length === 1) {
            this.isTouchPanning = true;
            this.isTouchZooming = false;
            this.dragStartX = event.touches[0].clientX;
            this.dragStartY = event.touches[0].clientY;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        } else if (event.touches.length === 2) {
            this.isTouchPanning = false;
            this.isTouchZooming = true;
            this.startTouchDistance = this.getTouchDistance(event.touches);
            const center = this.getTouchCenter(event.touches);
            this.lastTouchCenterX = center.x;
            this.lastTouchCenterY = center.y;
            this.scaleStartZoom = this.zoom;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        }
    }

    public onTouchMove(event: TouchEvent, canvas: HTMLElement, playgroundState: PlaygroundState) {
        if (event.touches.length === 1 && this.isTouchPanning) {
            const deltaX = (event.touches[0].clientX - this.dragStartX) * window.devicePixelRatio;
            const deltaY = (event.touches[0].clientY - this.dragStartY) * window.devicePixelRatio;
            this.offsetX = this.dragStartOffsetX + deltaX;
            this.offsetY = this.dragStartOffsetY + deltaY;
            playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
        } else if (event.touches.length === 2 && this.isTouchZooming) {
            const currentDistance = this.getTouchDistance(event.touches);
            const center = this.getTouchCenter(event.touches);
            const rect = canvas.getBoundingClientRect();
            const pixelX = (center.x - rect.left) * window.devicePixelRatio;
            const pixelY = (center.y - rect.top) * window.devicePixelRatio;

            if (this.startTouchDistance > 0) {
                const scale = currentDistance / this.startTouchDistance;
                const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * scale));

                this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
                this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
                this.zoom = newZoom;
            }

            const centerDeltaX = (center.x - this.lastTouchCenterX) * window.devicePixelRatio;
            const centerDeltaY = (center.y - this.lastTouchCenterY) * window.devicePixelRatio;
            this.offsetX += centerDeltaX;
            this.offsetY += centerDeltaY;

            this.lastTouchCenterX = center.x;
            this.lastTouchCenterY = center.y;

            playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
        }
    }

    public onTouchEnd(event: TouchEvent, canvas: HTMLElement) {
        if (event.touches.length === 0) {
            this.isTouchPanning = false;
            this.isTouchZooming = false;
        } else if (event.touches.length === 1) {
            this.isTouchPanning = true;
            this.isTouchZooming = false;
            this.dragStartX = event.touches[0].clientX;
            this.dragStartY = event.touches[0].clientY;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        }
    }

    public onGestureStart(event: any) {
        this.scaleStartZoom = this.zoom;
        this.dragStartOffsetX = this.offsetX;
        this.dragStartOffsetY = this.offsetY;
    }

    public onGestureChange(event: any, canvas: HTMLElement, playgroundState: PlaygroundState) {
        const rect = canvas.getBoundingClientRect();
        const pixelX = (event.clientX - rect.left) * window.devicePixelRatio;
        const pixelY = (event.clientY - rect.top) * window.devicePixelRatio;
        const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * event.scale));
        this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
        this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
        this.zoom = newZoom;
        playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    public onGestureEnd() {
    }
}

const LOADING_TIMEOUT_MS = 60000;

const playgroundState = new PlaygroundState();
const gestureManager = new GestureManager();
const loadingProgress = new LoadingProgress();
let animationLoopRunning = false;
let wasmLoadPromise: Promise<void> | null = null;
let fontLoadPromise: Promise<void> | null = null;
let loadingTimeoutId: number | null = null;

function updateProgressUI(): void {
    const progressBar = document.getElementById('progress-bar');
    const progressText = document.getElementById('progress-text');
    if (progressBar && progressText) {
        const progress = loadingProgress.getOverallProgress();
        progressBar.style.width = `${progress}%`;
        progressText.textContent = `${progress}%`;
    }
}

function resetProgressUI(): void {
    const progressBar = document.getElementById('progress-bar');
    const progressText = document.getElementById('progress-text');
    if (progressBar && progressText) {
        progressBar.style.transition = 'none';
        progressBar.style.width = '0%';
        progressText.textContent = '0%';
        progressBar.offsetHeight;
        progressBar.style.transition = '';
    }
}

async function fetchWithProgress(
    url: string,
    onProgress: (loaded: number, total: number) => void
): Promise<ArrayBuffer> {
    const response = await fetch(url);
    if (!response.ok) {
        throw new Error(`Failed to fetch ${url}: ${response.status}`);
    }
    const contentLength = response.headers.get('content-length');
    const total = contentLength ? parseInt(contentLength, 10) : 0;
    if (total > 0) {
        onProgress(0, total);
    }
    const reader = response.body?.getReader();
    if (!reader) {
        const buffer = await response.arrayBuffer();
        onProgress(buffer.byteLength, buffer.byteLength);
        return buffer;
    }
    const chunks: Uint8Array[] = [];
    let loaded = 0;
    while (true) {
        const { done, value } = await reader.read();
        if (done) {
            break;
        }
        chunks.push(value);
        loaded += value.length;
        onProgress(loaded, total > 0 ? total : loaded);
    }
    const result = new Uint8Array(loaded);
    let offset = 0;
    for (const chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
    }
    return result.buffer;
}

async function loadFonts(): Promise<void> {
    const loadFont = async (
        url: string,
        cacheKey: string,
        onProgress: (loaded: number, total: number) => void,
        onDone: () => void
    ): Promise<Uint8Array | null> => {
        try {
            // Try cache first
            const cached = await getCached(cacheKey);
            if (cached) {
                console.log(`[PAGX Viewer] Font loaded from cache: ${cacheKey}`);
                onDone();
                updateProgressUI();
                return new Uint8Array(cached);
            }
            
            const buffer = await fetchWithProgress(url, onProgress);
            onDone();
            updateProgressUI();
            // Cache for next time (non-blocking)
            putCache(cacheKey, buffer);
            return new Uint8Array(buffer);
        } catch (error) {
            console.warn(`Error loading font ${url}:`, error);
            onDone();
            updateProgressUI();
            return null;
        }
    };

    const [fontData, emojiFontData] = await Promise.all([
        loadFont(
            FONT_URL,
            'font-noto-sans',
            (loaded, total) => {
                loadingProgress.fontLoaded = loaded;
                if (total > 0) {
                    loadingProgress.fontTotal = total;
                }
                updateProgressUI();
            },
            () => { loadingProgress.fontDone = true; }
        ),
        loadFont(
            EMOJI_FONT_URL,
            'font-noto-emoji',
            (loaded, total) => {
                loadingProgress.emojiFontLoaded = loaded;
                if (total > 0) {
                    loadingProgress.emojiFontTotal = total;
                }
                updateProgressUI();
            },
            () => { loadingProgress.emojiFontDone = true; }
        ),
    ]);
    playgroundState.fontData = fontData;
    playgroundState.emojiFontData = emojiFontData;
}

async function loadWasm(): Promise<void> {
    // Try cache first
    let wasmBuffer: ArrayBuffer;
    const cached = await getCached('wasm-binary');
    if (cached) {
        console.log('[PAGX Viewer] WASM loaded from cache');
        wasmBuffer = cached;
        loadingProgress.wasmDone = true;
        updateProgressUI();
    } else {
        wasmBuffer = await fetchWithProgress(WASM_URL, (loaded, total) => {
            loadingProgress.wasmLoaded = loaded;
            if (total > 0) {
                loadingProgress.wasmTotal = total;
            }
            updateProgressUI();
        });
        loadingProgress.wasmDone = true;
        updateProgressUI();
        // Cache for next time (non-blocking)
        putCache('wasm-binary', wasmBuffer);
    }

    const module = await PAGXWasm({
        locateFile: (file: string) => chrome.runtime.getURL('wasm/' + file),
        mainScriptUrlOrBlob: chrome.runtime.getURL('wasm/pagx-playground.js'),
        wasmBinary: wasmBuffer,
    });
    playgroundState.module = module as PAGXModule;
    TGFXBind(playgroundState.module as any);
    const pagxView = playgroundState.module.PAGXView.MakeFrom('#pagx-canvas');
    if (!pagxView) {
        throw new Error('Failed to create PAGXView');
    }
    playgroundState.pagxView = pagxView;
    updateSize();
    playgroundState.pagxView.updateZoomScaleAndOffset(1.0, 0, 0);
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    bindCanvasEvents(canvas);
    animationLoop();
    setupVisibilityListeners();
}

function registerFontsToView(): void {
    if (!playgroundState.pagxView) {
        return;
    }
    const fontData = playgroundState.fontData || new Uint8Array(0);
    const emojiFontData = playgroundState.emojiFontData || new Uint8Array(0);
    playgroundState.pagxView.registerFonts(fontData, emojiFontData);
}

function updateSize() {
    if (!playgroundState.pagxView) {
        return;
    }
    playgroundState.resized = false;
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const containerRect = container.getBoundingClientRect();
    const scaleFactor = window.devicePixelRatio;
    let canvasWidth = containerRect.width;
    let canvasHeight = containerRect.height;
    const dividerSize = 4;
    if (playgroundState.codeViewVisible) {
        if (window.innerWidth <= 600) {
            canvasHeight = (containerRect.height - dividerSize) / 2;
        } else {
            canvasWidth = (containerRect.width - dividerSize) / 2;
        }
    }
    canvas.width = canvasWidth * scaleFactor;
    canvas.height = canvasHeight * scaleFactor;
    canvas.style.width = canvasWidth + "px";
    canvas.style.height = canvasHeight + "px";
    playgroundState.pagxView.updateSize();
}

function draw() {
    playgroundState.pagxView?.draw();
}

function animationLoop() {
    if (animationLoopRunning) {
        return;
    }
    animationLoopRunning = true;
    const frame = () => {
        if (!playgroundState.pagxView || !playgroundState.isPageVisible) {
            animationLoopRunning = false;
            playgroundState.animationFrameId = null;
            return;
        }
        draw();
        playgroundState.animationFrameId = requestAnimationFrame(frame);
    };
    playgroundState.animationFrameId = requestAnimationFrame(frame);
}

function handleVisibilityChange() {
    playgroundState.isPageVisible = !document.hidden;
    if (playgroundState.isPageVisible && playgroundState.animationFrameId === null) {
        animationLoop();
    }
}

function setupVisibilityListeners() {
    document.addEventListener('visibilitychange', handleVisibilityChange);
    window.addEventListener('beforeunload', () => {
        if (playgroundState.animationFrameId !== null) {
            cancelAnimationFrame(playgroundState.animationFrameId);
            playgroundState.animationFrameId = null;
        }
    });
}

function bindCanvasEvents(canvas: HTMLElement) {
    canvas.style.cursor = 'grab';

    canvas.addEventListener('wheel', (e: WheelEvent) => {
        e.preventDefault();
        gestureManager.onWheel(e, canvas, playgroundState);
    }, { passive: false });

    canvas.addEventListener('mousedown', (e: MouseEvent) => {
        e.preventDefault();
        gestureManager.onMouseDown(e, canvas);
    });
    canvas.addEventListener('mousemove', (e: MouseEvent) => {
        gestureManager.onMouseMove(e, playgroundState);
    });
    canvas.addEventListener('mouseup', () => {
        gestureManager.onMouseUp(canvas);
    });
    canvas.addEventListener('mouseleave', () => {
        gestureManager.onMouseUp(canvas);
    });

    canvas.addEventListener('touchstart', (e: TouchEvent) => {
        e.preventDefault();
        gestureManager.onTouchStart(e, canvas);
    }, { passive: false });
    canvas.addEventListener('touchmove', (e: TouchEvent) => {
        e.preventDefault();
        gestureManager.onTouchMove(e, canvas, playgroundState);
    }, { passive: false });
    canvas.addEventListener('touchend', (e: TouchEvent) => {
        gestureManager.onTouchEnd(e, canvas);
    });
    canvas.addEventListener('touchcancel', (e: TouchEvent) => {
        gestureManager.onTouchEnd(e, canvas);
    });

    canvas.addEventListener('gesturestart', (e: Event) => {
        e.preventDefault();
        gestureManager.onGestureStart(e as any);
    });
    canvas.addEventListener('gesturechange', (e: Event) => {
        e.preventDefault();
        gestureManager.onGestureChange(e as any, canvas, playgroundState);
    });
    canvas.addEventListener('gestureend', (e: Event) => {
        e.preventDefault();
        gestureManager.onGestureEnd();
    });
}

type DropZoneState = 'dropzone' | 'loading' | 'error';

function setDropZoneState(state: DropZoneState, errorMessage?: string): void {
    const dropZoneContent = document.getElementById('drop-zone-content');
    const loadingContent = document.getElementById('loading-content');
    const errorContent = document.getElementById('error-content');
    const dropZone = document.getElementById('drop-zone');
    if (!dropZoneContent || !loadingContent || !errorContent || !dropZone) {
        return;
    }
    dropZoneContent.classList.toggle('hidden', state !== 'dropzone');
    loadingContent.classList.toggle('hidden', state !== 'loading');
    errorContent.classList.toggle('hidden', state !== 'error');
    dropZone.classList.remove('hidden');
    if (state === 'error' && errorMessage !== undefined) {
        const errorMessageEl = document.getElementById('error-message');
        if (errorMessageEl) {
            errorMessageEl.textContent = errorMessage;
        }
    }
}

function showLoadingUI(): void {
    setDropZoneState('loading');
}

function showDropZoneUI(): void {
    setDropZoneState('dropzone');
}

function showErrorUI(message: string, showRetry: boolean = false): void {
    setDropZoneState('error', message);
    const retryBtn = document.getElementById('retry-btn');
    if (retryBtn) {
        retryBtn.classList.toggle('hidden', !showRetry);
    }
}

function hideDropZone(): void {
    const dropZone = document.getElementById('drop-zone');
    if (dropZone) {
        dropZone.classList.add('hidden');
    }
}

function hidePlaybackUI(): void {
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    const toolbar = document.getElementById('toolbar') as HTMLDivElement;
    canvas.classList.add('hidden');
    toolbar.classList.add('hidden');
}

async function loadExternalFiles(baseURL: string): Promise<void> {
    if (!playgroundState.pagxView) {
        return;
    }
    const paths = playgroundState.pagxView.getExternalFilePaths();
    const count = paths.size();
    if (count === 0) {
        paths.delete();
        return;
    }
    const fetches: Promise<void>[] = [];
    for (let i = 0; i < count; i++) {
        const filePath = paths.get(i);
        
        // Validate path to prevent directory traversal
        if (!filePath || filePath.startsWith('/') || filePath.includes('..')) {
            console.warn(`[PAGX Viewer] Skipping invalid external file path: ${filePath}`);
            continue;
        }
        
        const fileURL = baseURL + filePath;
        fetches.push(
            new Promise<void>((resolve) => {
                chrome.runtime.sendMessage(
                    { type: 'fetchUrl', url: fileURL },
                    (response: any) => {
                        if (!response) {
                            console.warn(`[PAGX Viewer] No response for external file: ${filePath}`);
                            resolve();
                            return;
                        }
                        if (response.error) {
                            console.warn(`[PAGX Viewer] Failed to load external file ${filePath}: ${response.error}`);
                            resolve();
                            return;
                        }
                        if (response.data) {
                            try {
                                const data = new Uint8Array(response.data);
                                playgroundState.pagxView?.loadFileData(filePath, data);
                            } catch (error) {
                                console.error(`[PAGX Viewer] Error loading external file ${filePath}:`, error);
                            }
                        }
                        resolve();
                    }
                );
            })
        );
    }
    paths.delete();
    await Promise.all(fetches);
}

async function loadPAGXData(data: Uint8Array, name: string, baseURL: string) {
    if (!playgroundState.pagxView) {
        throw new Error('PAGXView not initialized');
    }

    // Prevent concurrent loads
    if (playgroundState.isLoading) {
        throw new Error('Another file is already loading');
    }
    playgroundState.isLoading = true;

    try {
        // Validate input
        if (!data || data.length === 0) {
            throw new Error('File is empty');
        }

        // Store raw PAGX text for code viewer
        playgroundState.pagxText = new TextDecoder().decode(data);

        const toolbar = document.getElementById('toolbar') as HTMLDivElement;
        const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
        const fileNameEl = document.getElementById('file-name');

        registerFontsToView();
        playgroundState.pagxView.parsePAGX(data);
        await loadExternalFiles(baseURL);
        playgroundState.pagxView.buildLayers();
        gestureManager.resetTransform(playgroundState);
        hideDropZone();
        canvas.classList.remove('hidden');
        toolbar.classList.remove('hidden');
        updateSize();
        draw();
        if (fileNameEl) {
            fileNameEl.textContent = name;
        }
        document.title = 'PAGX Viewer - ' + name;

        // Update code panel if currently visible
        if (playgroundState.codeViewVisible) {
            updateCodePanelContent();
        }
    } finally {
        playgroundState.isLoading = false;
    }
}

function startLoadingTimeout(): void {
    clearLoadingTimeout();
    loadingTimeoutId = window.setTimeout(() => {
        loadingTimeoutId = null;
        showErrorUI('Loading timed out. Please check your network and try again.', true);
    }, LOADING_TIMEOUT_MS);
}

function clearLoadingTimeout(): void {
    if (loadingTimeoutId !== null) {
        clearTimeout(loadingTimeoutId);
        loadingTimeoutId = null;
    }
}

async function prepareForLoading(): Promise<void> {
    hidePlaybackUI();
    showLoadingUI();
    resetProgressUI();
    startLoadingTimeout();
    await new Promise(resolve => requestAnimationFrame(resolve));

    if (!wasmLoadPromise) {
        wasmLoadPromise = loadWasm();
    }
    if (!fontLoadPromise) {
        fontLoadPromise = loadFonts();
    }

    const loadingStartTime = Date.now();
    await Promise.all([wasmLoadPromise, fontLoadPromise]);
    clearLoadingTimeout();
    updateProgressUI();

    const elapsed = Date.now() - loadingStartTime;
    const minDisplayTime = 300;
    if (elapsed < minDisplayTime) {
        await new Promise(resolve => setTimeout(resolve, minDisplayTime - elapsed));
    }
}

async function loadPAGXFile(file: File) {
    try {
        await prepareForLoading();

        const fileBuffer = await file.arrayBuffer();
        await loadPAGXData(new Uint8Array(fileBuffer), file.name, '');
    } catch (error) {
        console.error('Failed to load PAGX file:', error);
        showErrorUI('Failed to load file. Please check the file format.');
    }
}

// SVG path for the code icon (shared between on/off states)
const CODE_ICON_PATH = 'M128 128h768a42.666667 42.666667 0 0 1 42.666667 42.666667v682.666666a42.666667 42.666667 0 0 1-42.666667 42.666667H128a42.666667 42.666667 0 0 1-42.666667-42.666667V170.666667a42.666667 42.666667 0 0 1 42.666667-42.666667z m42.666667 85.333333v597.333334h682.666666V213.333333H170.666667z m682.666666 298.666667l-150.826666 150.869333-60.373334-60.373333L732.672 512 642.133333 421.504l60.373334-60.373333L853.333333 512zM291.328 512l90.538667 90.496-60.330667 60.373333L170.666667 512l150.869333-150.869333L381.866667 421.546667 291.328 512z m188.416 213.333333H388.949333l155.306667-426.666666h90.794667l-155.306667 426.666666z';
const CODE_ICON_COLOR_ON = '#1e7aeb';
const CODE_ICON_COLOR_OFF = '#cccccc';

function highlightXml(text: string): string {
    // Escape HTML entities first
    let escaped = text
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');

    // Highlight XML comments: <!-- ... -->
    escaped = escaped.replace(
        /&lt;!--[\s\S]*?--&gt;/g,
        match => `<span class="xml-comment">${match}</span>`
    );

    // Highlight XML declaration: <?xml ... ?>
    escaped = escaped.replace(
        /&lt;\?[\s\S]*?\?&gt;/g,
        match => `<span class="xml-declaration">${match}</span>`
    );

    // Highlight tags and attributes
    escaped = escaped.replace(
        /(&lt;\/?)([\w:.-]+)((?:\s+[\s\S]*?)?)(\/?&gt;)/g,
        (_match, open, tagName, attrs, close) => {
            let highlightedAttrs = attrs;
            if (attrs) {
                highlightedAttrs = attrs.replace(
                    /([\w:.-]+)(\s*=\s*)(&quot;[^&]*&quot;)/g,
                    '<span class="xml-attr-name">$1</span>$2<span class="xml-attr-value">$3</span>'
                );
            }
            return `<span class="xml-tag">${open}${tagName}</span>${highlightedAttrs}<span class="xml-tag">${close}</span>`;
        }
    );

    return escaped;
}

function updateCodePanelContent(): void {
    const codeContent = document.getElementById('code-content');
    if (!codeContent) {
        return;
    }
    if (playgroundState.pagxText) {
        codeContent.innerHTML = highlightXml(playgroundState.pagxText);
    } else {
        codeContent.textContent = '(No PAGX data loaded)';
    }
}

function toggleCodeView(visible: boolean): void {
    const container = document.getElementById('container') as HTMLDivElement;
    const codePanel = document.getElementById('code-panel') as HTMLDivElement;
    const codeToggleBtn = document.getElementById('code-toggle-btn') as HTMLButtonElement;
    const splitDivider = document.getElementById('split-divider') as HTMLDivElement;
    if (!container || !codePanel || !codeToggleBtn) {
        return;
    }

    playgroundState.codeViewVisible = visible;

    if (visible) {
        container.classList.add('split-view');
        codePanel.classList.remove('hidden');
        splitDivider?.classList.remove('hidden');
        codeToggleBtn.classList.add('active');
        codeToggleBtn.setAttribute('aria-pressed', 'true');
        // Update icon color to blue (on)
        const svgPath = codeToggleBtn.querySelector('path');
        if (svgPath) {
            svgPath.setAttribute('fill', CODE_ICON_COLOR_ON);
        }
        updateCodePanelContent();
    } else {
        container.classList.remove('split-view');
        codePanel.classList.add('hidden');
        splitDivider?.classList.add('hidden');
        codeToggleBtn.classList.remove('active');
        codeToggleBtn.setAttribute('aria-pressed', 'false');
        // Update icon color to gray (off)
        const svgPath = codeToggleBtn.querySelector('path');
        if (svgPath) {
            svgPath.setAttribute('fill', CODE_ICON_COLOR_OFF);
        }
    }

    // Canvas size changed due to split-view toggle, need to update
    if (playgroundState.pagxView) {
        requestAnimationFrame(() => {
            updateSize();
            draw();
        });
    }
}

function setupCodeToggle(): void {
    const codeToggleBtn = document.getElementById('code-toggle-btn');
    if (!codeToggleBtn) {
        return;
    }
    codeToggleBtn.addEventListener('click', () => {
        toggleCodeView(!playgroundState.codeViewVisible);
    });
}

function setupDragAndDrop() {
    const dropZone = document.getElementById('drop-zone') as HTMLDivElement;
    const dropZoneContent = document.getElementById('drop-zone-content') as HTMLDivElement;
    const errorContent = document.getElementById('error-content') as HTMLDivElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const fileInput = document.getElementById('file-input') as HTMLInputElement;
    const openBtn = document.getElementById('open-btn') as HTMLButtonElement;
    const resetBtn = document.getElementById('reset-btn') as HTMLButtonElement;

    const preventDefaults = (e: Event) => {
        e.preventDefault();
        e.stopPropagation();
    };

    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        container.addEventListener(eventName, preventDefaults, false);
        document.body.addEventListener(eventName, preventDefaults, false);
    });

    ['dragenter', 'dragover'].forEach(eventName => {
        container.addEventListener(eventName, () => {
            dropZone.classList.add('drag-over');
            dropZone.classList.remove('hidden');
        }, false);
    });

    ['dragleave', 'drop'].forEach(eventName => {
        container.addEventListener(eventName, () => {
            dropZone.classList.remove('drag-over');
        }, false);
    });

    container.addEventListener('drop', (e: DragEvent) => {
        const files = e.dataTransfer?.files;
        if (files && files.length > 0) {
            const file = files[0];
            if (file.name.endsWith('.pagx')) {
                loadPAGXFile(file);
            } else {
                showErrorUI('Please drop a .pagx file');
            }
        }
    }, false);

    const openFilePicker = () => fileInput.click();

    dropZoneContent.addEventListener('click', openFilePicker);
    dropZoneContent.addEventListener('keydown', (e: KeyboardEvent) => {
        if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            openFilePicker();
        }
    });

    errorContent.addEventListener('click', openFilePicker);

    const retryBtn = document.getElementById('retry-btn') as HTMLButtonElement;
    if (retryBtn) {
        retryBtn.addEventListener('click', (e: Event) => {
            e.stopPropagation();
            wasmLoadPromise = null;
            fontLoadPromise = null;
            showLoadingUI();
            prepareForLoading().catch(error => {
                showErrorUI('Retry failed: ' + (error instanceof Error ? error.message : String(error)), true);
            });
        });
    }

    openBtn.addEventListener('click', () => {
        fileInput.click();
    });

    resetBtn.addEventListener('click', () => {
        gestureManager.resetTransform(playgroundState);
    });

    setupCodeToggle();

    fileInput.addEventListener('change', () => {
        const files = fileInput.files;
        if (files && files.length > 0) {
            loadPAGXFile(files[0]);
        }
        fileInput.value = '';
    });
}

if (typeof window !== 'undefined') {
    let resizeTimeout: number | null = null;

    window.onload = async () => {
        setupDragAndDrop();

        console.log('[PAGX Viewer] Starting initialization...');

        // Clean up stale cache from previous versions (non-blocking)
        clearStaleCache();

        wasmLoadPromise = loadWasm().catch(error => {
            console.error('[PAGX Viewer] WASM load failed:', error);
            showErrorUI('Failed to initialize WASM: ' + (error instanceof Error ? error.message : String(error)));
            throw error;
        });
        fontLoadPromise = loadFonts().catch(error => {
            console.error('[PAGX Viewer] Font load failed:', error);
            throw error;
        });

        // Check URL parameters to get cached PAGX data from Background
        const params = new URLSearchParams(window.location.search);
        const tabId = params.get('tabId');
        if (tabId) {
            try {
                await prepareForLoading();
                const response: any = await new Promise((resolve) => {
                    chrome.runtime.sendMessage({ type: 'getPagxData', tabId: parseInt(tabId) }, resolve);
                });
                if (response && response.text) {
                    console.log('[PAGX Viewer] Got PAGX data from background, length:', response.text.length);
                    await loadPAGXData(
                        new TextEncoder().encode(response.text),
                        'document.pagx',
                        ''
                    );
                } else {
                    // No cached data (e.g. page was refreshed). Show the drop zone.
                    console.log('[PAGX Viewer] No cached data for this tab, showing drop zone');
                    showDropZoneUI();
                }
            } catch (error) {
                console.error('[PAGX Viewer] Failed to load PAGX:', error);
                showErrorUI('Failed to load file: ' + (error instanceof Error ? error.message : String(error)));
            }
        }
    };

    window.onresize = () => {
        if (!playgroundState.pagxView || playgroundState.resized) {
            return;
        }
        playgroundState.resized = true;
        if (resizeTimeout !== null) {
            clearTimeout(resizeTimeout);
        }
        resizeTimeout = window.setTimeout(() => {
            updateSize();
            playgroundState.resized = false;
            resizeTimeout = null;
        }, 300);
    };

    window.onunload = () => {
        // Clean up animation frame
        if (playgroundState.animationFrameId !== null) {
            cancelAnimationFrame(playgroundState.animationFrameId);
            playgroundState.animationFrameId = null;
        }
        // Clean up resize timeout
        if (resizeTimeout !== null) {
            clearTimeout(resizeTimeout);
            resizeTimeout = null;
        }
        // Request cleanup of WASM resources
        if (playgroundState.pagxView) {
            try {
                playgroundState.pagxView.delete?.();
            } catch (e) {
                // Ignore errors during cleanup
            }
        }
    };
}
