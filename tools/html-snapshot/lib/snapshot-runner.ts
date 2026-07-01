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
  SNAPSHOT_INIT_SCRIPT,
  buildTakeSnapshotExprWithHint,
  inlineExternalImages,
  inlineCanvases,
  materializeDecorativePseudoElements,
} from './browser-snapshot';
import { inlineIconFontsOnPage, ICON_FONT_INIT_SCRIPT } from './icon-font';
import {
  capturePagxAnimationsOnPage,
  PAGX_TRANSITION_INIT_SCRIPT,
  PAGX_ANIM_PAUSE_INIT_SCRIPT,
  PAGX_VIRTUAL_CLOCK_INIT_SCRIPT,
} from './animation-capture';
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
import { responseBytes, setViewport } from './browser-engine';
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
      // Coalesce concurrent listeners for the same URL onto a single body
      // read; the second-comer just awaits the first one and skips the
      // network/buffer cost.
      const pending = cache.awaitInflight(url);
      if (pending !== undefined) { await pending; return; }
      const slot = cache.beginInflight(url);
      if (slot === null) { return; }
      const headers = resp.headers() || {};
      const contentType = headers['content-type'] || 'application/octet-stream';
      if (!isCacheableContentType(url, contentType)) {
        slot.settle(undefined);
        return;
      }
      let entry: CachedResource | undefined;
      try {
        const buf = await responseBytes(resp, engine);
        entry = { status: 200, contentType, body: buf };
        cache.set(url, entry);
      } finally {
        slot.settle(entry);
      }
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
  captureAnimations?: boolean;
  scrollReveal?: boolean;
  downloadFonts?: boolean;
  fontDir?: string;
  downloadImages?: boolean;
  imageDir?: string;
  log?: CaptureLogger | null;
}

// Step the page from top to bottom (and back) before the snapshot is walked so
// that scroll-triggered reveal animations fire and lazy-loaded media is
// fetched. Many marketing pages keep their below-the-fold sections at
// `opacity: 0` until an IntersectionObserver flips them visible on scroll; the
// snapshot is otherwise taken at scroll `(0,0)` where those sections are still
// hidden (and therefore dropped). Walking the page once leaves those reveal
// classes applied (reveal libraries unobserve after the first hit), and the
// image `response` listener captures any `loading="lazy"` images that load
// along the way. `takeSnapshot` scrolls back to `(0,0)` before measuring, so
// the final geometry is unchanged — only the visibility/state is advanced.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function scrollThroughPage(page: any, settleMs: number, finalWaitMs: number): Promise<void> {
  await page.evaluate(async (perStepMs: number) => {
    const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
    const docHeight = () =>
      Math.max(
        document.body ? document.body.scrollHeight : 0,
        document.documentElement ? document.documentElement.scrollHeight : 0,
      );
    const viewport = window.innerHeight || 800;
    const step = Math.max(1, Math.floor(viewport * 0.8));
    // Cap the number of steps so a page that keeps growing (infinite scroll)
    // cannot loop forever.
    for (let i = 0; i < 200; i++) {
      const prevY = window.scrollY;
      const target = Math.min(prevY + step, docHeight());
      window.scrollTo(0, target);
      await sleep(perStepMs);
      // No further progress (reached the bottom or scroll is pinned): stop.
      if (window.scrollY <= prevY) break;
    }
    // Let the last batch of reveals/transitions settle at the bottom, then
    // return to the top so geometry is measured from the natural origin.
    await sleep(perStepMs);
    window.scrollTo(0, 0);
  }, settleMs);
  if (finalWaitMs > 0) {
    await new Promise((r) => setTimeout(r, finalWaitMs));
  }
}

export interface RunSnapshotResult {
  html: string;
  width: number;
  height: number;
  fonts: SavedFont[];
  images: SavedImage[];
}

// Read the body's intrinsic content size (the same `body.scrollWidth /
// body.scrollHeight` that snapshotMain() will pick later as the canvas size,
// see lib/browser-snapshot.ts §snapshotMain), without committing the same
// destructive layout edits snapshotMain() makes. Used by the second-pass
// viewport settle in runSnapshot() to detect pages whose script-driven layout
// (e.g. a `fit()` handler that reads window.innerWidth and writes
// transform: scale(...) on a fixed-size stage) reflows when the viewport
// is resized off the launch default.
//
// We deliberately do NOT zero body margin/padding or `position: relative`
// here — those land later in snapshotMain(). Most pages already reset their
// own UA margin (see e.g. game_ui's `*{margin:0;padding:0;box-sizing:border-box}`
// and the `<body style="margin:0">`), so a transient 8px UA margin only shifts
// the measured rect by a few pixels and doesn't change whether the second
// setViewport pass is needed. Keeping this read non-destructive means a
// failure to converge can't damage the page state the rest of the pipeline
// runs against.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function measureBodyScrollSize(page: any): Promise<{ width: number; height: number }> {
  return page.evaluate(() => {
    const body = document.body;
    if (!body) return { width: 0, height: 0 };
    void body.offsetHeight;
    const rect = body.getBoundingClientRect();
    return {
      width: Math.max(body.scrollWidth, Math.round(rect.width)),
      height: Math.max(body.scrollHeight, Math.round(rect.height)),
    };
  });
}

// Wait roughly two display frames for the engine compositor to flush a layout
// change. Matches the SEEK_SETTLE_MS budget in eval-animation/baseline-frames.js
// for the same reason: a single frame is racy under headless Chromium, two
// frames are a comfortable budget that has held across the eval corpus.
const VIEWPORT_RESETTLE_MS = 60;

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
    captureAnimations = true,
    scrollReveal = false,
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
    // The transition recorder must be installed before the page's own scripts
    // run so load-triggered entrance transitions are observed at their start;
    // only register it when animation capture is enabled. PAGX_ANIM_PAUSE_INIT_SCRIPT
    // (same script the baseline uses) is registered alongside it so finite
    // animations stay paused across settle and remain seekable when the
    // universal global sampler drives them to each sample time — otherwise a
    // finite animation that finished during settle would drop out of
    // `document.getAnimations()` and be missed, and the capture would no longer
    // share the baseline's clock.
    // PAGX_VIRTUAL_CLOCK_INIT_SCRIPT is installed first so it wraps
    // setTimeout/setInterval/rAF before any page script schedules on them; the
    // page's timer-driven state machine then stays frozen at t=0 until the
    // animation sampler advances it deterministically (via pagxSeekAllToTime).
    initScripts: captureAnimations
      ? [PAGX_VIRTUAL_CLOCK_INIT_SCRIPT, SNAPSHOT_INIT_SCRIPT, ICON_FONT_INIT_SCRIPT, PAGX_TRANSITION_INIT_SCRIPT, PAGX_ANIM_PAUSE_INIT_SCRIPT]
      : [SNAPSHOT_INIT_SCRIPT, ICON_FONT_INIT_SCRIPT],
  });

  try {
    // With the virtual clock installed the page's timers are frozen at t=0.
    // Flush zero-delay init timers (advanceTo(0)) before measuring the body so
    // the canvas size reflects the same t=0 layout the eval baseline measures
    // (baseline-frames.js does the identical advanceTo(0) before its rect read).
    // Best-effort: no-op when animation capture / the clock are disabled.
    if (captureAnimations) {
      try {
        await page.evaluate(() => {
          const clock = (window as unknown as { __pagxClock?: { advanceTo?: (ms: number) => void } }).__pagxClock;
          if (clock && typeof clock.advanceTo === 'function') clock.advanceTo(0);
        });
      } catch (err) {
        if (log) log(`virtual-clock advanceTo(0) skipped: ${errMessage(err)}`);
      }
    }

    // Second-pass viewport settle. Some pages register a `resize` handler that
    // reads `window.innerWidth/innerHeight` and writes a derived transform
    // back onto a fixed-size stage element — e.g. a 1920x1080 mock with
    // `fit() { screen.style.transform = 'scale(min(w/1920, h/1080))' }`. The
    // launch viewport (1400x900 by default) feeds that handler one set of
    // numbers; the resulting `body.scrollWidth/scrollHeight` (which
    // snapshotMain() will later pick as the canvas size) is different — and
    // would freeze the inline `transform: matrix(scale)` at the *launch*
    // viewport's scale even though the canvas, and any downstream rendering
    // (eval-animation/baseline-frames.js, `pagx render`), runs at the
    // *measured* canvas size. The two scales then disagree and the rendered
    // stage lands smaller than the baseline.
    //
    // Detect this by comparing the measured body to the launch viewport;
    // when they differ, resize the viewport to the measured size and wait
    // one frame so the page's own resize handler reruns at the canvas
    // dimensions everything else uses. Lock that initial measurement as the
    // canvas size: a follow-up resize handler may chain into a layout-
    // mutating step (e.g. an `alignWeapons()` that writes a `--wy` custom
    // property which extends the body), and re-measuring after that would
    // pick a different value than baseline-frames.js (which captures the
    // body rect once, *before* the viewport resize, with all transforms
    // suppressed) — leaving the snapshot's PNG and the baseline PNG at
    // different sizes despite living on the same source page. Best-effort:
    // a measurement failure leaves the viewport at its launch size and the
    // rest of the pipeline proceeds (matching the pre-fix behaviour).
    let lockedCanvas: { width: number; height: number } | null = null;
    try {
      const measured = await measureBodyScrollSize(page);
      if (measured.width > 0 && measured.height > 0
          && (measured.width !== viewportWidth || measured.height !== viewportHeight)) {
        await setViewport(page, engine, {
          width: measured.width,
          height: measured.height,
          deviceScaleFactor: 1,
        });
        await new Promise((r) => setTimeout(r, VIEWPORT_RESETTLE_MS));
        lockedCanvas = { width: measured.width, height: measured.height };
        if (log) {
          log(`viewport resettled ${viewportWidth}x${viewportHeight} -> ${measured.width}x${measured.height} (page reflow)`);
        }
      }
    } catch (err) {
      if (log) log(`viewport resettle skipped: ${errMessage(err)}`);
    }

    // Optionally walk the page top-to-bottom first so scroll-triggered reveal
    // animations fire and lazy media is fetched (the image `response` listener
    // is already attached, so anything loaded during the scroll lands in
    // `imageBytesByUrl` below). Best-effort: a failure leaves the page at its
    // settled, scroll-(0,0) state and the rest of the pipeline proceeds.
    if (scrollReveal) {
      try {
        await scrollThroughPage(page, 250, Math.max(waitMs, 400));
        if (log) log('scroll-reveal: walked page to trigger reveal animations / lazy media');
      } catch (err) {
        if (log) log(`scroll-reveal failed: ${errMessage(err)}`);
      }
    }

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

    // Normalise any running animations (CSS @keyframes, WAAPI, GSAP, anime.js)
    // into the canonical `@keyframes` + `animation` subset the importer plays
    // back. Must run after the icon-font/image inlining and pseudo-element
    // materialisation (so it sees the final DOM) but before the snapshot is
    // walked (so its injected keyframes block and inline `animation-*`
    // longhands are captured). Best-effort: a failure degrades to a static
    // frame.
    if (captureAnimations) {
      try {
        const stats = await capturePagxAnimationsOnPage(page, {
          logger: log || (() => {}),
        });
        if (log && stats.count > 0) {
          log(`captured ${stats.count} animation(s)`);
        }
      } catch (err) {
        if (log) log(`animation capture failed: ${errMessage(err)}`);
      }
    }

    // `takeSnapshot` lives on `window.__pagxSnapshot` thanks to
    // `SNAPSHOT_INIT_SCRIPT` registered above; the evaluate call only
    // ships the ~70-byte entry expression now. When the second-pass
    // viewport settle locked an initial canvas size (a `fit()`-style
    // resize handler reflowed the page), pass it as a hint so
    // snapshotMain() does not reread `body.scrollWidth/scrollHeight`
    // after a follow-up layout mutator (e.g. an `alignWeapons()` that
    // chained off the resize handler) extends the body further.
    const snapshotExpr = lockedCanvas
      ? buildTakeSnapshotExprWithHint(lockedCanvas.width, lockedCanvas.height)
      : TAKE_SNAPSHOT_EXPR;
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
