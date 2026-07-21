// Reusable snapshot pipeline. Wraps the steps that snapshot.js (CLI) and
// server.js (HTTP service) both need: open a fresh page in an already-
// launched browser, settle it, inline images / canvases / icon fonts, and
// evaluate the in-page `takeSnapshot()` helper. The browser is *not* closed
// — the caller owns its lifecycle so the HTTP service can keep one browser
// warm across many requests.
//
// Returns: { html, width, height }

import {
  TAKE_SNAPSHOT_EXPR,
  MEASURE_CANVAS_EXPR,
  SNAPSHOT_INIT_SCRIPT,
  inlineExternalImages,
  inlineCanvases,
  materializeDecorativePseudoElements,
} from './browser-snapshot';
import { inlineIconFontsOnPage, ICON_FONT_INIT_SCRIPT } from './icon-font';
import {
  makeFontCaptureListener,
  saveDownloadedFonts,
  type SavedFont,
} from './font-download';
import {
  saveDownloadedImages,
  type ImageCaptureValue,
  type SavedImage,
} from './image-download';
import { openAndSettlePage } from './page-loader';
import {
  makeCaptureListener,
  type CaptureLogger,
  type CaptureResponseListener,
} from './capture-listener';
import { errMessage, SNAPSHOT_DEFAULTS } from './common';
import { applyCanvasViewport } from './canvas-viewport';
import { responseBytes } from './browser-engine';
import type {
  BrowserResponse,
  CookieParam,
  EngineName,
  EngineWrapper,
} from './browser-engine';
import { ResourceCache, type CachedResource } from './resource-cache';

// Build a `page.on('response')` listener that captures every successful
// image response into `cache` as a `url -> { buffer, contentType }` mapping.
// The bytes are later turned into either a `data:` URI (default, self-
// contained snapshot) or an on-disk file path (`--download-images`, small
// snapshot) and handed to `inlineExternalImages`, so the in-page pass reuses
// the browser's already-loaded bytes instead of re-fetching cross-origin
// images (which the legacy `fetch(url, { mode: 'cors' })` path silently
// failed for, since most image CDNs do not return CORS headers).
//
// We capture by URL key (not by element ref) so the same image used by
// multiple <img> tags is stored once.
export function makeImageCaptureListener(
  engine: EngineName,
  cache: Map<string, ImageCaptureValue>,
  log: CaptureLogger | null,
): CaptureResponseListener {
  return makeCaptureListener<ImageCaptureValue>({
    resourceType: 'image',
    engine,
    cache,
    log,
    label: 'image',
    project: (buf: Buffer, resp: BrowserResponse): ImageCaptureValue => ({
      buffer: buf,
      contentType: resp.headers()['content-type'] || 'application/octet-stream',
    }),
  });
}

// Turn a captured `{ buffer, contentType }` entry into a base64 `data:` URI —
// the default, self-contained representation handed to `inlineExternalImages`
// when images are not being written to disk.
function entryToDataUri(entry: ImageCaptureValue): string {
  const ct = (entry && entry.contentType) || 'application/octet-stream';
  return `data:${ct};base64,${entry.buffer.toString('base64')}`;
}

// Decide whether a (url, contentType) pair represents a sub-resource the cache
// is meant to hold (CSS / JS / font). Images, documents, XHR, etc. are skipped:
// they are large or response-specific and would only push the cacheable assets
// (the actually-shared CDN bundles) out of the budget.
function isCacheableContentType(url: string, contentType: string): boolean {
  const ct = (contentType || '').toLowerCase().split(';')[0].trim();
  if (ct.startsWith('text/css') || ct.startsWith('application/javascript') ||
      ct.startsWith('text/javascript') || ct.startsWith('application/x-javascript') ||
      ct.startsWith('application/json') || ct.startsWith('font/') ||
      ct.startsWith('application/font') || ct.startsWith('application/x-font') ||
      ct.startsWith('application/vnd.ms-fontobject')) {
    return true;
  }
  // Many CDN font URLs come back with `application/octet-stream`; fall back to
  // the URL extension so the listener still picks them up.
  const lowerUrl = url.toLowerCase().split('?')[0];
  if (lowerUrl.endsWith('.css') || lowerUrl.endsWith('.js') || lowerUrl.endsWith('.mjs') ||
      lowerUrl.endsWith('.woff') || lowerUrl.endsWith('.woff2') || lowerUrl.endsWith('.ttf') ||
      lowerUrl.endsWith('.otf') || lowerUrl.endsWith('.eot')) {
    return true;
  }
  return false;
}

// Route handler for the `request.continue` / `route.fulfill` API. Given the
// metadata of a network request, return either a `fulfill` decision (with the
// cached body) or `continue` so the engine performs the real network fetch.
// Only http(s) GET requests are considered; everything else falls through.
export interface RouteRequestInfo {
  url: string;
  method: string;
  resourceType: string;
}
export type RouteDecision =
  | { action: 'fulfill'; fulfillment: { status: number; contentType: string; body: Buffer } }
  | { action: 'continue' };

export function makeResourceCacheRouteHandler(
  cache: ResourceCache,
): (req: RouteRequestInfo) => RouteDecision {
  return function handle(req: RouteRequestInfo): RouteDecision {
    if (req.method !== 'GET') return { action: 'continue' };
    if (!/^https?:\/\//i.test(req.url)) return { action: 'continue' };
    const hit = cache.get(req.url);
    if (hit === undefined) return { action: 'continue' };
    return {
      action: 'fulfill',
      fulfillment: { status: hit.status, contentType: hit.contentType, body: hit.body },
    };
  };
}

// `page.on('response')` listener that populates `cache` with successful
// http(s) GET sub-resources (CSS / JS / fonts) so a follow-up snapshot can
// fulfil them without re-hitting the network. Mirrors the
// `makeCaptureListener` shape but writes into a typed `ResourceCache` rather
// than a plain Map, and only stores resources flagged by
// `isCacheableContentType`.
export function makeResourceCacheListener(
  engine: EngineName,
  cache: ResourceCache,
  log: CaptureLogger | null,
): CaptureResponseListener {
  return async function captureResponse(resp: BrowserResponse): Promise<void> {
    try {
      const req = resp.request();
      if (req.method() !== 'GET') return;
      const url = resp.url();
      if (!url || !/^https?:\/\//i.test(url)) return;
      if (!resp.ok()) return;
      if (cache.has(url)) return;
      const headers = resp.headers() || {};
      const contentType = headers['content-type'] || 'application/octet-stream';
      if (!isCacheableContentType(url, contentType)) return;
      const buf = await responseBytes(resp, engine);
      const entry: CachedResource = { status: 200, contentType, body: buf };
      cache.set(url, entry);
    } catch (err) {
      if (log) log(`resource cache capture skipped: ${errMessage(err)}`);
    }
  };
}

export interface RunSnapshotOptions {
  viewportWidth?: number;
  viewportHeight?: number;
  waitMs?: number;
  selector?: string;
  cookies?: CookieParam[];
  headers?: Array<[string, string]>;
  inlineIconFonts?: boolean;
  downloadFonts?: boolean;
  fontDir?: string;
  downloadImages?: boolean;
  imageDir?: string;
  log?: CaptureLogger | null;
}

export interface RunSnapshotResult {
  html: string;
  width: number;
  height: number;
  fonts: SavedFont[];
  images: SavedImage[];
}

// Run a best-effort save step (fonts or images): on success, log the count;
// on failure, log the error and return an empty array so the rest of the
// pipeline still proceeds. Centralised so the font and image branches stop
// re-implementing the same try/catch/log scaffolding around their respective
// `save*` calls.
async function bestEffortSave<TItem, TCache>(
  saveFn: (cache: TCache, dir: string, log: CaptureLogger) => Promise<TItem[]>,
  cache: TCache,
  dir: string,
  log: CaptureLogger | null,
  label: string,
): Promise<TItem[]> {
  try {
    const arr = await saveFn(cache, dir, log || (() => {}));
    if (log) log(`downloaded ${arr.length} ${label} file(s) to ${dir}`);
    return arr;
  } catch (err) {
    if (log) log(`${label} download failed: ${errMessage(err)}`);
    return [];
  }
}

// Run the full snapshot pipeline against `targetUrl` using the already-
// launched browser in `engineHandle`. The page is always closed before this
// function returns (success or failure); the browser stays open.
export async function runSnapshot(
  engineHandle: EngineWrapper,
  targetUrl: string,
  opts?: RunSnapshotOptions,
): Promise<RunSnapshotResult> {
  const {
    viewportWidth = SNAPSHOT_DEFAULTS.viewportWidth,
    viewportHeight = SNAPSHOT_DEFAULTS.viewportHeight,
    waitMs = SNAPSHOT_DEFAULTS.waitMs,
    selector = '',
    cookies = [],
    headers = [],
    inlineIconFonts = true,
    downloadFonts = false,
    fontDir = '',
    downloadImages = false,
    imageDir = '',
    log = null,
  } = opts || {};

  const { engine } = engineHandle;
  const imageBytesByUrl = new Map<string, ImageCaptureValue>();
  const fontBytesByUrl = new Map<string, Buffer>();
  // Image capture is always on; font capture only when the caller asked for
  // it. Both read disjoint resource types off the same `response` event, so a
  // single composed listener avoids racing two `page.on('response')`
  // handlers (page-loader accepts exactly one).
  //
  // The capture listeners forward their per-resource skip messages to `log`
  // when one is provided (`snapshot.js` always passes one; the HTTP service
  // does too). Passing `null` previously silenced every "image/font capture
  // skipped: ..." line, so a real CDN failure was indistinguishable from a
  // healthy run — the snapshot was missing assets but nothing told the
  // operator why.
  const captureImage = makeImageCaptureListener(engine, imageBytesByUrl, log);
  const captureFont: CaptureResponseListener | null = downloadFonts
    ? makeFontCaptureListener(engine, fontBytesByUrl, log)
    : null;
  const onResponse: (resp: BrowserResponse) => void = captureFont
    ? (resp: BrowserResponse) => {
        void captureImage(resp);
        void captureFont(resp);
      }
    : (resp: BrowserResponse) => { void captureImage(resp); };
  const page = await openAndSettlePage(engineHandle, targetUrl, {
    viewportWidth,
    viewportHeight,
    waitMs,
    selector,
    cookies,
    headers,
    onConsole: log
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      ? (msg: any) => {
          if (msg.type() === 'error') log(`page error: ${msg.text()}`);
        }
      : null,
    onPageError: log ? (err: Error) => log(`page exception: ${errMessage(err)}`) : null,
    onResponse,
    // Ship both helper bundles to the page exactly once at navigation
    // time. Subsequent `page.evaluate(...)` calls only have to send the
    // entry expression (`TAKE_SNAPSHOT_EXPR` and the icon-font helpers'
    // dispatch wrappers in lib/icon-font.ts), instead of re-encoding the
    // ~80 KB helper source for every snapshot.
    initScripts: [SNAPSHOT_INIT_SCRIPT, ICON_FONT_INIT_SCRIPT],
  });

  try {
    // PAGX's renderer can read `data:` URIs and local files but not
    // `http(s)://` URLs, so every external image must be turned into one of
    // those before the snapshot is walked.
    let images: SavedImage[] = [];
    const srcByUrl: Record<string, string> = {};
    if (downloadImages && imageDir) {
      images = await bestEffortSave(
        saveDownloadedImages,
        imageBytesByUrl,
        imageDir,
        log,
        'image',
      );
      for (const { url, path: filePath } of images) {
        srcByUrl[url] = filePath;
      }
    } else {
      for (const [url, entry] of imageBytesByUrl) {
        srcByUrl[url] = entryToDataUri(entry);
      }
    }
    await page.evaluate(inlineExternalImages, srcByUrl);

    // Capture each <canvas>'s live bitmap as a data URI so the snapshot
    // walker can emit it as an <img>. Without this, every chart / scripted
    // graphic on the page (ECharts, Chart.js, etc.) becomes an empty box.
    await page.evaluate(inlineCanvases);

    if (inlineIconFonts) {
      try {
        const stats = await inlineIconFontsOnPage(page, {
          logger: log || (() => {}),
        });
        if (log && stats.total > 0) {
          log(`inlined ${stats.inlined}/${stats.total} icon-font glyph(s) as SVG`);
        }
      } catch (err) {
        // The icon-font pass is best-effort: failures degrade to the
        // legacy font-named span path. Surface the diagnostic so the
        // operator can decide whether to investigate, but never abort
        // the snapshot — the rest of the page still has to make it out.
        if (log) log(`inline-icon-fonts failed: ${errMessage(err)}`);
      }
    }

    // Materialise decorative `::before` / `::after` pseudo-elements (toggle
    // sliders, custom radio dots, divider lines, etc.). Runs after the
    // icon-font pass so text-bearing pseudos that were converted to inline
    // SVGs are already removed from `getComputedStyle(el, '::after')` and
    // can't trip the "host carries text pseudo" guard inside the
    // materialiser. Best-effort: a failure here only loses pseudo-element
    // boxes from the snapshot, never the rest of the document — but raise
    // the message above the usual log noise so a regression that disables
    // the entire pass (e.g. a missing helper triggering `ReferenceError`
    // inside `page.evaluate`) does not get buried among importer warnings.
    try {
      await page.evaluate(materializeDecorativePseudoElements);
    } catch (err) {
      if (log) {
        log(`WARNING: pseudo-element materialisation skipped — decorative ::before/::after boxes will be missing from the snapshot: ${errMessage(err)}`);
      }
    }

    // Resize the viewport to the body canvas before measuring geometry, exactly
    // as baseline.js does before it screenshots the ground truth. `pagx render`
    // roots the subset at <body> (0,0), and the baseline is captured in a
    // viewport equal to the body canvas, so any element whose containing block
    // is the initial containing block — a `position: absolute` box anchored by
    // `right`/`bottom` with no positioned ancestor, `position: fixed`, or a
    // `vw`/`vh` length — must be measured against that same box. Measured at the
    // default 1400x900 viewport instead, such an element resolves to
    // viewport-relative coordinates (e.g. `right: 20px` on a 200px-wide body
    // reads `left: 1300px`) that the body-rooted render can't reproduce.
    //
    // `measureCanvas` neutralises <html>/<body> and returns the canvas size the
    // same way baseline.js's `captureBodyRect` does. `applyCanvasViewport`
    // (shared with the baseline) resizes the viewport to that canvas and decides
    // whether the resize was safe — see lib/canvas-viewport.ts for the full
    // rationale and the viewport-dependence guard. When the resize is kept we
    // pass the pre-resize dimensions back into `takeSnapshot` so the emitted
    // canvas matches the baseline's screenshot clip exactly (re-measuring after
    // the resize's reflow could drift by a pixel), while element geometry is
    // measured against the resized viewport. When the guard reverts the resize,
    // we fall back to the no-arg snapshot, which re-measures the canvas at the
    // settle viewport — exactly what the baseline clips to in that case.
    let snapshotExpr = TAKE_SNAPSHOT_EXPR;
    try {
      const canvas = await page.evaluate(MEASURE_CANVAS_EXPR) as {
        width: number; height: number;
      };
      if (canvas && canvas.width > 0 && canvas.height > 0) {
        const { reverted } = await applyCanvasViewport(
          page,
          engine,
          canvas,
          { width: viewportWidth, height: viewportHeight },
          log,
        );
        if (!reverted) {
          snapshotExpr =
            `(() => window.__pagxSnapshot.takeSnapshot(` +
            `{canvasWidth:${canvas.width},canvasHeight:${canvas.height}}))()`;
        }
      }
    } catch (err) {
      // Non-fatal: fall back to the no-arg snapshot at the current viewport.
      // A measurement/resize hiccup should degrade to the previous behaviour,
      // not abort a snapshot we can still produce.
      if (log) log(`viewport resize to canvas skipped: ${errMessage(err)}`);
    }

    // `takeSnapshot` lives on `window.__pagxSnapshot` thanks to
    // `SNAPSHOT_INIT_SCRIPT` registered above; the evaluate call only
    // ships the ~70-byte entry expression now.
    const snapshot = await page.evaluate(snapshotExpr) as {
      html: string; width: number; height: number;
    };

    // Write the web fonts the browser fetched to disk. The font bytes were
    // captured off the `response` event during load + settle, so they are
    // already in `fontBytesByUrl` by now (no page interaction needed). This
    // is best-effort: a decode/write failure degrades to "no fonts on disk"
    // and the rest of the snapshot still returns.
    const fonts: SavedFont[] = (downloadFonts && fontDir)
      ? await bestEffortSave(saveDownloadedFonts, fontBytesByUrl, fontDir, log, 'font')
      : [];
    return { ...snapshot, fonts, images };
  } finally {
    // Always close the page so a long-lived browser (HTTP server) does
    // not leak a tab per request. Swallow close-time errors — the
    // snapshot result has already been captured and we don't want a
    // teardown hiccup to mask the real return value.
    try {
      await page.close();
    } catch (_) {
      /* ignore */
    }
  }
}
