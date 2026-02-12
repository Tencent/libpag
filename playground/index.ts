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

import { PAGXModule, PAGXView } from './types';
import PAGXWasm from './wasm-mt/pagx-playground';
import { TGFXBind } from '@tgfx/binding';

interface I18nStrings {
    dropText: string;
    dropSubtext: string;
    loading: string;
    errorTitle: string;
    errorFormat: string;
    errorWasm: string;
    errorFont: string;
    errorNetwork: string;
    errorBrowser: string;
    openFile: string;
    resetView: string;
    invalidFile: string;
    spec: string;
    specTitle: string;
    leave: string;
    samples: string;
    samplesTitle: string;
    home: string;
    back: string;
}

const i18n: Record<string, I18nStrings> = {
    en: {
        dropText: 'Drag & Drop PAGX file here',
        dropSubtext: 'or click to browse',
        loading: 'Loading...',
        errorTitle: 'Failed to load',
        errorFormat: 'Invalid file format. Please use a valid .pagx file.',
        errorWasm: 'Failed to load WebAssembly module.',
        errorFont: 'Failed to load fonts.',
        errorNetwork: 'Network error. Please check your connection.',
        errorBrowser: 'Minimum browser versions required:',
        openFile: 'Open PAGX File',
        resetView: 'Reset View',
        invalidFile: 'Please drop a .pagx file',
        spec: 'Spec',
        specTitle: 'PAGX Specification',
        leave: 'Leave',
        samples: 'Samples',
        samplesTitle: 'PAGX Samples',
        home: 'Home',
        back: 'Back',
    },
    zh: {
        dropText: '拖放 PAGX 文件到此处',
        dropSubtext: '或点击选择文件',
        loading: '加载中...',
        errorTitle: '加载失败',
        errorFormat: '无效的文件格式，请使用有效的 .pagx 文件。',
        errorWasm: 'WebAssembly 模块加载失败。',
        errorFont: '字体加载失败。',
        errorNetwork: '网络错误，请检查网络连接。',
        errorBrowser: '浏览器最低版本要求：',
        openFile: '打开 PAGX 文件',
        resetView: '重置视图',
        invalidFile: '请拖放 .pagx 文件',
        spec: 'Spec',
        specTitle: 'PAGX 格式规范',
        leave: '离开',
        samples: '示例',
        samplesTitle: 'PAGX 示例',
        home: '首页',
        back: '返回',
    },
};

function getLocale(): string {
    const params = new URLSearchParams(window.location.search);
    const langParam = params.get('lang');
    if (langParam === 'zh' || langParam === 'en') {
        return langParam;
    }
    const lang = navigator.language || '';
    return lang.toLowerCase().startsWith('zh') ? 'zh' : 'en';
}

function t(): I18nStrings {
    return i18n[getLocale()];
}

const MIN_ZOOM = 0.001;
const MAX_ZOOM = 1000.0;

// Font URLs for preloading
const FONT_URL = './fonts/NotoSansSC-Regular.otf';
const EMOJI_FONT_URL = './fonts/NotoColorEmoji.ttf';
const WASM_URL = './wasm-mt/pagx-playground.wasm';

// Estimated sizes for progress calculation (in bytes)
const ESTIMATED_WASM_SIZE = 2400000;
const ESTIMATED_FONT_SIZE = 8800000;
const ESTIMATED_EMOJI_FONT_SIZE = 10300000;

class PlaygroundState {
    module: PAGXModule | null = null;
    pagxView: PAGXView | null = null;
    animationFrameId: number | null = null;
    isPageVisible: boolean = true;
    resized: boolean = false;
    zoom: number = 1.0;
    offsetX: number = 0;
    offsetY: number = 0;
    fontData: Uint8Array | null = null;
    emojiFontData: Uint8Array | null = null;
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
        // Only count resources that are not yet done
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
            // All resources cached, show 99%
            return 99;
        }
        // Cap at 99%, reserve 100% for after PAGX file loaded
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

enum ScaleGestureState {
    SCALE_START = 0,
    SCALE_CHANGE = 1,
    SCALE_END = 2,
}

enum ScrollGestureState {
    SCROLL_START = 0,
    SCROLL_CHANGE = 1,
    SCROLL_END = 2,
}

enum DeviceType {
    TOUCH = 0,
    MOUSE = 1,
}

class GestureManager {
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

    // Drag state
    private isDragging = false;
    private dragStartX = 0;
    private dragStartY = 0;
    private dragStartOffsetX = 0;
    private dragStartOffsetY = 0;

    // Touch state
    private lastTouchDistance = 0;
    private lastTouchCenterX = 0;
    private lastTouchCenterY = 0;
    private isTouchPanning = false;
    private isTouchZooming = false;

    public zoom = 1.0;
    public offsetX = 0;
    public offsetY = 0;

    private handleScrollEvent(
        event: WheelEvent,
        state: ScrollGestureState,
        playgroundState: PlaygroundState,
    ) {
        if (state === ScrollGestureState.SCROLL_CHANGE) {
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
        state: ScaleGestureState,
        canvas: HTMLElement,
        playgroundState: PlaygroundState,
    ) {
        if (state === ScaleGestureState.SCALE_START) {
            this.scaleY = 1.0;
            this.scaleStartZoom = this.zoom;
        }
        if (state === ScaleGestureState.SCALE_CHANGE) {
            const rect = canvas.getBoundingClientRect();
            const pixelX = (event.clientX - rect.left) * window.devicePixelRatio;
            const pixelY = (event.clientY - rect.top) * window.devicePixelRatio;
            const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * this.scaleY));
            this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
            this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
            this.zoom = newZoom;
        }
        if (state === ScaleGestureState.SCALE_END) {
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
            this.handleScrollEvent(event, ScrollGestureState.SCROLL_END, playgroundState);
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
            this.handleScaleEvent(event, ScaleGestureState.SCALE_END, canvas, playgroundState);
            this.clearState();
        }, this.pinchTimeout);
    }

    private getDeviceType(event: WheelEvent): DeviceType {
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
        return isTouchpad ? DeviceType.TOUCH : DeviceType.MOUSE;
    }

    public onWheel(event: WheelEvent, canvas: HTMLElement, playgroundState: PlaygroundState) {
        const deviceType = this.getDeviceType(event);
        let wheelRatio = (deviceType === DeviceType.MOUSE ? this.mouseWheelRatio : this.touchWheelRatio);
        if (!event.deltaY || (!event.ctrlKey && !event.metaKey)) {
            this.resetScrollTimeout(event, playgroundState);
            this.handleScrollEvent(event, ScrollGestureState.SCROLL_CHANGE, playgroundState);
        } else {
            this.scaleY *= Math.exp(-(event.deltaY) / wheelRatio);
            if (!this.timer) {
                this.resetScaleTimeout(event, canvas, playgroundState);
                this.handleScaleEvent(event, ScaleGestureState.SCALE_START, canvas, playgroundState);
            } else {
                this.resetScaleTimeout(event, canvas, playgroundState);
                this.handleScaleEvent(event, ScaleGestureState.SCALE_CHANGE, canvas, playgroundState);
            }
        }
    }

    // Mouse drag handlers
    public onMouseDown(event: MouseEvent, canvas: HTMLElement) {
        // Only handle left mouse button
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

    // Touch handlers
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
            // Single finger pan
            this.isTouchPanning = true;
            this.isTouchZooming = false;
            this.dragStartX = event.touches[0].clientX;
            this.dragStartY = event.touches[0].clientY;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        } else if (event.touches.length === 2) {
            // Two finger zoom/pan
            this.isTouchPanning = false;
            this.isTouchZooming = true;
            this.lastTouchDistance = this.getTouchDistance(event.touches);
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
            // Single finger pan
            const deltaX = (event.touches[0].clientX - this.dragStartX) * window.devicePixelRatio;
            const deltaY = (event.touches[0].clientY - this.dragStartY) * window.devicePixelRatio;
            this.offsetX = this.dragStartOffsetX + deltaX;
            this.offsetY = this.dragStartOffsetY + deltaY;
            playgroundState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
        } else if (event.touches.length === 2 && this.isTouchZooming) {
            // Two finger zoom and pan
            const currentDistance = this.getTouchDistance(event.touches);
            const center = this.getTouchCenter(event.touches);
            const rect = canvas.getBoundingClientRect();
            const pixelX = (center.x - rect.left) * window.devicePixelRatio;
            const pixelY = (center.y - rect.top) * window.devicePixelRatio;

            // Calculate zoom
            if (this.lastTouchDistance > 0) {
                const scale = currentDistance / this.lastTouchDistance;
                const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.zoom * scale));

                // Zoom around pinch center
                this.offsetX = (this.offsetX - pixelX) * (newZoom / this.zoom) + pixelX;
                this.offsetY = (this.offsetY - pixelY) * (newZoom / this.zoom) + pixelY;
                this.zoom = newZoom;
            }

            // Also handle pan during pinch
            const centerDeltaX = (center.x - this.lastTouchCenterX) * window.devicePixelRatio;
            const centerDeltaY = (center.y - this.lastTouchCenterY) * window.devicePixelRatio;
            this.offsetX += centerDeltaX;
            this.offsetY += centerDeltaY;

            this.lastTouchDistance = currentDistance;
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
            // Switched from pinch to single finger
            this.isTouchPanning = true;
            this.isTouchZooming = false;
            this.dragStartX = event.touches[0].clientX;
            this.dragStartY = event.touches[0].clientY;
            this.dragStartOffsetX = this.offsetX;
            this.dragStartOffsetY = this.offsetY;
        }
    }
}

const playgroundState = new PlaygroundState();
const gestureManager = new GestureManager();
const loadingProgress = new LoadingProgress();
let animationLoopRunning = false;
let wasmLoadPromise: Promise<void> | null = null;
let fontLoadPromise: Promise<void> | null = null;

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
        // Remove transition temporarily to reset instantly
        progressBar.style.transition = 'none';
        progressBar.style.width = '0%';
        progressText.textContent = '0%';
        // Force reflow to apply the reset
        progressBar.offsetHeight;
        // Restore transition
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
        onProgress: (loaded: number, total: number) => void,
        onDone: () => void
    ): Promise<Uint8Array | null> => {
        try {
            const buffer = await fetchWithProgress(url, onProgress);
            onDone();
            updateProgressUI();
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
    // First fetch the WASM file with progress tracking
    const wasmBuffer = await fetchWithProgress(WASM_URL, (loaded, total) => {
        loadingProgress.wasmLoaded = loaded;
        if (total > 0) {
            loadingProgress.wasmTotal = total;
        }
        updateProgressUI();
    });
    loadingProgress.wasmDone = true;
    updateProgressUI();
    // Then instantiate the module with the pre-fetched WASM
    const module = await PAGXWasm({
        locateFile: (file: string) => './wasm-mt/' + file,
        mainScriptUrlOrBlob: './wasm-mt/pagx-playground.js',
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
    const screenRect = container.getBoundingClientRect();
    const scaleFactor = window.devicePixelRatio;
    canvas.width = screenRect.width * scaleFactor;
    canvas.height = screenRect.height * scaleFactor;
    canvas.style.width = screenRect.width + "px";
    canvas.style.height = screenRect.height + "px";
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
    // Set initial cursor style
    canvas.style.cursor = 'grab';

    // Wheel events for scroll and zoom
    canvas.addEventListener('wheel', (e: WheelEvent) => {
        e.preventDefault();
        gestureManager.onWheel(e, canvas, playgroundState);
    }, { passive: false });

    // Mouse drag events
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

    // Touch events for mobile
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

    // Prevent browser pinch-to-zoom on Safari
    canvas.addEventListener('gesturestart', (e: Event) => {
        e.preventDefault();
    });
    canvas.addEventListener('gesturechange', (e: Event) => {
        e.preventDefault();
    });
    canvas.addEventListener('gestureend', (e: Event) => {
        e.preventDefault();
    });
}

function showLoadingUI(): void {
    const dropZoneContent = document.getElementById('drop-zone-content');
    const loadingContent = document.getElementById('loading-content');
    const errorContent = document.getElementById('error-content');
    const dropZone = document.getElementById('drop-zone');
    if (dropZoneContent && loadingContent && errorContent && dropZone) {
        dropZoneContent.classList.add('hidden');
        loadingContent.classList.remove('hidden');
        errorContent.classList.add('hidden');
        dropZone.classList.remove('hidden');
    }
}

function showDropZoneUI(): void {
    const dropZoneContent = document.getElementById('drop-zone-content');
    const loadingContent = document.getElementById('loading-content');
    const errorContent = document.getElementById('error-content');
    const dropZone = document.getElementById('drop-zone');
    if (dropZoneContent && loadingContent && errorContent && dropZone) {
        dropZoneContent.classList.remove('hidden');
        loadingContent.classList.add('hidden');
        errorContent.classList.add('hidden');
        dropZone.classList.remove('hidden');
    }
}

function showErrorUI(message: string): void {
    const dropZoneContent = document.getElementById('drop-zone-content');
    const loadingContent = document.getElementById('loading-content');
    const errorContent = document.getElementById('error-content');
    const errorMessage = document.getElementById('error-message');
    const dropZone = document.getElementById('drop-zone');
    if (dropZoneContent && loadingContent && errorContent && errorMessage && dropZone) {
        dropZoneContent.classList.add('hidden');
        loadingContent.classList.add('hidden');
        errorContent.classList.remove('hidden');
        errorMessage.textContent = message;
        dropZone.classList.remove('hidden');
    }
}

function hideDropZone(): void {
    const dropZone = document.getElementById('drop-zone');
    if (dropZone) {
        dropZone.classList.add('hidden');
    }
}

const DEFAULT_TITLE = 'PAGX Playground';

function goHome(): void {
    if (playgroundState.pagxView) {
        playgroundState.pagxView.loadPAGX(new Uint8Array(0));
        gestureManager.resetTransform(playgroundState);
    }
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    const toolbar = document.getElementById('toolbar') as HTMLDivElement;
    const navBtns = document.getElementById('nav-btns') as HTMLDivElement;
    canvas.classList.add('hidden');
    toolbar.classList.add('hidden');
    navBtns.classList.remove('hidden');
    document.title = DEFAULT_TITLE;
    showDropZoneUI();
    currentPlayingFile = null;

    // Clear file parameter from URL
    const newUrl = new URL(window.location.href);
    newUrl.searchParams.delete('file');
    newUrl.hash = '';
    history.replaceState(null, '', newUrl.toString());
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
        const fileURL = baseURL + filePath;
        fetches.push(
            fetch(fileURL)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`Failed to fetch ${fileURL}: ${response.status}`);
                    }
                    return response.arrayBuffer();
                })
                .then(buffer => {
                    playgroundState.pagxView?.loadFileData(filePath, new Uint8Array(buffer));
                })
                .catch(error => {
                    console.warn(`Failed to load external file ${filePath}:`, error);
                })
        );
    }
    paths.delete();
    await Promise.all(fetches);
}

async function loadPAGXData(data: Uint8Array, name: string, baseURL: string) {
    const navBtns = document.getElementById('nav-btns') as HTMLDivElement;
    const toolbar = document.getElementById('toolbar') as HTMLDivElement;
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;

    if (!playgroundState.pagxView) {
        throw new Error('PAGXView not initialized');
    }

    registerFontsToView();
    playgroundState.pagxView.parsePAGX(data);
    await loadExternalFiles(baseURL);
    playgroundState.pagxView.buildLayers();
    gestureManager.resetTransform(playgroundState);
    updateSize();
    hideDropZone();
    canvas.classList.remove('hidden');
    toolbar.classList.remove('hidden');
    navBtns.classList.add('hidden');
    document.title = 'PAGX Playground - ' + name;
}

async function loadPAGXFile(file: File) {
    // Show loading UI with progress reset to 0%
    const loadingStartTime = Date.now();
    showLoadingUI();
    resetProgressUI();
    // Wait for 0% to render before starting
    await new Promise(resolve => requestAnimationFrame(resolve));

    // Start loading resources if not already started
    if (!wasmLoadPromise) {
        wasmLoadPromise = loadWasm();
    }
    if (!fontLoadPromise) {
        fontLoadPromise = loadFonts();
    }

    try {
        // Wait for WASM and fonts (progress goes from 0% to 99%)
        await Promise.all([wasmLoadPromise, fontLoadPromise]);
        updateProgressUI();

        // Ensure minimum display time for loading UI (300ms)
        const elapsed = Date.now() - loadingStartTime;
        const minDisplayTime = 300;
        if (elapsed < minDisplayTime) {
            await new Promise(resolve => setTimeout(resolve, minDisplayTime - elapsed));
        }

        // Load PAGX file (progress stays at 99% during this)
        const fileBuffer = await file.arrayBuffer();

        // Register fonts and load PAGX file
        await loadPAGXData(new Uint8Array(fileBuffer), file.name, '');
    } catch (error) {
        console.error('Failed to load PAGX file:', error);
        showErrorUI(t().errorFormat);
    }
}

async function loadPAGXFromURL(url: string) {
    // Show loading UI with progress reset to 0%
    const loadingStartTime = Date.now();
    showLoadingUI();
    resetProgressUI();
    // Wait for 0% to render before starting
    await new Promise(resolve => requestAnimationFrame(resolve));

    // Start loading resources if not already started
    if (!wasmLoadPromise) {
        wasmLoadPromise = loadWasm();
    }
    if (!fontLoadPromise) {
        fontLoadPromise = loadFonts();
    }

    try {
        // Wait for WASM and fonts (progress goes from 0% to 99%)
        await Promise.all([wasmLoadPromise, fontLoadPromise]);
        updateProgressUI();

        // Ensure minimum display time for loading UI (300ms)
        const elapsed = Date.now() - loadingStartTime;
        const minDisplayTime = 300;
        if (elapsed < minDisplayTime) {
            await new Promise(resolve => setTimeout(resolve, minDisplayTime - elapsed));
        }

        // Fetch PAGX file from URL
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`Failed to fetch: ${response.status} ${response.statusText}`);
        }
        const fileBuffer = await response.arrayBuffer();

        // Extract filename and base URL
        const lastSlash = url.lastIndexOf('/');
        const baseURL = lastSlash >= 0 ? url.substring(0, lastSlash + 1) : '';
        const name = url.substring(lastSlash + 1) || 'remote.pagx';

        // Register fonts and load PAGX file
        await loadPAGXData(new Uint8Array(fileBuffer), name, baseURL);
        currentPlayingFile = url;

        // Update URL with file parameter (without page reload)
        const newUrl = new URL(window.location.href);
        newUrl.searchParams.set('file', url);
        newUrl.hash = '';
        history.replaceState(null, '', newUrl.toString());
    } catch (error) {
        console.error('Failed to load PAGX from URL:', error);
        const message = error instanceof Error ? error.message : 'Unknown error';
        showErrorUI(message);
    }
}

function getPAGXUrlFromParams(): string | null {
    const params = new URLSearchParams(window.location.search);
    return params.get('file');
}

function setupDragAndDrop() {
    const dropZone = document.getElementById('drop-zone') as HTMLDivElement;
    const dropZoneContent = document.getElementById('drop-zone-content') as HTMLDivElement;
    const errorContent = document.getElementById('error-content') as HTMLDivElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const fileInput = document.getElementById('file-input') as HTMLInputElement;
    const leaveBtn = document.getElementById('leave-btn') as HTMLButtonElement;
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
                alert(t().invalidFile);
            }
        }
    }, false);

    dropZoneContent.addEventListener('click', () => {
        fileInput.click();
    });

    errorContent.addEventListener('click', () => {
        fileInput.click();
    });

    leaveBtn.addEventListener('click', () => {
        goHome();
    });

    openBtn.addEventListener('click', () => {
        fileInput.click();
    });

    resetBtn.addEventListener('click', () => {
        gestureManager.resetTransform(playgroundState);
    });

    fileInput.addEventListener('change', () => {
        const files = fileInput.files;
        if (files && files.length > 0) {
            loadPAGXFile(files[0]);
        }
        fileInput.value = '';
    });
}

function checkWebGL2Support(): boolean {
    const canvas = document.createElement('canvas');
    const gl = canvas.getContext('webgl2');
    return gl !== null;
}

function checkWasmSupport(): boolean {
    try {
        if (typeof WebAssembly === 'object' &&
            typeof WebAssembly.instantiate === 'function') {
            const module = new WebAssembly.Module(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
            return module instanceof WebAssembly.Module;
        }
    } catch (e) {
        // WebAssembly not supported
    }
    return false;
}

function getBrowserRequirements(): string {
    return `${t().errorBrowser}
• Chrome 57+
• Firefox 52+
• Safari 15+
• Edge 79+`;
}

function applyI18n(): void {
    const strings = t();
    const locale = getLocale();
    document.documentElement.lang = locale === 'zh' ? 'zh-CN' : 'en';

    const dropText = document.querySelector('.drop-text');
    const dropSubtext = document.querySelector('.drop-subtext');
    const loadingText = document.querySelector('.loading-text');
    const errorTitle = document.querySelector('.error-title');
    const openBtn = document.getElementById('open-btn');
    const resetBtn = document.getElementById('reset-btn');
    const leaveBtn = document.getElementById('leave-btn');

    if (dropText) dropText.textContent = strings.dropText;
    if (dropSubtext) dropSubtext.textContent = strings.dropSubtext;
    if (loadingText) loadingText.textContent = strings.loading;
    if (errorTitle) errorTitle.textContent = strings.errorTitle;
    if (openBtn) openBtn.title = strings.openFile;
    if (resetBtn) resetBtn.title = strings.resetView;
    if (leaveBtn) leaveBtn.title = strings.leave;

    const specBtn = document.getElementById('spec-btn');
    const specBtnText = document.getElementById('spec-btn-text');
    if (specBtn) specBtn.title = strings.specTitle;
    if (specBtnText) specBtnText.textContent = strings.spec;

    const samplesBtn = document.getElementById('samples-btn');
    const samplesBtnText = document.getElementById('samples-btn-text');
    if (samplesBtn) samplesBtn.title = strings.samplesTitle;
    if (samplesBtnText) samplesBtnText.textContent = strings.samples;

    const toolbarSamplesBtn = document.getElementById('toolbar-samples-btn');
    if (toolbarSamplesBtn) toolbarSamplesBtn.title = strings.samplesTitle;

    const samplesTitle = document.querySelector('.samples-title');
    if (samplesTitle) samplesTitle.textContent = strings.samplesTitle;

    const samplesBackBtn = document.getElementById('samples-back-btn');
    if (samplesBackBtn) {
        samplesBackBtn.title = strings.back;
        const span = samplesBackBtn.querySelector('span');
        if (span) span.textContent = strings.back;
    }

    const samplesSpecBtn = document.getElementById('samples-spec-btn');
    if (samplesSpecBtn) {
        samplesSpecBtn.title = strings.specTitle;
        const span = samplesSpecBtn.querySelector('span');
        if (span) span.textContent = strings.spec;
    }
}

let sampleFiles: string[] = [];
let currentSampleFile: string | null = null;
let currentPlayingFile: string | null = null;

async function loadSampleList(): Promise<void> {
    if (sampleFiles.length > 0) {
        return;
    }
    const response = await fetch('./samples/index.json');
    if (!response.ok) {
        throw new Error('Failed to load samples index');
    }
    sampleFiles = await response.json();
}

function renderSampleList(): void {
    const list = document.getElementById('samples-list') as HTMLDivElement;
    list.innerHTML = '';
    for (const file of sampleFiles) {
        const a = document.createElement('a');
        a.href = '#';

        const baseName = file.replace(/\.pagx$/, '');
        const imageUrl = `./samples/images/${baseName}.webp`;

        a.innerHTML = `
            <img class="sample-image" src="${imageUrl}" alt="${baseName}" loading="lazy">
            <span class="sample-name">${file}</span>
        `;
        if (file === currentSampleFile) {
            a.classList.add('active');
        }
        a.addEventListener('click', (e) => {
            e.preventDefault();
            currentSampleFile = file;
            hideSamplesPage();
            loadPAGXFromURL('./samples/' + file);
        });
        list.appendChild(a);
    }
}

function showSamplesPage(): void {
    const container = document.getElementById('container') as HTMLDivElement;
    const samplesPage = document.getElementById('samples-page') as HTMLDivElement;
    container.classList.add('hidden');
    samplesPage.classList.remove('hidden');
    document.title = t().samplesTitle;

    loadSampleList().then(renderSampleList).catch((error) => {
        console.error('Failed to load samples:', error);
    });
}

function hideSamplesPage(): void {
    const container = document.getElementById('container') as HTMLDivElement;
    const samplesPage = document.getElementById('samples-page') as HTMLDivElement;
    container.classList.remove('hidden');
    samplesPage.classList.add('hidden');
}

function handleRoute(): void {
    const hash = window.location.hash;
    if (hash === '#samples') {
        showSamplesPage();
    } else {
        hideSamplesPage();
    }
}

if (typeof window !== 'undefined') {
    window.onload = async () => {
        // Apply i18n texts
        applyI18n();

        // Setup routing
        window.addEventListener('hashchange', handleRoute);
        handleRoute();

        // Setup drag and drop early so UI is responsive
        setupDragAndDrop();

        if (!checkWasmSupport()) {
            alert('WebAssembly is not supported.\n\n' + getBrowserRequirements());
            return;
        }
        if (!checkWebGL2Support()) {
            alert('WebGL 2.0 is not supported.\n\n' + getBrowserRequirements());
            return;
        }

        // Start preloading resources in background (will be awaited when file is selected)
        wasmLoadPromise = loadWasm().catch(error => {
            console.error('WASM load failed:', error);
            throw error;
        });
        fontLoadPromise = loadFonts().catch(error => {
            console.error('Font load failed:', error);
            throw error;
        });

        // Check for URL parameter and auto-load if present
        const pagxUrl = getPAGXUrlFromParams();
        if (pagxUrl) {
            loadPAGXFromURL(pagxUrl);
        }
    };

    window.onresize = () => {
        if (!playgroundState.pagxView || playgroundState.resized) {
            return;
        }
        playgroundState.resized = true;
        window.setTimeout(() => {
            updateSize();
        }, 300);
    };
}
