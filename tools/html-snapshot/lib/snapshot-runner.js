// Reusable snapshot pipeline. Wraps the steps that snapshot.js (CLI) and
// server.js (HTTP service) both need: open a fresh page in an already-
// launched browser, settle it, inline images / canvases / icon fonts, and
// evaluate the in-page `takeSnapshot()` helper. The browser is *not* closed
// — the caller owns its lifecycle so the HTTP service can keep one browser
// warm across many requests.
//
// Returns: { html, width, height }

'use strict';

const {
  TAKE_SNAPSHOT_EXPR,
  SNAPSHOT_INIT_SCRIPT,
  inlineExternalImages,
  inlineCanvases,
} = require('./browser-snapshot');
const { inlineIconFontsOnPage, ICON_FONT_INIT_SCRIPT } = require('./icon-font');
const { openAndSettlePage } = require('./page-loader');
const { responseBytes } = require('./browser-engine');
const { errMessage } = require('./cli');

// Build a `page.on('response')` listener that captures every successful
// image response into `cache` as a `url -> data:URI` mapping. The map is
// later handed to `inlineExternalImages` so the in-page pass reuses the
// browser's already-loaded bytes instead of re-fetching cross-origin
// images (which the legacy `fetch(url, { mode: 'cors' })` path silently
// failed for, since most image CDNs do not return CORS headers).
//
// We capture by URL key (not by element ref) so the same image used by
// multiple <img> tags pays a single base64 conversion. The listener never
// throws — a detached response or scheme we don't support just leaves the
// URL absent from the cache, and the in-page fetch fallback will retry.
function makeImageCaptureListener(engine, cache, log) {
  return async function captureImageResponse(resp) {
    try {
      const req = resp.request();
      if (req.resourceType() !== 'image') return;
      const url = req.url();
      if (!url || url.startsWith('data:')) return;
      if (!resp.ok()) return;
      const buf = await responseBytes(resp, engine);
      const ct = resp.headers()['content-type'] || 'application/octet-stream';
      cache.set(url, `data:${ct};base64,${buf.toString('base64')}`);
    } catch (err) {
      // Common case: response detached after a navigation, or the request
      // was served from cache and the body is no longer accessible. Drop
      // it — the in-page fetch fallback handles cache misses.
      if (log) log(`image capture skipped: ${errMessage(err)}`);
    }
  };
}

// Run the full snapshot pipeline against `targetUrl` using the already-
// launched browser in `engineHandle`. The page is always closed before this
// function returns (success or failure); the browser stays open.
//
// `opts`:
//   viewportWidth, viewportHeight, waitMs, selector, cookies, headers
//                          — forwarded to openAndSettlePage
//   inlineIconFonts        — when true (default), webfont icon glyphs are
//                            replaced with inline <svg>
//   log                    — optional `(string) => void` for progress / error
//                            diagnostics. When omitted, the pipeline runs
//                            silently. Image-capture skip diagnostics are
//                            intentionally suppressed regardless to match
//                            snapshot.js's pre-extraction behaviour (one
//                            "skipped" line per cross-origin image was too
//                            noisy).
async function runSnapshot(engineHandle, targetUrl, opts) {
  const {
    viewportWidth = 1400,
    viewportHeight = 900,
    waitMs = 800,
    selector = '',
    cookies = [],
    headers = [],
    inlineIconFonts = true,
    log = null,
  } = opts || {};

  const { engine } = engineHandle;
  const imageBytesByUrl = new Map();
  const page = await openAndSettlePage(engineHandle, targetUrl, {
    viewportWidth,
    viewportHeight,
    waitMs,
    selector,
    cookies,
    headers,
    onConsole: log
      ? (msg) => {
          if (msg.type() === 'error') log(`page error: ${msg.text()}`);
        }
      : null,
    onPageError: log ? (err) => log(`page exception: ${errMessage(err)}`) : null,
    onResponse: makeImageCaptureListener(engine, imageBytesByUrl, null),
    // Ship both helper bundles to the page exactly once at navigation
    // time. Subsequent `page.evaluate(...)` calls only have to send the
    // entry expression (`TAKE_SNAPSHOT_EXPR` and the icon-font helpers'
    // dispatch wrappers in lib/icon-font.js), instead of re-encoding the
    // ~80 KB helper source for every snapshot.
    initScripts: [SNAPSHOT_INIT_SCRIPT, ICON_FONT_INIT_SCRIPT],
  });

  try {
    // PAGX's renderer can read `data:` URIs and local files but not
    // `http(s)://` URLs. Inline every external image into a base64 data
    // URI here so the downstream `pagx render` step can decode them
    // directly. The Map captured above is handed in as a plain object so
    // `page.evaluate` can serialise it across the CDP boundary; misses
    // fall back to in-page fetch (limited to 8 concurrent requests
    // inside the helper).
    await page.evaluate(inlineExternalImages, Object.fromEntries(imageBytesByUrl));

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

    // `takeSnapshot` lives on `window.__pagxSnapshot` thanks to
    // `SNAPSHOT_INIT_SCRIPT` registered above; the evaluate call only
    // ships the ~70-byte entry expression now.
    return await page.evaluate(TAKE_SNAPSHOT_EXPR);
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

module.exports = { runSnapshot, makeImageCaptureListener };
