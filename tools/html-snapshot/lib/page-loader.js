// Shared puppeteer page-loading flow used by snapshot.js and eval/baseline.js.
// Both callers need the same sequence (set viewport, navigate to a URL, wait
// for networkidle, optionally wait for a #root child to appear, optionally
// wait an extra settle delay), so the steps live here in one place.

'use strict';

// Default React-CDN apps mount under <div id="root">. If the page has such a
// root, wait until it has at least one child; otherwise resolve immediately.
// The function body is serialised into the browser, so it must be fully
// self-contained.
function rootHasChildrenScript() {
  return 'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true';
}

// Open a fresh page on `browser`, navigate to `url`, and apply the standard
// wait strategy. `url` must already be a fully-qualified URL — callers are
// responsible for prepending `file://` when the input is a local path. Returns
// the page; the caller is responsible for closing it.
async function openAndSettlePage(browser, url, opts) {
  const {
    viewportWidth = 1400,
    viewportHeight = 900,
    waitMs = 800,
    selector = '',
    cookies = [],
    headers = [],
    onConsole = null,
    onPageError = null,
  } = opts || {};

  const page = await browser.newPage();
  await page.setViewport({
    width: viewportWidth,
    height: viewportHeight,
    deviceScaleFactor: 1,
  });
  if (onConsole) page.on('console', onConsole);
  if (onPageError) page.on('pageerror', onPageError);

  if (headers.length > 0) {
    const headerMap = {};
    for (const [key, value] of headers) headerMap[key] = value;
    await page.setExtraHTTPHeaders(headerMap);
  }
  if (cookies.length > 0) {
    // page.setCookie expects each cookie scoped to a URL or domain. Scope to
    // the navigation target so the cookie travels on the very first request.
    const scoped = cookies.map((c) => ({ ...c, url }));
    await page.setCookie(...scoped);
  }

  await page.goto(url, { waitUntil: 'networkidle0', timeout: 30000 });

  if (selector) {
    await page.waitForSelector(selector, { timeout: 15000 });
  } else {
    try {
      await page.waitForFunction(rootHasChildrenScript(), { timeout: 10000 });
    } catch (_) { /* not fatal — pages without #root simply skip this wait */ }
  }

  if (waitMs > 0) {
    await new Promise((r) => setTimeout(r, waitMs));
  }
  return page;
}

module.exports = { openAndSettlePage };
