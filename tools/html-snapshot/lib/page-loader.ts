// Shared page-loading flow used by snapshot.js and eval/baseline.js. Both
// callers need the same sequence (open a page, set a viewport, navigate to a
// URL, wait for networkidle, optionally wait for a #root child to appear,
// optionally wait an extra settle delay), so the steps live here in one place.
//
// The flow is engine-agnostic — it accepts either a raw puppeteer/playwright
// Browser handle (legacy callers) or the `{ browser, engine }` wrapper that
// `lib/browser-engine.ts`'s `launchBrowser` returns. The engine name is
// threaded through to the adapter's per-engine helpers so cookies, viewport,
// and waitUntil semantics match whichever browser was launched.

import {
  unwrap,
  newPage,
  mapWaitUntil,
  addCookies,
  addInitScript,
  waitForNetworkIdle,
  type Browser,
  type BrowserResponse,
  type CookieParam,
  type EngineName,
  type EngineWrapper,
  type Page,
} from './browser-engine';
import { MAX_CAPTURE_HEIGHT_PX } from './common';

// Default React-CDN apps mount under <div id="root">. If the page has such a
// root, wait until it has at least one child; otherwise resolve immediately.
// Defined as a string constant rather than a function — the body is
// serialised into the browser via `page.waitForFunction`, which accepts the
// expression source directly on both engines, so wrapping it in a Node-side
// function would just toString() back to the same value on every call.
const ROOT_HAS_CHILDREN_SCRIPT =
  'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true';

// Many corpus pages defer below-the-fold content: cards, images, and whole
// sections mount lazily via `IntersectionObserver`, legacy `scroll` listeners,
// or native `<img loading="lazy">`. None of those fire while the page sits
// un-scrolled at the top, so a naive screenshot captures skeleton placeholders
// (or a body that collapsed to the initial viewport height) instead of the
// real page.
//
// `AUTO_SCROLL_SWEEP_SCRIPT` performs one top-to-bottom sweep in
// viewport-sized steps so every lazy trigger enters view at least once, and
// returns the document height afterwards. It runs entirely inside the page
// (engine-agnostic — both puppeteer and playwright accept an async function
// string via `page.evaluate`). `stepFraction < 1` overlaps successive
// viewports so an observer with a small negative `rootMargin` never slips
// between two steps.
//
// A single sweep isn't enough for infinite-scroll feeds (bilibili, weibo, …):
// reaching the bottom only *triggers* a data fetch, and the freshly-appended
// rows arrive over the network as skeletons whose fixed height keeps the
// document momentarily "stable". `settleLazyContent` therefore drives several
// sweeps from Node, waiting for `networkidle` between them, so each batch has
// landed before we decide the page has stopped growing. Everything is bounded
// (round count, document height, wall-clock budget) so a feed settles at a
// reasonable length instead of scrolling forever.
const AUTO_SCROLL_SWEEP_SCRIPT = `(async (cfg) => {
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const docHeight = () => Math.max(
    document.body ? document.body.scrollHeight : 0,
    document.documentElement ? document.documentElement.scrollHeight : 0,
  );
  const viewportH = window.innerHeight || 900;
  const step = Math.max(1, Math.floor(viewportH * cfg.stepFraction));
  let y = window.scrollY || 0;
  for (let pass = 0; pass < cfg.maxStepsPerSweep; pass++) {
    const height = docHeight();
    y = Math.min(y + step, height);
    try { window.scrollTo(0, y); } catch (_) { /* ignore */ }
    // Nudge legacy scroll-listener lazy loaders; IntersectionObserver fires on
    // its own once the element enters view, so this is only a belt-and-braces
    // signal for older libraries.
    try { window.dispatchEvent(new Event('scroll')); } catch (_) { /* ignore */ }
    await sleep(cfg.stepDelayMs);
    if ((y + viewportH) >= docHeight() - 2) break;
    if (docHeight() >= cfg.maxHeightPx) break;
  }
  return docHeight();
})`;

// Wait (bounded) for any in-flight images to finish decoding so they actually
// paint, then return to the origin so downstream rect measurement / screenshot
// framing (which also resets scroll) starts clean, and flush layout.
const AUTO_SCROLL_FINALIZE_SCRIPT = `(async (cfg) => {
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  try {
    const imgs = Array.prototype.slice.call(document.images || [])
      .filter((im) => im && !im.complete);
    await Promise.race([
      Promise.all(imgs.map((im) => (im.decode ? im.decode().catch(() => {}) : Promise.resolve()))),
      sleep(cfg.imageWaitMs),
    ]);
  } catch (_) { /* non-fatal */ }
  try { window.scrollTo(0, 0); } catch (_) { /* ignore */ }
  if (document.body) { void document.body.offsetHeight; }
})`;

// Many corpus pages mount a React app via `@babel/standalone` loaded from a CDN
// with `<script type="text/babel" data-presets="react">`. Recent Babel releases
// flipped `@babel/preset-react`'s default `runtime` from `classic` to
// `automatic`. Under the automatic runtime JSX compiles to
// `import { jsx } from "react/jsx-runtime"`, and Babel injects that output as a
// plain (non-module) <script>, so the browser throws "Cannot use import
// statement outside a module" — the app never mounts and the body collapses to
// zero height. We can't pin the CDN version, so force the classic runtime by
// wrapping the `react` preset before Babel's DOMContentLoaded transform runs.
// Registering the listener from an init script (document_start) guarantees it
// fires before Babel's own handler, and the patch is a no-op on pages that
// never define `window.Babel`.
const BABEL_CLASSIC_RUNTIME_SHIM = `(function () {
  function patch() {
    try {
      var B = window.Babel;
      var preset = B && B.availablePresets && B.availablePresets.react;
      if (typeof preset !== 'function' || preset.__classicRuntimePatched) return;
      var wrapped = function (api, opts) {
        opts = opts || {};
        if (opts.runtime === undefined) opts.runtime = 'classic';
        return preset(api, opts);
      };
      wrapped.__classicRuntimePatched = true;
      B.availablePresets.react = wrapped;
    } catch (e) { /* leave the page untouched if Babel's shape changed */ }
  }
  document.addEventListener('DOMContentLoaded', patch, true);
})();`;

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type ConsoleListener = (msg: any) => void;
export type PageErrorListener = (err: Error) => void;
export type ResponseListener = (resp: BrowserResponse) => void | Promise<void>;

export interface OpenAndSettlePageOptions {
  viewportWidth?: number;
  viewportHeight?: number;
  waitMs?: number;
  selector?: string;
  cookies?: CookieParam[];
  // [key, value] pairs.
  headers?: Array<[string, string]>;
  onConsole?: ConsoleListener | null;
  onPageError?: PageErrorListener | null;
  onResponse?: ResponseListener | null;
  initScripts?: string[];
  // Walk the viewport from top to bottom before settling so lazy-loaded content
  // (IntersectionObserver / scroll listeners / native `loading="lazy"`) is
  // realised instead of captured as skeleton placeholders. Defaults to true;
  // set `HTML_SNAPSHOT_NO_AUTOSCROLL=1` (or pass `false`) to skip it.
  autoScroll?: boolean;
}

// Tuning for the lazy-content warm-up scroll. Bounded on every axis so an
// infinite-scroll feed settles at a reasonable length rather than growing
// unbounded. Shared by every caller of `openAndSettlePage` so the baseline and
// the subset always measure the same page state.
const AUTO_SCROLL_CONFIG = {
  // Fraction of the viewport to advance per step (<1 overlaps successive views).
  stepFraction: 0.8,
  // Pause after each in-page step for observers/listeners to react and requests
  // to fire before the next step.
  stepDelayMs: 120,
  // Steps in a single top-to-bottom sweep (a tall feed rendered in one round).
  maxStepsPerSweep: 80,
  // Sweeps to run: each triggers the next infinite-scroll batch; we stop early
  // once the document stops growing (see `settleLazyContent`).
  maxRounds: 12,
  // Per-round wait for the freshly-triggered data/image fetches to go quiet.
  networkIdleMs: 4000,
  // Hard caps guarding against infinite-scroll pages that never stop growing.
  // Stop sweeping once the page has grown past the height we can actually
  // capture: the baseline and subset both clamp the final canvas to
  // MAX_CAPTURE_HEIGHT_PX, so scrolling a feed taller than that only wastes the
  // time budget on content that will be cropped off-canvas anyway.
  maxHeightPx: MAX_CAPTURE_HEIGHT_PX,
  maxTotalMs: 25000,
  // Rounds of unchanged document height before we call the page settled.
  stableRounds: 2,
  // Bounded wait for in-flight image decode in the finalize step.
  imageWaitMs: 5000,
};

// Realise lazy content by alternating an in-page top-to-bottom sweep with a
// Node-side `networkidle` wait, repeating until the document stops growing (or
// a bound trips). Non-fatal: a page that throws mid-scroll (or an engine that
// rejects the evaluate) falls through to whatever is already rendered.
async function settleLazyContent(page: Page, engine: EngineName): Promise<void> {
  const cfg = AUTO_SCROLL_CONFIG;
  const cfgJson = JSON.stringify(cfg);
  const startedAt = Date.now();
  let lastHeight = -1;
  let stable = 0;
  try {
    for (let round = 0; round < cfg.maxRounds; round++) {
      if (Date.now() - startedAt >= cfg.maxTotalMs) break;
      const height: number = await page.evaluate(
        `${AUTO_SCROLL_SWEEP_SCRIPT}(${cfgJson})`,
      );
      // Let this round's freshly-triggered data/image fetches land before we
      // judge whether the page has stopped growing.
      await waitForNetworkIdle(page, engine, cfg.networkIdleMs);
      if (typeof height === 'number' && Math.abs(height - lastHeight) < 4) {
        stable += 1;
      } else {
        stable = 0;
        lastHeight = typeof height === 'number' ? height : lastHeight;
      }
      if (stable >= cfg.stableRounds) break;
      if (typeof height === 'number' && height >= cfg.maxHeightPx) break;
    }
    await page.evaluate(`${AUTO_SCROLL_FINALIZE_SCRIPT}(${cfgJson})`);
  } catch (_) {
    // Best-effort: still try to return the page to the origin so later framing
    // isn't thrown off by a mid-scroll failure.
    try {
      await page.evaluate('try { window.scrollTo(0, 0); } catch (e) {}');
    } catch (_e) { /* ignore */ }
  }
}

// Open a fresh page on `browserOrWrapper`, navigate to `url`, and apply the
// standard wait strategy. `url` must already be a fully-qualified URL —
// callers are responsible for prepending `file://` when the input is a local
// path. Returns the page; the caller is responsible for closing it.
//
// `browserOrWrapper` may be either:
//   - the `{ browser, engine }` wrapper returned by `launchBrowser`, or
//   - a raw puppeteer Browser (legacy callers — engine defaults to puppeteer).
export async function openAndSettlePage(
  browserOrWrapper: EngineWrapper | Browser,
  url: string,
  opts?: OpenAndSettlePageOptions,
): Promise<Page> {
  const {
    viewportWidth = 1400,
    viewportHeight = 900,
    waitMs = 800,
    selector = '',
    cookies = [],
    headers = [],
    onConsole = null,
    onPageError = null,
    onResponse = null,
    initScripts = [],
    autoScroll = process.env.HTML_SNAPSHOT_NO_AUTOSCROLL !== '1',
  } = opts || {};

  const { engine } = unwrap(browserOrWrapper);
  const page = await newPage(browserOrWrapper, {
    viewport: { width: viewportWidth, height: viewportHeight, deviceScaleFactor: 1 },
  });
  if (onConsole) page.on('console', onConsole);
  if (onPageError) page.on('pageerror', onPageError);
  // Register onResponse before any navigation so the listener captures the
  // very first request's bytes (favicon, document, …). Used by snapshot.js
  // to cache image responses for later in-page inlining without paying the
  // CORS round-trip a second time.
  if (onResponse) page.on('response', onResponse);
  // Init scripts must be registered before `page.goto` so the page's own
  // inline scripts see the helper bundles already on `window`. Each entry
  // is a self-contained source string (callers prepare it with
  // `windowAttachScript` from lib/browser-snapshot.ts / lib/icon-font.ts).
  // Always register the Babel classic-runtime shim first so it is in place
  // before any caller-supplied init scripts and before the page's own scripts.
  await addInitScript(page, engine, BABEL_CLASSIC_RUNTIME_SHIM);
  for (const script of initScripts) {
    if (script) await addInitScript(page, engine, script);
  }

  if (headers.length > 0) {
    const headerMap: Record<string, string> = {};
    for (const [key, value] of headers) headerMap[key] = value;
    await page.setExtraHTTPHeaders(headerMap);
  }
  if (cookies.length > 0) {
    // Scope cookies to the navigation target so they travel on the very first
    // request. Both engines accept `{ name, value, url }` here.
    const scoped = cookies.map((c) => ({ ...c, url }));
    await addCookies(page, engine, scoped);
  }

  // Navigate gating on `load` (document + all subresources fetched), not
  // `networkidle`. `networkidle` is fragile: it resolves only after the
  // network stays quiet for ~500ms, which is easily defeated by pages that
  // keep connections open or stream a long tail of external requests (CDN
  // keep-alive, Google Fonts, icon webfonts, the Tailwind Play CDN, image
  // hosts). Worse, when many pages load concurrently the shared CDNs saturate
  // and even reaching `<body>` can blow past the 30s deadline — a
  // parser-blocking `<script src="cdn…">` placed after slow stylesheets stalls
  // the parser, so the timeout fallback would see no body and fail the whole
  // render. `load` is reliable under that contention and guarantees a body.
  try {
    await page.goto(url, { waitUntil: mapWaitUntil(engine, 'load'), timeout: 30000 });
  } catch (err) {
    // A navigation *timeout* should still not fail the whole render: the
    // document is usually parsed and painted well before every last subresource
    // resolves, so fall back to whatever is rendered as long as a body exists
    // and let the waitForRoot + waitMs steps below give async apps time to
    // mount. Anything that is not a timeout (a genuinely broken navigation) is
    // re-thrown.
    const msg = err && (err as Error).message ? (err as Error).message : String(err);
    if (!/timeout/i.test(msg)) throw err;
    let hasBody = await page.evaluate(() => !!document.body).catch(() => false);
    if (!hasBody) {
      // No `<body>` yet usually means a parser-blocking `<script src="cdn…">`
      // queued behind slow external stylesheets stalled the HTML parser before
      // it reached `<body>` (common under high concurrency when shared CDNs are
      // saturated). The document is still live — give the parser a bounded
      // extra window to finish rather than failing the whole render, then
      // re-check. Only if the body never materialises is this a genuine dead
      // navigation worth re-throwing.
      hasBody = await page
        .waitForSelector('body', { timeout: 15000 })
        .then(() => true)
        .catch(() => false);
      if (!hasBody) throw err;
    }
  }

  // Best-effort settle: give the network a short, bounded chance to go quiet so
  // late `@font-face` fetches and SPA XHRs land before we screenshot. This is
  // non-fatal — `waitForNetworkIdle` resolves either way — so a chatty CDN only
  // costs the settle budget instead of failing navigation the way a
  // `networkidle`-gated `goto` would.
  await waitForNetworkIdle(page, engine, 5000);

  if (selector) {
    await page.waitForSelector(selector, { timeout: 15000 });
  } else {
    try {
      await page.waitForFunction(ROOT_HAS_CHILDREN_SCRIPT, { timeout: 10000 });
    } catch (_) { /* not fatal — pages without #root simply skip this wait */ }
  }

  if (waitMs > 0) {
    await new Promise((r) => setTimeout(r, waitMs));
  }

  // Realise lazy-loaded content by walking the viewport top-to-bottom. Done
  // after the initial settle (app mounted, first paint done) so observers are
  // registered, and before returning so the caller screenshots/measures the
  // fully-populated page. Runs for baseline, snapshot, and the flex counter
  // alike, keeping every measurement on the same page state.
  if (autoScroll) {
    await settleLazyContent(page, engine);
  }

  return page;
}
