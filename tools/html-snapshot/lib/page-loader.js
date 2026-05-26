// Shared page-loading flow used by snapshot.js and eval/baseline.js. Both
// callers need the same sequence (open a page, set a viewport, navigate to a
// URL, wait for networkidle, optionally wait for a #root child to appear,
// optionally wait an extra settle delay), so the steps live here in one place.
//
// The flow is engine-agnostic — it accepts either a raw puppeteer/playwright
// Browser handle (legacy callers) or the `{ browser, engine }` wrapper that
// `lib/browser-engine.js`'s `launchBrowser` returns. The engine name is
// threaded through to the adapter's per-engine helpers so cookies, viewport,
// and waitUntil semantics match whichever browser was launched.

'use strict';

const { unwrap, newPage, mapWaitUntil, addCookies, addInitScript } = require('./browser-engine');

// Default React-CDN apps mount under <div id="root">. If the page has such a
// root, wait until it has at least one child; otherwise resolve immediately.
// Defined as a string constant rather than a function — the body is
// serialised into the browser via `page.waitForFunction`, which accepts the
// expression source directly on both engines, so wrapping it in a Node-side
// function would just toString() back to the same value on every call.
const ROOT_HAS_CHILDREN_SCRIPT =
  'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true';

// Open a fresh page on `browserOrWrapper`, navigate to `url`, and apply the
// standard wait strategy. `url` must already be a fully-qualified URL —
// callers are responsible for prepending `file://` when the input is a local
// path. Returns the page; the caller is responsible for closing it.
//
// `browserOrWrapper` may be either:
//   - the `{ browser, engine }` wrapper returned by `launchBrowser`, or
//   - a raw puppeteer Browser (legacy callers — engine defaults to puppeteer).
async function openAndSettlePage(browserOrWrapper, url, opts) {
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
  // `windowAttachScript` from lib/browser-snapshot.js / lib/icon-font.js).
  for (const script of initScripts) {
    if (script) await addInitScript(page, engine, script);
  }

  if (headers.length > 0) {
    const headerMap = {};
    for (const [key, value] of headers) headerMap[key] = value;
    await page.setExtraHTTPHeaders(headerMap);
  }
  if (cookies.length > 0) {
    // Scope cookies to the navigation target so they travel on the very first
    // request. Both engines accept `{ name, value, url }` here.
    const scoped = cookies.map((c) => ({ ...c, url }));
    await addCookies(page, engine, scoped);
  }

  await page.goto(url, { waitUntil: mapWaitUntil(engine, 'networkidle'), timeout: 30000 });

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
  return page;
}

module.exports = { openAndSettlePage };
