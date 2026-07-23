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
  capturePagxAnimationsOnPage,
  settleAndInlineCanvasesOnPage,
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
    // Native setTimeout stashed by PAGX_VIRTUAL_CLOCK_INIT_SCRIPT before it froze
    // the page's timers. The page's own setTimeout only fires when the virtual
    // clock is advanced, so a sleep on it here would never resolve and this
    // real-time reveal walk would hang.
    const st = (window as unknown as { __pagxRealSetTimeout?: typeof setTimeout }).__pagxRealSetTimeout || setTimeout;
    const sleep = (ms: number) => new Promise((r) => st(r, ms));
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

// After `applyCanvasViewport` reverts a ballooned, viewport-dependent page, the
// probe's tall-viewport resize has usually left the layout stuck inflated when
// the virtual clock is installed (animation-capture path). The page's own
// resize handler grew the layout on the tall probe viewport, but its matching
// shrink is queued on the page's timers — which the virtual clock froze — so
// restoring the settle viewport does NOT collapse it. Measured on
// ardot.tencent.com: the canvas balloons from ~4320px to ~12040px and the
// no-arg `takeSnapshot` below then re-measures (and emits) that inflated height,
// producing a hugely over-tall PAGX whose content is spread across mostly-empty
// space.
//
// A plain `resize` event or a wall-clock wait does not help (the shrink is
// debounced on the page's own, frozen, setTimeout); only nudging the virtual
// clock fires the pending timer. So flush those timers by advancing the clock a
// bounded amount (dispatching `resize` first so event-only handlers also
// re-run), which lets the debounced shrink run and the layout collapse back to
// the settle-viewport size. The forward-only clock also stepped WAAPI/GSAP
// animations forward while flushing, so re-pin them to the timeline start
// (mirroring `pagxSeekAllToTime(0)`) — otherwise the emitted static base frame
// would bake in a mid-animation state on top of the captured @keyframes. The
// layout collapse is a JS/style state change, not an animation, so resetting
// animation time does not undo it.
//
// Best-effort and a no-op without the virtual clock (the non-animation path
// never installs it). Only runs on the revert branch, i.e. the already-degraded
// viewport-dependent case; fixed-size animated mocks keep canvas ≈ viewport,
// never balloon, and never reach here.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function resettleAfterViewportRevert(page: any): Promise<void> {
  await page.evaluate(() => {
    const clock = (window as unknown as {
      __pagxClock?: { now?: () => number; advanceTo?: (ms: number) => void };
    }).__pagxClock;
    if (!clock || typeof clock.advanceTo !== 'function' || typeof clock.now !== 'function') return;
    // Clear typical resize/layout debounces (100–1000ms) with margin; three
    // 2s steps also let a handler that reschedules itself settle.
    for (let i = 0; i < 3; i++) {
      try { window.dispatchEvent(new Event('resize')); } catch (_) { /* ignore */ }
      try { clock.advanceTo((clock.now() as number) + 2000); } catch (_) { /* ignore */ }
      void document.body.offsetHeight;
    }
    // Re-pin declarative animations to the timeline start (t=0). WAAPI/CSS:
    // pause + currentTime=0. GSAP: seek the global timeline to 0.
    try {
      if (typeof document.getAnimations === 'function') {
        for (const a of document.getAnimations()) {
          try { a.pause(); a.currentTime = 0; } catch (_) { /* ignore */ }
        }
      }
    } catch (_) { /* ignore */ }
    try {
      const gsap = (window as unknown as { gsap?: { globalTimeline?: { pause: () => void; time: (t: number, suppress?: boolean) => void } } }).gsap;
      if (gsap && gsap.globalTimeline) { gsap.globalTimeline.pause(); gsap.globalTimeline.time(0, true); }
    } catch (_) { /* ignore */ }
    void document.body.offsetHeight;
  });
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
    //
    // Factored into a reusable pass because a JS-driven page (Vue/React) can
    // mount additional <img> elements *after* this first pass runs — most
    // notably when the animation-capture virtual clock is advanced below and a
    // component that was frozen at t=0 finally renders. Those late arrivals
    // must be inlined too before the snapshot walk; otherwise they keep a bare
    // site-absolute `http(s)` src (e.g. `/expert.svg`), which PAGX can neither
    // render nor resolve, so the element is dropped from the output. Each call
    // rebuilds `srcByUrl` from the current `imageBytesByUrl` (the `response`
    // listener keeps filling it as new images load) and re-runs
    // `inlineExternalImages`, which is idempotent — it skips <img> already
    // carrying `data-snapshot-src`, so only the newly mounted images are
    // touched.
    const images: SavedImage[] = [];
    const seenImageUrls = new Set<string>();
    const inlineCapturedImages = async (): Promise<void> => {
      const srcByUrl: Record<string, string> = {};
      if (downloadImages && imageDir) {
        const saved = await bestEffortSave(
          saveDownloadedImages,
          imageBytesByUrl,
          imageDir,
          log,
          'image',
        );
        for (const item of saved) {
          srcByUrl[item.url] = item.path;
          // De-dupe across passes so the returned manifest lists each saved
          // image once even though `saveDownloadedImages` re-reports the full
          // set every call.
          if (!seenImageUrls.has(item.url)) {
            seenImageUrls.add(item.url);
            images.push(item);
          }
        }
      } else {
        for (const [url, entry] of imageBytesByUrl) {
          srcByUrl[url] = entryToDataUri(entry);
        }
      }
      await page.evaluate(inlineExternalImages, srcByUrl);
    };
    await inlineCapturedImages();

    // Capture each <canvas>'s live bitmap as a data URI so the snapshot
    // walker can emit it as an <img>. Without this, every chart / scripted
    // graphic on the page (ECharts, Chart.js, etc.) becomes an empty box.
    //
    // Non-capture path only: the real clock has already let each chart's
    // entrance animation finish during settle, so a plain t=0 read gets the
    // settled frame. In animation-capture mode the virtual clock froze rAF at
    // t=0 (the chart's series has not grown yet), so canvases are settled and
    // inlined AFTER the sampler instead — see `settleAndInlineCanvasesOnPage`
    // below. Doing it here too would bake in the empty first frame.
    if (!captureAnimations) {
      await page.evaluate(inlineCanvases);
    }

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

      // Now that the sampler has finished seeking the (forward-only) virtual
      // clock, settle any rAF-driven <canvas> charts frozen at t=0 and inline
      // their bitmaps as static images. Deferred to here because advancing the
      // clock before sampling would have collapsed the declarative timeline
      // (see `settleAndInlineCanvasesOnPage`). Best-effort: on failure the
      // canvas simply keeps its t=0 (empty) frame, matching prior behaviour.
      try {
        await settleAndInlineCanvasesOnPage(page, { logger: log || (() => {}) });
      } catch (err) {
        if (log) log(`canvas settle failed: ${errMessage(err)}`);
      }
    }

    // Second image-inline pass. Advancing the virtual clock during animation
    // capture (and the canvas settle above) can mount <img> elements that did
    // not exist during the first pass — a common shape on JS-rendered pages
    // where a deferred/transitioned component only renders once its timers
    // fire. Re-inline so those late arrivals become self-contained `data:` /
    // local references instead of unresolvable site-absolute URLs that get
    // dropped downstream. Idempotent (the in-page pass skips already-inlined
    // <img>s), so this only touches images that appeared after the first pass.
    // Best-effort: a failure here just leaves late images as-is, matching prior
    // behaviour.
    try {
      await inlineCapturedImages();
    } catch (err) {
      if (log) log(`late image inline pass skipped: ${errMessage(err)}`);
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
        if (reverted && captureAnimations) {
          // The revert left the frozen-clock page's layout stuck inflated by
          // the probe resize; flush the debounced shrink and re-pin animations
          // so the no-arg `takeSnapshot` below re-measures the collapsed,
          // settle-viewport canvas instead of the ballooned one. Best-effort.
          try {
            await resettleAfterViewportRevert(page);
          } catch (err) {
            if (log) log(`layout re-settle after viewport revert skipped: ${errMessage(err)}`);
          }
        }
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
