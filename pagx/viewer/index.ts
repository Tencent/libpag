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
import PAGXWasm from './wasm-mt/pagx-viewer';
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
const WASM_URL = './wasm-mt/pagx-viewer.wasm';

// Estimated sizes for progress calculation (in bytes)
const ESTIMATED_WASM_SIZE = 2400000;
const ESTIMATED_FONT_SIZE = 8800000;
const ESTIMATED_EMOJI_FONT_SIZE = 10300000;

class ViewerState {
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

class ViewerError extends Error {
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

    public zoom = 1.0;
    public offsetX = 0;
    public offsetY = 0;

    private handleScrollEvent(
        event: WheelEvent,
        state: ScrollGestureState,
        viewerState: ViewerState,
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
            viewerState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
        }
    }

    private handleScaleEvent(
        event: WheelEvent,
        state: ScaleGestureState,
        canvas: HTMLElement,
        viewerState: ViewerState,
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
        viewerState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    public clearState() {
        this.scaleY = 1.0;
        this.timer = undefined;
    }

    public resetTransform(viewerState: ViewerState) {
        this.zoom = 1.0;
        this.offsetX = 0;
        this.offsetY = 0;
        this.clearState();
        viewerState.pagxView?.updateZoomScaleAndOffset(this.zoom, this.offsetX, this.offsetY);
    }

    private resetScrollTimeout(
        event: WheelEvent,
        viewerState: ViewerState,
    ) {
        clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            this.handleScrollEvent(event, ScrollGestureState.SCROLL_END, viewerState);
            this.clearState();
        }, this.pinchTimeout);
    }

    private resetScaleTimeout(
        event: WheelEvent,
        canvas: HTMLElement,
        viewerState: ViewerState,
    ) {
        clearTimeout(this.timer);
        this.timer = window.setTimeout(() => {
            this.timer = undefined;
            this.handleScaleEvent(event, ScaleGestureState.SCALE_END, canvas, viewerState);
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

    public onWheel(event: WheelEvent, canvas: HTMLElement, viewerState: ViewerState) {
        const deviceType = this.getDeviceType(event);
        let wheelRatio = (deviceType === DeviceType.MOUSE ? this.mouseWheelRatio : this.touchWheelRatio);
        if (!event.deltaY || (!event.ctrlKey && !event.metaKey)) {
            this.resetScrollTimeout(event, viewerState);
            this.handleScrollEvent(event, ScrollGestureState.SCROLL_CHANGE, viewerState);
        } else {
            this.scaleY *= Math.exp(-(event.deltaY) / wheelRatio);
            if (!this.timer) {
                this.resetScaleTimeout(event, canvas, viewerState);
                this.handleScaleEvent(event, ScaleGestureState.SCALE_START, canvas, viewerState);
            } else {
                this.resetScaleTimeout(event, canvas, viewerState);
                this.handleScaleEvent(event, ScaleGestureState.SCALE_CHANGE, canvas, viewerState);
            }
        }
    }
}

const viewerState = new ViewerState();
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
    viewerState.fontData = fontData;
    viewerState.emojiFontData = emojiFontData;
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
        mainScriptUrlOrBlob: './wasm-mt/pagx-viewer.js',
        wasmBinary: wasmBuffer,
    });
    viewerState.module = module as PAGXModule;
    TGFXBind(viewerState.module as any);
    const pagxView = viewerState.module.PAGXView.MakeFrom('#pagx-canvas');
    if (!pagxView) {
        throw new Error('Failed to create PAGXView');
    }
    viewerState.pagxView = pagxView;
    updateSize();
    viewerState.pagxView.updateZoomScaleAndOffset(1.0, 0, 0);
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    bindCanvasEvents(canvas);
    animationLoop();
    setupVisibilityListeners();
}

function registerFontsToView(): void {
    if (!viewerState.pagxView) {
        return;
    }
    const fontData = viewerState.fontData || new Uint8Array(0);
    const emojiFontData = viewerState.emojiFontData || new Uint8Array(0);
    viewerState.pagxView.registerFonts(fontData, emojiFontData);
}

function updateSize() {
    if (!viewerState.pagxView) {
        return;
    }
    viewerState.resized = false;
    const canvas = document.getElementById('pagx-canvas') as HTMLCanvasElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const screenRect = container.getBoundingClientRect();
    const scaleFactor = window.devicePixelRatio;
    canvas.width = screenRect.width * scaleFactor;
    canvas.height = screenRect.height * scaleFactor;
    canvas.style.width = screenRect.width + "px";
    canvas.style.height = screenRect.height + "px";
    viewerState.pagxView.updateSize();
}

function draw() {
    viewerState.pagxView?.draw();
}

function animationLoop() {
    if (animationLoopRunning) {
        return;
    }
    animationLoopRunning = true;
    const frame = () => {
        if (!viewerState.pagxView || !viewerState.isPageVisible) {
            animationLoopRunning = false;
            viewerState.animationFrameId = null;
            return;
        }
        draw();
        viewerState.animationFrameId = requestAnimationFrame(frame);
    };
    viewerState.animationFrameId = requestAnimationFrame(frame);
}

function handleVisibilityChange() {
    viewerState.isPageVisible = !document.hidden;
    if (viewerState.isPageVisible && viewerState.animationFrameId === null) {
        animationLoop();
    }
}

function setupVisibilityListeners() {
    document.addEventListener('visibilitychange', handleVisibilityChange);
    window.addEventListener('beforeunload', () => {
        if (viewerState.animationFrameId !== null) {
            cancelAnimationFrame(viewerState.animationFrameId);
            viewerState.animationFrameId = null;
        }
    });
}

function bindCanvasEvents(canvas: HTMLElement) {
    canvas.addEventListener('wheel', (e: WheelEvent) => {
        e.preventDefault();
        gestureManager.onWheel(e, canvas, viewerState);
    }, { passive: false });

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

async function loadPAGXData(data: Uint8Array, name: string) {
    const fileName = document.getElementById('file-name') as HTMLSpanElement;
    const specBtn = document.getElementById('spec-btn') as HTMLAnchorElement;
    const toolbar = document.getElementById('toolbar') as HTMLDivElement;

    if (!viewerState.pagxView) {
        throw new Error('PAGXView not initialized');
    }

    registerFontsToView();
    viewerState.pagxView.loadPAGX(data);
    gestureManager.resetTransform(viewerState);
    updateSize();
    hideDropZone();
    toolbar.classList.remove('hidden');
    specBtn.classList.add('hidden');
    fileName.textContent = name;
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
        await loadPAGXData(new Uint8Array(fileBuffer), file.name);
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

        // Extract filename from URL
        const urlPath = new URL(url).pathname;
        const name = urlPath.substring(urlPath.lastIndexOf('/') + 1) || 'remote.pagx';

        // Register fonts and load PAGX file
        await loadPAGXData(new Uint8Array(fileBuffer), name);
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

    openBtn.addEventListener('click', () => {
        fileInput.click();
    });

    resetBtn.addEventListener('click', () => {
        gestureManager.resetTransform(viewerState);
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

    if (dropText) dropText.textContent = strings.dropText;
    if (dropSubtext) dropSubtext.textContent = strings.dropSubtext;
    if (loadingText) loadingText.textContent = strings.loading;
    if (errorTitle) errorTitle.textContent = strings.errorTitle;
    if (openBtn) openBtn.title = strings.openFile;
    if (resetBtn) resetBtn.title = strings.resetView;

    const specBtn = document.getElementById('spec-btn');
    const specBtnText = document.getElementById('spec-btn-text');
    const toolbarSpecBtn = document.getElementById('toolbar-spec-btn');
    if (specBtn) specBtn.title = strings.specTitle;
    if (specBtnText) specBtnText.textContent = strings.spec;
    if (toolbarSpecBtn) toolbarSpecBtn.title = strings.specTitle;
}

if (typeof window !== 'undefined') {
    window.onload = async () => {
        // Apply i18n texts
        applyI18n();

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
        if (!viewerState.pagxView || viewerState.resized) {
            return;
        }
        viewerState.resized = true;
        window.setTimeout(() => {
            updateSize();
        }, 300);
    };
}
