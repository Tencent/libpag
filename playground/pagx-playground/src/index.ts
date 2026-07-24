///////////////////////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////////////////////

// Resolved to the selected arch's pagx-viewer glue at build time via the
// `pagx-viewer-glue` alias in scripts/rollup.js (single-threaded adds a `.st` infix).
import { PAGXInit } from 'pagx-viewer-glue';
import type { PAGXView, PAGXModule } from '../../pagx-viewer/src/ts/pagx';
// Aliased in scripts/rollup.js to the pre-built ESM bundle inside wasm-mt/. pagx-player owns
// the canvas / toolbar / playback bar / gestures / source editor, so the playground is left as
// a thin shell that provides resource loading, i18n, drop zone UI, samples page and routing.
import { PAGXPlayer, type ToolbarSlot } from 'pagx-player';

interface I18nStrings {
    dropText: string;
    dropSubtext: string;
    loading: string;
    errorTitle: string;
    errorFormat: string;
    errorBrowser: string;
    openFile: string;
    invalidFile: string;
    docs: string;
    specification: string;
    aiSkills: string;
    leave: string;
    samples: string;
    samplesTitle: string;
    back: string;
}

const i18n: Record<string, I18nStrings> = {
    en: {
        dropText: 'Drag & Drop PAGX file here',
        dropSubtext: 'or click to browse',
        loading: 'Loading...',
        errorTitle: 'Failed to load',
        errorFormat: 'Invalid file format. Please use a valid .pagx file.',
        errorBrowser: 'Minimum browser versions required:',
        openFile: 'Open PAGX File',
        invalidFile: 'Please drop a .pagx file',
        docs: 'Docs',
        specification: 'Specification',
        aiSkills: 'AI Skills',
        leave: 'Leave',
        samples: 'Samples',
        samplesTitle: 'PAGX Samples',
        back: 'Back',
    },
    zh: {
        dropText: '拖放 PAGX 文件到此处',
        dropSubtext: '或点击选择文件',
        loading: '加载中...',
        errorTitle: '加载失败',
        errorFormat: '无效的文件格式，请使用有效的 .pagx 文件。',
        errorBrowser: '浏览器最低版本要求：',
        openFile: '打开 PAGX 文件',
        invalidFile: '请拖放 .pagx 文件',
        docs: '文档',
        specification: '格式规范',
        aiSkills: 'AI Skills',
        leave: '离开',
        samples: '示例',
        samplesTitle: 'PAGX 示例',
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

// Base URL used to fetch every remote asset the playground consumes (wasm, fonts, .pagx
// samples, sample thumbnails, player icons). Local development (localhost / 127.0.0.1) keeps
// relative paths so `npm run server` continues serving out of the working tree, while any
// other host is redirected to the PAG CDN. The CDN version substantially reduces first-load
// time for the official pagx site because the ~21 MB of fonts + wasm come from a
// geographically-close edge instead of the origin server. Ends without a trailing slash;
// concatenation sites (assetUrl below) prepend '/' explicitly.
//
// Uses `globalThis.location` rather than `window.location` because the pagx-viewer
// multi-threaded build starts Emscripten pthread workers whose script is this same rollup
// bundle. Worker context has `self.location` but no `window`, so probing `window.location`
// at module-init time (below) would throw ReferenceError on every worker spawn. `location`
// is present on both Window and DedicatedWorkerGlobalScope, so this call reads the correct
// hostname in either environment.
function computeResourceBase(): string {
    const host = globalThis.location?.hostname;
    if (host === 'localhost' || host === '127.0.0.1') {
        return '';
    }
    return 'https://pag.qq.com/pagx';
}

const RESOURCE_BASE = computeResourceBase();

// Build an absolute (CDN) or root-relative asset URL. relPath must be a path relative to the
// site root, e.g. 'wasm-mt/pagx-viewer.wasm' or 'samples/index.json'. Do NOT pass a leading
// '/'; passing one on the CDN branch would produce an invalid double-slash URL.
function assetUrl(relPath: string): string {
    return RESOURCE_BASE === '' ? `./${relPath}` : `${RESOURCE_BASE}/${relPath}`;
}

// Font URLs for preloading. Fonts and wasm live under the same CDN prefix in production so
// the browser can multiplex both downloads over a single connection.
const FONT_URL = assetUrl('fonts/NotoSansSC-Regular.otf');
const EMOJI_FONT_URL = assetUrl('fonts/NotoColorEmoji.ttf');

// __PAGX_INFIX__ is injected at build time by scripts/rollup.js: empty for the
// multi-threaded build, ".st" for the single-threaded build. The matching wasm is
// placed at wasm-mt/ by scripts/prebuild.js.
declare const __PAGX_INFIX__: string;
const WASM_URL = assetUrl(`wasm-mt/pagx-viewer${__PAGX_INFIX__}.wasm`);

// Estimated sizes for progress calculation (in bytes)
const ESTIMATED_WASM_SIZE = 2400000;
const ESTIMATED_FONT_SIZE = 8800000;
const ESTIMATED_EMOJI_FONT_SIZE = 10300000;

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

const loadingProgress = new LoadingProgress();
let fontData: Uint8Array | null = null;
let emojiFontData: Uint8Array | null = null;
let cachedModule: PAGXModule | null = null;
let wasmLoadPromise: Promise<PAGXModule> | null = null;
let fontLoadPromise: Promise<void> | null = null;
let player: PAGXPlayer | null = null;
let sampleFiles: string[] = [];
let currentPlayingFile: string | null = null;
let currentFileName: string = 'export.pagx';
// Snapshot of the player's playing state at the moment the samples overlay is opened. Used
// by hideSamplesPage() to restore the exact play/pause state on return so the samples page
// visit is transparent to the user - a running animation resumes, a paused one stays paused.
let wasPlayingBeforeSamples: boolean = false;
const DEFAULT_TITLE = 'PAGX Playground';

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
            // Silent degradation: if the CDN or local font is unreachable we log a warning
            // and return null instead of failing loadPAGXData. registerFontsToView then feeds
            // an empty Uint8Array into the view, and pagx-viewer falls back to system fonts
            // for text layers. Users see a visibly less accurate render rather than a hard
            // failure that would strand them on the error UI.
            console.warn(`Error loading font ${url}:`, error);
            onDone();
            updateProgressUI();
            return null;
        }
    };

    const [font, emojiFont] = await Promise.all([
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
    fontData = font;
    emojiFontData = emojiFont;
}

// Fetches the wasm binary with progress reporting, then instantiates PAGXModule. The result
// is cached: the player's `moduleFactory` hits the cache after the first load(). Errors
// clear wasmLoadPromise upstream so a retry can start over cleanly.
async function loadWasmModule(): Promise<PAGXModule> {
    if (cachedModule) {
        return cachedModule;
    }
    const wasmBuffer = await fetchWithProgress(WASM_URL, (loaded, total) => {
        loadingProgress.wasmLoaded = loaded;
        if (total > 0) {
            loadingProgress.wasmTotal = total;
        }
        updateProgressUI();
    });
    loadingProgress.wasmDone = true;
    updateProgressUI();
    // locateFile is called by emscripten glue for sidecar files like *.mem or *.data. wasm
    // itself is provided via wasmBinary so this fallback rarely fires; when it does, we still
    // want it to go through the same asset base as everything else.
    const module = (await PAGXInit({
        locateFile: (file: string) => assetUrl(`wasm-mt/${file}`),
        wasmBinary: wasmBuffer,
    })) as PAGXModule;
    cachedModule = module;
    return module;
}

// ---------------------------------------------------------------------------------------------
// Drop zone UI state machine.
// ---------------------------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------------------------
// Player integration.
// ---------------------------------------------------------------------------------------------

// External resources referenced from within a pagx (relative to the loaded sample's directory).
// Guards against path traversal and protocol schemes so a malicious document can't force a
// fetch into a different origin or the local filesystem.
function isSafeRelativePath(p: string): boolean {
    return !p.includes('://') && !p.startsWith('/') && !p.includes('..');
}

// Build a resolveResource callback for the player from a base URL. The player invokes the
// callback for every path reported by getExternalFilePaths() after parsePAGX; returning null
// tells the render pipeline to fall back to default data for that resource.
function makeResourceResolver(baseUrl: string) {
    return async (relPath: string): Promise<Uint8Array | null> => {
        if (!isSafeRelativePath(relPath)) {
            console.warn(`Skipping unsafe external file path: ${relPath}`);
            return null;
        }
        try {
            const response = await fetch(baseUrl + relPath);
            if (!response.ok) {
                console.warn(`Failed to fetch ${baseUrl + relPath}: ${response.status}`);
                return null;
            }
            return new Uint8Array(await response.arrayBuffer());
        } catch (error) {
            console.warn(`Failed to load external file ${relPath}:`, error);
            return null;
        }
    };
}

// Feed cached font bytes into the freshly parsed view. pagx-viewer's PAGXView.registerFonts
// is not on the structural PlayerView interface exposed by pagx-player, so we cast to reach it
// - the wasm module we injected via moduleFactory really is a pagx-viewer instance underneath.
async function registerFontsToView(view: unknown): Promise<void> {
    const font = fontData || new Uint8Array(0);
    const emoji = emojiFontData || new Uint8Array(0);
    (view as PAGXView).registerFonts(font, emoji);
}

// Buttons added around pagx-player's built-in Reset + Source Editor toolbar entries. `before`
// is prepended, `after` is appended - we want Samples/Open on the left, Leave on the right so
// the visual order matches the original playground toolbar (Samples | divider | Open | Reset
// | Editor | divider | Leave). Labels/titles are set here rather than in i18n so the toolbar
// text always stays in sync with the current locale at construction time.
function buildToolbarSlots(): { before: ToolbarSlot[]; after: ToolbarSlot[] } {
    const strings = t();
    const before: ToolbarSlot[] = [
        {
            id: 'toolbar-samples-btn',
            title: strings.samplesTitle,
            html: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="3" y="3" width="7" height="7"/>
                <rect x="14" y="3" width="7" height="7"/>
                <rect x="3" y="14" width="7" height="7"/>
                <rect x="14" y="14" width="7" height="7"/>
            </svg>`,
            onClick: () => {
                window.location.hash = '#samples';
            },
        },
        { id: 'toolbar-divider-samples', divider: true },
        {
            id: 'open-btn',
            title: strings.openFile,
            html: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>
            </svg>`,
            onClick: () => {
                const fileInput = document.getElementById('file-input') as HTMLInputElement | null;
                fileInput?.click();
            },
        },
    ];
    const after: ToolbarSlot[] = [
        { id: 'toolbar-divider-leave', divider: true },
        {
            id: 'leave-btn',
            title: strings.leave,
            html: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
                <polyline points="16 17 21 12 16 7"/>
                <line x1="21" y1="12" x2="9" y2="12"/>
            </svg>`,
            onClick: () => {
                goHome();
            },
        },
    ];
    return { before, after };
}

// Apply an edited XML through the currently-loaded player. Used by both the source editor's
// Apply and Save callbacks. Returns an empty string on success, or an error message.
async function applyXml(xmlText: string): Promise<string> {
    if (!player) {
        return 'Player not initialized';
    }
    try {
        const bytes = new TextEncoder().encode(xmlText);
        // Preserve playback position so an Apply doesn't jump the animation back to frame 0.
        // baseURL for external resources stays the same as the currently loaded sample (or
        // empty for drag-and-drop) - re-editing XML keeps the same resource root.
        await player.load(bytes, {
            preserveCurrentTime: true,
            xmlText,
            registerFonts: registerFontsToView,
            resolveResource: currentPlayingFile
                ? makeResourceResolver(assetUrl('samples/'))
                : undefined,
        });
        return '';
    } catch (e) {
        return e instanceof Error ? e.message : String(e);
    }
}

// Instantiate the pagx-player on first use. The player owns canvas/toolbar/playback bar and
// the source editor; the playground just gives it a container and callbacks. The moduleFactory
// wires wasm loading through our fetchWithProgress path so the progress overlay keeps working
// during the initial load; subsequent load() calls hit the cachedModule fast-path with no
// additional network I/O.
function ensurePlayer(): PAGXPlayer {
    if (player) {
        return player;
    }
    const host = document.getElementById('player-host');
    if (!host) {
        throw new Error('#player-host element not found');
    }
    const slots = buildToolbarSlots();
    player = new PAGXPlayer({
        container: host,
        moduleFactory: () => loadWasmModule(),
        iconBaseUrl: RESOURCE_BASE === '' ? './' : `${RESOURCE_BASE}/`,
        enableEditor: true,
        editorCallbacks: {
            onApply: applyXml,
            onSave: async (xmlText: string) => {
                const error = await applyXml(xmlText);
                if (error !== '') {
                    return error;
                }
                // Save = apply + local download. The playground can't write back to the source
                // file (no filesystem access), so we drop a copy in the user's Downloads folder
                // using the currently loaded file's name.
                const blob = new Blob([xmlText], { type: 'application/xml' });
                const url = URL.createObjectURL(blob);
                const anchor = document.createElement('a');
                anchor.href = url;
                anchor.download = currentFileName.endsWith('.pagx')
                    ? currentFileName
                    : `${currentFileName}.pagx`;
                document.body.appendChild(anchor);
                anchor.click();
                document.body.removeChild(anchor);
                URL.revokeObjectURL(url);
                return '';
            },
        },
        extraMenuItems: slots,
    });
    return player;
}

// ---------------------------------------------------------------------------------------------
// PAGX load flow.
// ---------------------------------------------------------------------------------------------
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

// Show the loading overlay and wait for wasm + fonts to be ready. Both fetches are kicked off
// concurrently at page load and cached on their promises so retries and second-file-loads
// share the same in-flight or completed downloads.
async function prepareForLoading(): Promise<void> {
    showLoadingUI();
    resetProgressUI();
    await new Promise(resolve => requestAnimationFrame(resolve));

    if (!wasmLoadPromise) {
        wasmLoadPromise = loadWasmModule();
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

    // Keep the loading overlay visible for a minimum window even when everything is cached; a
    // sub-100ms flash of the overlay is more distracting than reassuring.
    const elapsed = Date.now() - loadingStartTime;
    const minDisplayTime = 300;
    if (elapsed < minDisplayTime) {
        await new Promise(resolve => setTimeout(resolve, minDisplayTime - elapsed));
    }
}

async function loadPAGXData(
    data: Uint8Array,
    name: string,
    resolveResource?: (relPath: string) => Promise<Uint8Array | null>,
): Promise<void> {
    // Callers (loadPAGXFile / loadPAGXSample) invoke prepareForLoading() and closeEditor()
    // before their own file / network read so the loading overlay is visible - and the editor
    // panel is out of the way - while the bytes are being fetched, not just after.
    const p = ensurePlayer();

    // Hide the nav-btns overlay so it doesn't sit on top of the player's own toolbar. It is
    // restored on goHome().
    document.getElementById('nav-btns')?.classList.add('hidden');

    // Decode xmlText once for the source editor; the player forwards it into the editor via
    // the load option so the editor doesn't have to redo the TextDecoder work.
    const xmlText = new TextDecoder('utf-8').decode(data);
    await p.load(data, {
        xmlText,
        registerFonts: registerFontsToView,
        resolveResource,
    });

    p.show();
    hideDropZone();
    document.title = `PAGX Playground - ${name}`;
    currentFileName = name;
}

async function loadPAGXFile(file: File): Promise<void> {
    try {
        // Close the source editor and show the loading overlay before reading the file so
        // users get immediate feedback that the drop was accepted, and don't see the editor
        // panel lingering over the loading screen while arrayBuffer() reads the bytes.
        // Only user-initiated new loads (drag-drop, samples click, ?sample= URL) close the
        // editor; editor Apply/Save go through applyXml -> player.load directly and preserve
        // the editor state.
        player?.closeEditor();
        await prepareForLoading();
        const fileBuffer = await file.arrayBuffer();
        // Drag-and-drop files have no baseURL for external references (external assets
        // packaged next to a .pagx aren't accessible over http); pass undefined so the
        // player's default (no external fetches) applies.
        await loadPAGXData(new Uint8Array(fileBuffer), file.name);
        currentPlayingFile = null;
        history.replaceState(null, '', window.location.pathname);
    } catch (error) {
        console.error('Failed to load PAGX file:', error);
        // Load failed: the player has already torn down its own UI via its failure path, but
        // we still own the outer chrome (nav-btns overlay + currentPlayingFile pointer for
        // downstream resource resolution). Restore both so the user can navigate to Samples
        // from the error state and so a stale sample name doesn't leak into any subsequent
        // editor Apply that would otherwise fetch resources from the previous sample's base.
        currentPlayingFile = null;
        document.getElementById('nav-btns')?.classList.remove('hidden');
        showErrorUI(error instanceof Error ? error.message : t().errorFormat);
    }
}

function isValidSampleName(name: string): boolean {
    // Only allow filenames like "foo.pagx" or "foo-bar_baz.pagx".
    // Reject path separators, protocol schemes, and other suspicious patterns.
    return /^[\w][\w.\-]*\.pagx$/.test(name);
}

async function loadPAGXSample(name: string, pushHistory: boolean = true): Promise<void> {
    if (!isValidSampleName(name)) {
        showErrorUI(t().errorFormat);
        return;
    }
    try {
        // Mirror loadPAGXFile: close the editor and reveal the loading overlay before the
        // network round-trip so click feedback is immediate even on slow CDN networks.
        player?.closeEditor();
        await prepareForLoading();
        const url = assetUrl(`samples/${name}`);
        // Fetch the sample bytes ourselves (rather than delegating to prepareForLoading) so a
        // 404 from the CDN surfaces as a network error in the drop-zone instead of stalling
        // the player.
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`Failed to fetch: ${response.status} ${response.statusText}`);
        }
        const fileBuffer = await response.arrayBuffer();
        await loadPAGXData(
            new Uint8Array(fileBuffer),
            name,
            makeResourceResolver(assetUrl('samples/')),
        );
        currentPlayingFile = name;

        const cleanUrl = `${window.location.pathname}?sample=${encodeURIComponent(name)}`;
        if (pushHistory) {
            history.pushState(null, '', cleanUrl);
        }
    } catch (error) {
        console.error('Failed to load PAGX sample:', error);
        // Same recovery as loadPAGXFile's catch: reveal the nav overlay so the user isn't
        // stranded on the error UI, and null out currentPlayingFile so a stale name from a
        // prior successful load doesn't drive resource resolution on the next Apply.
        currentPlayingFile = null;
        document.getElementById('nav-btns')?.classList.remove('hidden');
        showErrorUI(error instanceof Error ? error.message : 'Unknown error');
    }
}

function getSampleNameFromParams(): string | null {
    const params = new URLSearchParams(window.location.search);
    return params.get('sample');
}

function goHome(pushHistory: boolean = true): void {
    // The player retains its wasm view / cached wasm module across hides so a subsequent
    // open() (drag-drop, samples click) can jump straight to load() without re-initializing.
    player?.hide();

    // Restore the top-level Samples / Docs overlay and drop the currently loaded file
    // metadata so a fresh open() starts from a clean slate.
    document.getElementById('nav-btns')?.classList.remove('hidden');
    showDropZoneUI();
    document.title = DEFAULT_TITLE;
    currentPlayingFile = null;
    currentFileName = 'export.pagx';
    // Going home discards any pre-samples playback state: hideSamplesPage may still fire
    // afterwards (popstate path that leaves #samples), and without this reset it would call
    // player.play() on the just-hidden player, resuming the render loop against a hidden
    // canvas.
    wasPlayingBeforeSamples = false;

    if (pushHistory) {
        history.pushState(null, '', window.location.pathname);
    }
}

// ---------------------------------------------------------------------------------------------
// Drag & drop.
// ---------------------------------------------------------------------------------------------
function setupDragAndDrop(): void {
    const dropZone = document.getElementById('drop-zone') as HTMLDivElement;
    const dropZoneContent = document.getElementById('drop-zone-content') as HTMLDivElement;
    const errorContent = document.getElementById('error-content') as HTMLDivElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const fileInput = document.getElementById('file-input') as HTMLInputElement;

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
            // Only toggle the drag-over visual cue. Do NOT clear `hidden` here: when a document
            // is already loaded, drop-zone is hidden and its internal sub-state is still frozen
            // on the previous load's "loading" screen (hideDropZone only touches the outer
            // hidden class). Revealing it on hover would flash that stale loading UI. Users see
            // the loading page only after actually dropping the file, when loadPAGXData ->
            // prepareForLoading -> showLoadingUI takes over deliberately.
            dropZone.classList.add('drag-over');
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

    fileInput.addEventListener('change', () => {
        const files = fileInput.files;
        if (files && files.length > 0) {
            loadPAGXFile(files[0]);
        }
        fileInput.value = '';
    });
}

// ---------------------------------------------------------------------------------------------
// Environment probes.
// ---------------------------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------------------------
// i18n / static text application.
// ---------------------------------------------------------------------------------------------
function applyI18n(): void {
    const strings = t();
    const locale = getLocale();
    document.documentElement.lang = locale === 'zh' ? 'zh-CN' : 'en';

    const dropText = document.querySelector('.drop-text');
    const dropSubtext = document.querySelector('.drop-subtext');
    const loadingText = document.querySelector('.loading-text');
    const errorTitle = document.querySelector('.error-title');

    if (dropText) dropText.textContent = strings.dropText;
    if (dropSubtext) dropSubtext.textContent = strings.dropSubtext;
    if (loadingText) loadingText.textContent = strings.loading;
    if (errorTitle) errorTitle.textContent = strings.errorTitle;

    const samplesBtn = document.getElementById('samples-btn');
    const samplesBtnText = document.getElementById('samples-btn-text');
    if (samplesBtn) samplesBtn.title = strings.samplesTitle;
    if (samplesBtnText) samplesBtnText.textContent = strings.samples;

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

// ---------------------------------------------------------------------------------------------
// Samples page.
// ---------------------------------------------------------------------------------------------
async function loadSampleList(): Promise<void> {
    if (sampleFiles.length > 0) {
        return;
    }
    const response = await fetch(assetUrl('samples/index.json'));
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
        const imageUrl = assetUrl(`samples/images/${baseName}.webp`);

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
    // Idempotent guard: hashchange -> handleRoute() and other routing edges can fire this
    // twice in a row for the same navigation. A second call would re-snapshot the player's
    // isPlaying() state after our own pause() from the first call already forced it to
    // false, silently losing the pre-samples playing flag that hideSamplesPage relies on
    // to restore playback on return.
    if (!samplesPage.classList.contains('hidden')) {
        return;
    }
    container.classList.add('hidden');
    samplesPage.classList.remove('hidden');
    document.title = t().samplesTitle;

    // Explicitly stop the wasm render loop while the samples overlay is on screen. Browser
    // rAF throttling on display:none already lightens the load, but pagx-viewer's internal
    // ticker isn't guaranteed to be pure-rAF driven; pausing here guarantees zero GPU cost
    // and avoids ResizeObserver / draw() churn behind the overlay. Snapshot the pre-samples
    // playing state so hideSamplesPage can restore it - a running animation must resume on
    // return, a paused one must stay paused.
    wasPlayingBeforeSamples = player?.getView()?.isPlaying() ?? false;
    player?.pause();

    loadSampleList().then(renderSampleList).catch((error) => {
        console.error('Failed to load samples:', error);
    });
}

function hideSamplesPage(): void {
    const container = document.getElementById('container') as HTMLDivElement;
    const samplesPage = document.getElementById('samples-page') as HTMLDivElement;
    // Idempotent guard mirrors showSamplesPage - a second hide would clobber the snapshot
    // that was already consumed, and could call player.play() on a player whose canvas is
    // no longer visible if goHome ran between the two hide calls.
    if (samplesPage.classList.contains('hidden')) {
        return;
    }
    container.classList.remove('hidden');
    samplesPage.classList.add('hidden');
    if (wasPlayingBeforeSamples) {
        player?.play();
    }
    wasPlayingBeforeSamples = false;
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

// ---------------------------------------------------------------------------------------------
// Bootstrap.
// ---------------------------------------------------------------------------------------------
if (typeof window !== 'undefined') {
    window.onload = async () => {
        applyI18n();

        // Routing wires up first so back/forward and #samples clicks resolve immediately even
        // during the async wasm/font prefetch below.
        window.addEventListener('hashchange', handleRoute);
        window.addEventListener('popstate', handlePopState);
        handleRoute();

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
        wasmLoadPromise = loadWasmModule().catch(error => {
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
    };
}
