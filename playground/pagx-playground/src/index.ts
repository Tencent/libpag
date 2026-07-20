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

// Resolved to the selected arch's pagx-viewer glue at build time via the
// `pagx-viewer-glue` alias in scripts/rollup.js (single-threaded adds a `.st` infix).
import { PAGXInit } from 'pagx-viewer-glue';
import type { PAGXView, PAGXModule } from '../../pagx-viewer/src/ts/pagx';
import { init as initEditor, togglePanel as toggleEditorPanel } from './editor';

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
    docs: string;
    specification: string;
    aiSkills: string;
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
        docs: 'Docs',
        specification: 'Specification',
        aiSkills: 'AI Skills',
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
        spec: '格式',
        specTitle: 'PAGX 格式规范',
        docs: '文档',
        specification: '格式规范',
        aiSkills: 'AI Skills',
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

// __PAGX_INFIX__ is injected at build time by scripts/rollup.js: empty for the
// multi-threaded build, ".st" for the single-threaded build. The matching wasm is
// placed at wasm-mt/ by scripts/prebuild.js.
declare const __PAGX_INFIX__: string;
const WASM_URL = `./wasm-mt/pagx-viewer${__PAGX_INFIX__}.wasm`;

// Estimated sizes for progress calculation (in bytes)
const ESTIMATED_WASM_SIZE = 2400000;
const ESTIMATED_FONT_SIZE = 8800000;
const ESTIMATED_EMOJI_FONT_SIZE = 10300000;

class PlaygroundState {
    module: PAGXModule | null = null;
    pagxView: PAGXView | null = null;
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

interface GestureEvent extends UIEvent {
    scale: number;
    rotation: number;
    clientX: number;
    clientY: number;
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

            // Calculate zoom using absolute ratio relative to the start distance.
            if (this.startTouchDistance > 0) {
                const scale = currentDistance / this.startTouchDistance;
                const newZoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, this.scaleStartZoom * scale));

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

    // Safari gesture handlers
    public onGestureStart(event: GestureEvent) {
        this.scaleStartZoom = this.zoom;
        this.dragStartOffsetX = this.offsetX;
        this.dragStartOffsetY = this.offsetY;
    }

    public onGestureChange(event: GestureEvent, canvas: HTMLElement,
                           playgroundState: PlaygroundState) {
        // event.scale is already an absolute ratio relative to gesturestart.
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

const playgroundState = new PlaygroundState();
const gestureManager = new GestureManager();
const loadingProgress = new LoadingProgress();
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
    // Initialize PAGX module
    const module = await PAGXInit({
        locateFile: (file: string) => './wasm-mt/' + file,
        wasmBinary: wasmBuffer,
    });
    playgroundState.module = module;
    const pagxView = module.PAGXView.init('#pagx-canvas');
    if (!pagxView) {
        throw new Error('Failed to create PAGXView');
    }
    playgroundState.pagxView = pagxView;
    pagxView.setBackgroundColor('#000000', 0);
    updateSize();
    pagxView.updateZoomScaleAndOffset(1.0, 0, 0);
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    bindCanvasEvents(canvas);
    pagxView.start();
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

function handleVisibilityChange() {
    if (document.hidden) {
        playgroundState.pagxView?.stop();
    } else {
        playgroundState.pagxView?.start();
    }
}

function setupVisibilityListeners() {
    document.addEventListener('visibilitychange', handleVisibilityChange);
    window.addEventListener('beforeunload', () => {
        playgroundState.pagxView?.stop();
    });
}

function bindCanvasEvents(canvas: HTMLElement) {
    // Set initial cursor style
    canvas.style.cursor = 'grab';

    // A press whose pointer barely moves is treated as a click that toggles playback,
    // while a larger movement is a pan gesture handled by the gesture manager.
    const CLICK_MOVE_THRESHOLD = 5;
    let pressStartX = 0;
    let pressStartY = 0;

    // Wheel events for scroll and zoom
    canvas.addEventListener('wheel', (e: WheelEvent) => {
        e.preventDefault();
        gestureManager.onWheel(e, canvas, playgroundState);
    }, { passive: false });

    // Mouse drag events
    canvas.addEventListener('mousedown', (e: MouseEvent) => {
        e.preventDefault();
        pressStartX = e.clientX;
        pressStartY = e.clientY;
        gestureManager.onMouseDown(e, canvas);
    });
    canvas.addEventListener('mousemove', (e: MouseEvent) => {
        gestureManager.onMouseMove(e, playgroundState);
    });
    canvas.addEventListener('mouseup', (e: MouseEvent) => {
        gestureManager.onMouseUp(canvas);
        if (e.button === 0 &&
            Math.abs(e.clientX - pressStartX) < CLICK_MOVE_THRESHOLD &&
            Math.abs(e.clientY - pressStartY) < CLICK_MOVE_THRESHOLD) {
            togglePlayback();
        }
    });
    canvas.addEventListener('mouseleave', () => {
        gestureManager.onMouseUp(canvas);
    });

    // Touch events for mobile
    canvas.addEventListener('touchstart', (e: TouchEvent) => {
        e.preventDefault();
        if (e.touches.length === 1) {
            pressStartX = e.touches[0].clientX;
            pressStartY = e.touches[0].clientY;
        }
        gestureManager.onTouchStart(e, canvas);
    }, { passive: false });
    canvas.addEventListener('touchmove', (e: TouchEvent) => {
        e.preventDefault();
        gestureManager.onTouchMove(e, canvas, playgroundState);
    }, { passive: false });
    canvas.addEventListener('touchend', (e: TouchEvent) => {
        const wasSingleTouch = e.touches.length === 0 && e.changedTouches.length === 1;
        gestureManager.onTouchEnd(e, canvas);
        if (wasSingleTouch) {
            const touch = e.changedTouches[0];
            if (Math.abs(touch.clientX - pressStartX) < CLICK_MOVE_THRESHOLD &&
                Math.abs(touch.clientY - pressStartY) < CLICK_MOVE_THRESHOLD) {
                togglePlayback();
            }
        }
    });
    canvas.addEventListener('touchcancel', (e: TouchEvent) => {
        gestureManager.onTouchEnd(e, canvas);
    });

    // Safari gesture events for pinch-to-zoom
    canvas.addEventListener('gesturestart', (e: Event) => {
        e.preventDefault();
        gestureManager.onGestureStart(e as GestureEvent);
    });
    canvas.addEventListener('gesturechange', (e: Event) => {
        e.preventDefault();
        gestureManager.onGestureChange(e as GestureEvent, canvas, playgroundState);
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

function showErrorUI(message: string): void {
    setDropZoneState('error', message);
}

function hideDropZone(): void {
    const dropZone = document.getElementById('drop-zone');
    if (dropZone) {
        dropZone.classList.add('hidden');
    }
}

function showPlaybackBar(): void {
    const playbackBar = document.getElementById('playback-bar') as HTMLDivElement;
    if (!playbackBar) {
        return;
    }
    // Static PAGX has no animation (zero duration); hide the playback bar for it.
    const hasAnimation = !!playgroundState.pagxView && playgroundState.pagxView.durationMicros() > 0;
    if (hasAnimation) {
        playbackBar.classList.remove('hidden');
        updatePlaybackUI();
        updateLoopIcon();
    } else {
        playbackBar.classList.add('hidden');
    }
}

function formatTime(microseconds: number): string {
    const seconds = Math.max(0, microseconds / 1_000_000);
    return seconds.toFixed(2) + 's';
}

function getCurrentFrame(): number {
    const view = playgroundState.pagxView;
    if (!view) return 0;
    const rate = view.frameRate();
    if (rate <= 0) return 0;
    return Math.round(Math.max(0, view.currentTimeMicros()) * rate / 1_000_000);
}

function getTotalFrames(): number {
    const view = playgroundState.pagxView;
    if (!view) return 0;
    const rate = view.frameRate();
    if (rate <= 0) return 0;
    return Math.ceil(view.durationMicros() * rate / 1_000_000);
}

function updatePlayPauseIcon(): void {
    const playPauseImg = document.getElementById('play-pause-img') as HTMLImageElement;
    if (!playgroundState.pagxView || !playPauseImg) {
        return;
    }
    const playing = playgroundState.pagxView.isPlaying();
    playPauseImg.src = playing ? './pause.png' : './play.png';
}

function updateSliderFill(slider: HTMLInputElement): void {
    const pct = (parseFloat(slider.value) / 1000) * 100;
    slider.style.background =
        'linear-gradient(90deg, #4c7cf3, #8b5cf0, #ec5b9c) no-repeat 0 0 / ' +
        pct + '% 100%, rgba(255, 255, 255, 0.18)';
}

function updateProgressSlider(): void {
    const slider = document.getElementById('progress-slider') as HTMLInputElement;
    if (!playgroundState.pagxView || !slider) {
        return;
    }
    const duration = playgroundState.pagxView.durationMicros();
    if (duration <= 0) {
        slider.value = '0';
        updateSliderFill(slider);
        return;
    }
    const current = Math.max(0, playgroundState.pagxView.currentTimeMicros());
    slider.value = String(Math.round((current / duration) * 1000));
    updateSliderFill(slider);
}

function updateTimeDisplay(): void {
    const timeText = document.getElementById('time-text');
    const frameText = document.getElementById('frame-text');
    if (!playgroundState.pagxView || !timeText || !frameText) {
        return;
    }
    const current = playgroundState.pagxView.currentTimeMicros();
    const duration = playgroundState.pagxView.durationMicros();
    timeText.textContent = formatTime(current) + ' / ' + formatTime(duration);
    frameText.textContent = String(getCurrentFrame()) + ' / ' + String(getTotalFrames());
}

function updateLoopIcon(): void {
    const loopBtn = document.getElementById('loop-btn');
    if (!playgroundState.pagxView || !loopBtn) {
        return;
    }
    const looping = playgroundState.pagxView.isLoop();
    loopBtn.classList.toggle('active', looping);
}

function updatePlaybackUI(): void {
    updatePlayPauseIcon();
    updateProgressSlider();
    updateTimeDisplay();
    // updateLoopIcon is intentionally NOT called here: the loop state only changes on user
    // click, so refreshing it on the 100ms playback tick would be wasted DOM work. Call sites
    // that toggle the loop state (loop button handler, initial showPlaybackBar) update it
    // explicitly.
}

function togglePlayback(): void {
    const view = playgroundState.pagxView;
    if (!view || view.durationMicros() <= 0) {
        return;
    }
    if (view.isPlaying()) {
        view.pause();
    } else {
        // Restart from the head when starting playback at the last frame (the user stepped or
        // dragged the playhead to the end), so play replays instead of being stuck at the end.
        if (view.currentTimeMicros() >= view.durationMicros()) {
            view.setCurrentTimeMicros(0);
        }
        view.play();
    }
    updatePlaybackUI();
}

// Steps the playhead by one frame in the given direction (-1 previous, +1 next). Stepping always
// pauses first, then seeks by a single frame duration clamped to the animation range. Composed from
// the view's playback primitives so the frame-navigation policy lives in the UI layer.
function stepFrame(direction: number): void {
    const view = playgroundState.pagxView;
    if (!view) {
        return;
    }
    const rate = view.frameRate();
    const duration = view.durationMicros();
    if (rate <= 0 || duration <= 0) {
        return;
    }
    view.pause();
    const frameDurationUs = 1_000_000 / rate;
    const target = view.currentTimeMicros() + direction * frameDurationUs;
    view.setCurrentTimeMicros(Math.max(0, Math.min(duration, target)));
    updatePlaybackUI();
}

function hidePlaybackUI(): void {
    document.getElementById('pagx-canvas')?.classList.add('hidden');
    document.getElementById('toolbar')?.classList.add('hidden');
    document.getElementById('playback-bar')?.classList.add('hidden');
}

const DEFAULT_TITLE = 'PAGX Playground';

function goHome(pushHistory: boolean = true): void {
    if (playgroundState.pagxView) {
        playgroundState.pagxView.clear();
        gestureManager.resetTransform(playgroundState);
    }
    document.getElementById('pagx-canvas')?.classList.add('hidden');
    document.getElementById('toolbar')?.classList.add('hidden');
    document.getElementById('playback-bar')?.classList.add('hidden');
    document.getElementById('nav-btns')?.classList.remove('hidden');
    document.title = DEFAULT_TITLE;
    showDropZoneUI();
    currentPlayingFile = null;

    // Clear sample parameter from URL
    if (pushHistory) {
        history.pushState(null, '', window.location.pathname);
    }

    // Notify the Source Editor module that the document has been cleared.
    window.dispatchEvent(new CustomEvent('pagx:loaded', { detail: { xmlText: null } }));
}

function isSafeRelativePath(path: string): boolean {
    // Reject protocol schemes, absolute paths, and path traversal.
    return !path.includes('://') && !path.startsWith('/') && !path.includes('..');
}

async function loadExternalFiles(baseURL: string): Promise<void> {
    if (!playgroundState.pagxView) {
        return;
    }
    const paths = playgroundState.pagxView.getExternalFilePaths();
    if (paths.length === 0) {
        return;
    }
    const fetches: Promise<void>[] = [];
    for (const filePath of paths) {
        if (!isSafeRelativePath(filePath)) {
            console.warn(`Skipping unsafe external file path: ${filePath}`);
            continue;
        }
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
    await Promise.all(fetches);
}

// Shared post-reparse UI refresh. Any call site that runs parsePAGX + buildLayers must invoke
// this so the canvas backing store, playback bar visibility and playback UI values stay in sync
// with the newly parsed document; forgetting it leaves the UI stuck on the previous file's
// duration/dimensions.
function refreshUIAfterReparse(): void {
    updateSize();
    // showPlaybackBar handles the static-image case (duration === 0 → hide) and refreshes the
    // slider / time / loop icon internally when the bar becomes visible.
    showPlaybackBar();
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
    // The view is a reused singleton, so reset loop to its default enabled state so each newly
    // opened file starts fresh instead of inheriting the previous file's loop toggle.
    playgroundState.pagxView.setLoop(true);
    gestureManager.resetTransform(playgroundState);
    refreshUIAfterReparse();
    // Draw the first frame before showing canvas to avoid flashing old content
    playgroundState.pagxView.draw();
    hideDropZone();
    canvas.classList.remove('hidden');
    toolbar.classList.remove('hidden');
    navBtns.classList.add('hidden');
    document.title = 'PAGX Playground - ' + name;
    currentFileName = name;

    // Notify the Source Editor module that a new XML document is available.
    const decoder = new TextDecoder('utf-8');
    const xmlText = decoder.decode(data);
    window.dispatchEvent(new CustomEvent('pagx:loaded', { detail: { xmlText } }));
}

const LOADING_TIMEOUT_MS = 60000;

function withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
    let timer: number | undefined;
    const timeout = new Promise<never>((_, reject) => {
        timer = window.setTimeout(
            () => reject(new Error('Loading timed out. Please check your network and try again.')),
            ms
        );
    });
    return Promise.race([promise, timeout]).finally(() => {
        if (timer !== undefined) {
            window.clearTimeout(timer);
        }
    }) as Promise<T>;
}

async function prepareForLoading(): Promise<void> {
    if (playgroundState.pagxView) {
        hidePlaybackUI();
    }
    showLoadingUI();
    resetProgressUI();
    await new Promise(resolve => requestAnimationFrame(resolve));

    if (!wasmLoadPromise) {
        wasmLoadPromise = loadWasm();
    }
    if (!fontLoadPromise) {
        fontLoadPromise = loadFonts();
    }

    const loadingStartTime = Date.now();
    try {
        await withTimeout(
            Promise.all([wasmLoadPromise, fontLoadPromise]),
            LOADING_TIMEOUT_MS
        );
    } catch (error) {
        // Reset promises so the next attempt can retry from scratch.
        wasmLoadPromise = null;
        fontLoadPromise = null;
        throw error;
    }
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
        currentPlayingFile = null;

        history.replaceState(null, '', window.location.pathname);
    } catch (error) {
        console.error('Failed to load PAGX file:', error);
        const message = error instanceof Error ? error.message : t().errorFormat;
        showErrorUI(message);
    }
}

const SAMPLES_DIR = './samples/';

function isValidSampleName(name: string): boolean {
    // Only allow filenames like "foo.pagx" or "foo-bar_baz.pagx".
    // Reject path separators, protocol schemes, and other suspicious patterns.
    return /^[\w][\w.\-]*\.pagx$/.test(name);
}

async function loadPAGXSample(name: string, pushHistory: boolean = true) {
    if (!isValidSampleName(name)) {
        showErrorUI(t().errorFormat);
        return;
    }
    try {
        await prepareForLoading();

        const url = SAMPLES_DIR + name;
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`Failed to fetch: ${response.status} ${response.statusText}`);
        }
        const fileBuffer = await response.arrayBuffer();

        await loadPAGXData(new Uint8Array(fileBuffer), name, SAMPLES_DIR);
        currentPlayingFile = name;

        const cleanUrl = window.location.pathname + '?sample=' + encodeURIComponent(name);
        if (pushHistory) {
            history.pushState(null, '', cleanUrl);
        }
    } catch (error) {
        console.error('Failed to load PAGX sample:', error);
        const message = error instanceof Error ? error.message : 'Unknown error';
        showErrorUI(message);
    }
}

function getSampleNameFromParams(): string | null {
    const params = new URLSearchParams(window.location.search);
    return params.get('sample');
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

    const isFileDrag = (e: DragEvent): boolean => {
        const types = e.dataTransfer?.types;
        return types ? Array.prototype.indexOf.call(types, 'Files') !== -1 : false;
    };

    ['dragenter', 'dragover'].forEach(eventName => {
        container.addEventListener(eventName, (e: Event) => {
            if (!isFileDrag(e as DragEvent)) {
                return;
            }
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

function setupPlaybackControls(): void {
    const playPauseBtn = document.getElementById('play-pause-btn') as HTMLButtonElement;
    const prevFrameBtn = document.getElementById('prev-frame-btn') as HTMLButtonElement;
    const nextFrameBtn = document.getElementById('next-frame-btn') as HTMLButtonElement;
    const progressSlider = document.getElementById('progress-slider') as HTMLInputElement;

    if (playPauseBtn) {
        playPauseBtn.addEventListener('click', () => {
            togglePlayback();
        });
    }

    if (prevFrameBtn) {
        prevFrameBtn.addEventListener('click', () => {
            stepFrame(-1);
        });
    }

    if (nextFrameBtn) {
        nextFrameBtn.addEventListener('click', () => {
            stepFrame(1);
        });
    }

    const loopBtn = document.getElementById('loop-btn') as HTMLButtonElement;
    if (loopBtn) {
        loopBtn.addEventListener('click', () => {
            if (!playgroundState.pagxView) return;
            playgroundState.pagxView.setLoop(!playgroundState.pagxView.isLoop());
            updateLoopIcon();
            // Release focus so the Space shortcut keeps working after clicking.
            loopBtn.blur();
        });
    }

    let isDraggingSlider = false;
    let wasPlayingBeforeDrag = false;
    if (progressSlider) {
        const seekToSliderValue = () => {
            if (!playgroundState.pagxView) return;
            const duration = playgroundState.pagxView.durationMicros();
            if (duration <= 0) return;
            const value = parseFloat(progressSlider.value);
            const targetTime = (value / 1000) * duration;
            playgroundState.pagxView.setCurrentTimeMicros(targetTime);
        };
        progressSlider.addEventListener('input', () => {
            if (!playgroundState.pagxView) return;
            // Pause on drag start so the frame follows the slider instead of the render loop
            // advancing over it; remember the state to restore on release.
            if (!isDraggingSlider) {
                isDraggingSlider = true;
                wasPlayingBeforeDrag = playgroundState.pagxView.isPlaying();
                if (wasPlayingBeforeDrag) {
                    playgroundState.pagxView.pause();
                }
            }
            updateSliderFill(progressSlider);
            seekToSliderValue();
            updateTimeDisplay();
            updatePlayPauseIcon();
        });
        progressSlider.addEventListener('change', () => {
            isDraggingSlider = false;
            if (!playgroundState.pagxView) return;
            seekToSliderValue();
            if (wasPlayingBeforeDrag) {
                playgroundState.pagxView.play();
                wasPlayingBeforeDrag = false;
            }
            updatePlaybackUI();
            // Release focus so the Space shortcut keeps working after dragging.
            progressSlider.blur();
        });
    }

    // Keyboard shortcuts: Space toggles play/pause, ArrowLeft/ArrowRight step one frame.
    // Guards are shared across shortcuts:
    //   - text-entry targets (input/textarea/contentEditable) never trigger playback shortcuts;
    //   - when the progress slider itself holds focus, ArrowLeft/Right must fall through to the
    //     native range control (fine-grained scrub) rather than jumping a whole frame; Space
    //     however still toggles playback so the user can pause without leaving the slider;
    //   - no file loaded (canvas hidden) → do nothing so the shortcut can't be misused before
    //     the drop-zone is dismissed.
    document.addEventListener('keydown', (e: KeyboardEvent) => {
        const isPlayPause = e.code === 'Space';
        const stepDirection = e.code === 'ArrowLeft' ? -1 : e.code === 'ArrowRight' ? 1 : 0;
        if (!isPlayPause && stepDirection === 0) return;
        const target = e.target;
        const isTextInput =
            (target instanceof HTMLInputElement && target.type !== 'range') ||
            target instanceof HTMLTextAreaElement ||
            (target instanceof HTMLElement && target.isContentEditable);
        if (isTextInput) return;
        // Range slider owns Arrow keys for scrub; only Space passes through to play/pause.
        const isRangeSlider = target instanceof HTMLInputElement && target.type === 'range';
        if (isRangeSlider && !isPlayPause) return;
        const canvas = document.getElementById('pagx-canvas');
        if (!canvas || canvas.classList.contains('hidden')) return;
        e.preventDefault();
        if (isPlayPause) {
            togglePlayback();
        } else {
            stepFrame(stepDirection);
        }
    });

    // Update playback UI periodically. Also fire once on the play -> stop transition so the slider,
    // time and icon settle on the final frame after a single (non-looping) playback ends.
    let wasPlaying = false;
    setInterval(() => {
        if (!playgroundState.pagxView) return;
        const canvas = document.getElementById('pagx-canvas');
        if (!canvas || canvas.classList.contains('hidden')) return;
        const playing = playgroundState.pagxView.isPlaying();
        if (!isDraggingSlider && (playing || wasPlaying)) {
            updatePlaybackUI();
        }
        wasPlaying = playing;
    }, 100);
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
• Chrome 69+
• Firefox 79+
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

    document.querySelectorAll<HTMLElement>('.nav-dropdown-toggle').forEach((toggle) => {
        toggle.title = strings.docs;
        const span = toggle.querySelector('span');
        if (span) span.textContent = strings.docs;
    });
    document.querySelectorAll('.nav-dropdown-spec').forEach((item) => {
        const span = item.querySelector('span');
        if (span) span.textContent = strings.specification;
    });
    document.querySelectorAll('.nav-dropdown-skills').forEach((item) => {
        const span = item.querySelector('span');
        if (span) span.textContent = strings.aiSkills;
    });
}

let sampleFiles: string[] = [];
let currentPlayingFile: string | null = null;
let currentFileName: string = 'export.pagx';

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
        a.addEventListener('click', (e) => {
            e.preventDefault();
            // Clear hash before loading so replaceState won't carry #samples
            history.replaceState(null, '', window.location.pathname + window.location.search);
            hideSamplesPage();
            loadPAGXSample(file);
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

function handlePopState(): void {
    const sampleName = getSampleNameFromParams();
    if (sampleName) {
        if (sampleName !== currentPlayingFile) {
            loadPAGXSample(sampleName, false);
        }
    } else {
        goHome(false);
    }
    handleRoute();
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
        window.addEventListener('popstate', handlePopState);
        handleRoute();

        // Setup samples back button
        const samplesBackBtn = document.getElementById('samples-back-btn');
        if (samplesBackBtn) {
            samplesBackBtn.addEventListener('click', (e) => {
                e.preventDefault();
                history.replaceState(null, '', window.location.pathname + window.location.search);
                hideSamplesPage();
            });
        }

        // Setup drag and drop early so UI is responsive
        setupDragAndDrop();
        setupPlaybackControls();

        if (!checkWasmSupport() || !checkWebGL2Support()) {
            showErrorUI(getBrowserRequirements());
            const errorContent = document.getElementById('error-content');
            if (errorContent) {
                errorContent.style.cursor = 'default';
                errorContent.style.pointerEvents = 'none';
            }
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
        const sampleName = getSampleNameFromParams();
        if (sampleName) {
            loadPAGXSample(sampleName, false);
        }

        // Encodes XML, reparses and redraws the view. Shared by the editor's Apply
        // and Save actions. Returns '' on success, otherwise an error message.
        //
        // Runs the same UI refresh as loadPAGXData's post-parse step so that an edit which
        // changes the canvas dimensions (width/height attrs) or the animation duration (static
        // ↔ animated) is fully reflected: canvas backing store is resized, the playback bar
        // shows/hides, and the time/frame display picks up the new duration instead of showing
        // the previous file's values.
        const applyXmlToView = (xmlText: string): string => {
            if (playgroundState.pagxView === null) {
                return 'PAGXView not initialized';
            }
            try {
                const data = new TextEncoder().encode(xmlText);
                playgroundState.pagxView.parsePAGX(data);
                playgroundState.pagxView.buildLayers();
                refreshUIAfterReparse();
                playgroundState.pagxView.draw();
                return '';
            } catch (e) {
                return e instanceof Error ? e.message : String(e);
            }
        };
        // Initialize the Source Editor module (keyboard shortcut L to toggle).
        initEditor({
            onApply: applyXmlToView,
            onSave: (xmlText: string): string => {
                const error = applyXmlToView(xmlText);
                if (error !== '') {
                    return error;
                }
                const blob = new Blob([xmlText], { type: 'application/xml' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = currentFileName.endsWith('.pagx') ? currentFileName : currentFileName + '.pagx';
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                return '';
            },
        });

        const sourceEditorBtn = document.getElementById('source-editor-btn');
        if (sourceEditorBtn) {
            sourceEditorBtn.addEventListener('click', () => {
                toggleEditorPanel();
            });
        }
    };

    // Observe container resize. The C++ PAGXView::draw() now auto-detects
    // canvas drawing-buffer size changes and rebuilds its render surface in
    // the same frame, so we can sync canvas.width/height and trigger a draw
    // synchronously in the callback. This keeps resize + new frame within a
    // single browser paint tick, eliminating the flicker that the old 300ms
    // setTimeout debounce was trying to mask.
    //
    // rAF throttle: ResizeObserver may fire multiple times per frame during a
    // fast drag. Coalesce into one updateSize() + draw() per frame to cap GL
    // surface rebuild cost on low-end devices.
    const container = document.getElementById('container');
    if (container) {
        let pendingResizeFrame: number | null = null;
        const resizeObserver = new ResizeObserver(() => {
            if (!playgroundState.pagxView || pendingResizeFrame !== null) {
                return;
            }
            pendingResizeFrame = window.requestAnimationFrame(() => {
                pendingResizeFrame = null;
                if (!playgroundState.pagxView) {
                    return;
                }
                updateSize();
                playgroundState.pagxView.draw();
            });
        });
        resizeObserver.observe(container);
    }
}
