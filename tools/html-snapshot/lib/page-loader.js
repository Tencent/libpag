// Shared puppeteer page-loading flow used by snapshot.js and eval/baseline.js.
// Both callers need the same sequence (set viewport, navigate to file://, wait
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

// Open a fresh page on `browser`, navigate to file://`absolutePath`, and apply
// the standard wait strategy. Returns the page; the caller is responsible for
// closing it.
async function openAndSettlePage(browser, absolutePath, opts) {
  const {
    viewportWidth = 1400,
    viewportHeight = 900,
    waitMs = 800,
    selector = '',
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

  await page.goto('file://' + absolutePath, { waitUntil: 'networkidle0', timeout: 30000 });

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
