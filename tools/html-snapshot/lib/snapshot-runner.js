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
const { makeFontCaptureListener, saveDownloadedFonts } = require('./font-download');
const { saveDownloadedImages } = require('./image-download');
const { openAndSettlePage } = require('./page-loader');
const { responseBytes } = require('./browser-engine');
const { errMessage } = require('./cli');

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
// multiple <img> tags is stored once. The listener never throws — a detached
// response or scheme we don't support just leaves the URL absent from the
// cache, and the in-page fetch fallback will retry.
function makeImageCaptureListener(engine, cache, log) {
  return async function captureImageResponse(resp) {
    try {
      const req = resp.request();
      if (req.resourceType() !== 'image') return;
      const url = req.url();
      if (!url || url.startsWith('data:')) return;
      if (!resp.ok()) return;
      if (cache.has(url)) return;
      const buf = await responseBytes(resp, engine);
      const ct = resp.headers()['content-type'] || 'application/octet-stream';
      cache.set(url, { buffer: buf, contentType: ct });
    } catch (err) {
      // Common case: response detached after a navigation, or the request
      // was served from cache and the body is no longer accessible. Drop
      // it — the in-page fetch fallback handles cache misses.
      if (log) log(`image capture skipped: ${errMessage(err)}`);
    }
  };
}

// Turn a captured `{ buffer, contentType }` entry into a base64 `data:` URI —
// the default, self-contained representation handed to `inlineExternalImages`
// when images are not being written to disk.
function entryToDataUri(entry) {
  const ct = (entry && entry.contentType) || 'application/octet-stream';
  return `data:${ct};base64,${entry.buffer.toString('base64')}`;
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
//   downloadFonts          — when true, every web font the browser fetched
//                            while rendering is written to `fontDir` as a
//                            plain SFNT (TTF/OTF). Off by default.
//   fontDir                — destination directory for downloaded fonts;
//                            required when `downloadFonts` is true.
//   downloadImages         — when true, every external image the browser
//                            fetched is written to `imageDir` and referenced
//                            by its local file path instead of inlined as a
//                            base64 `data:` URI, keeping the snapshot small.
//                            Off by default (images are inlined).
//   imageDir               — destination directory for downloaded images;
//                            required when `downloadImages` is true.
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
    downloadFonts = false,
    fontDir = '',
    downloadImages = false,
    imageDir = '',
    log = null,
  } = opts || {};

  const { engine } = engineHandle;
  const imageBytesByUrl = new Map();
  const fontBytesByUrl = new Map();
  // Image capture is always on; font capture only when the caller asked for
  // it. Both read disjoint resource types off the same `response` event, so a
  // single composed listener avoids racing two `page.on('response')`
  // handlers (page-loader accepts exactly one).
  const captureImage = makeImageCaptureListener(engine, imageBytesByUrl, null);
  const captureFont = downloadFonts
    ? makeFontCaptureListener(engine, fontBytesByUrl, null)
    : null;
  const onResponse = captureFont
    ? (resp) => {
        captureImage(resp);
        captureFont(resp);
      }
    : captureImage;
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
    onResponse,
    // Ship both helper bundles to the page exactly once at navigation
    // time. Subsequent `page.evaluate(...)` calls only have to send the
    // entry expression (`TAKE_SNAPSHOT_EXPR` and the icon-font helpers'
    // dispatch wrappers in lib/icon-font.js), instead of re-encoding the
    // ~80 KB helper source for every snapshot.
    initScripts: [SNAPSHOT_INIT_SCRIPT, ICON_FONT_INIT_SCRIPT],
  });

  try {
    // PAGX's renderer can read `data:` URIs and local files but not
    // `http(s)://` URLs, so every external image must be turned into one of
    // those before the snapshot is walked. Two strategies, both driven by the
    // bytes the response listener already captured:
    //   - default: inline each image as a base64 `data:` URI so the snapshot
    //     is fully self-contained.
    //   - `--download-images`: write the bytes to `imageDir` and reference the
    //     local file path instead, keeping the snapshot (and the PAGX produced
    //     from it) small. Best-effort — a write failure for a given image
    //     leaves it absent from the map, so it falls back to the in-page fetch
    //     + inline path below.
    // The resulting `url -> data:URI | path` map is handed to
    // `inlineExternalImages` as a plain object so `page.evaluate` can
    // serialise it across the CDP boundary; misses fall back to in-page fetch
    // (limited to 8 concurrent requests inside the helper).
    let images = [];
    const srcByUrl = {};
    if (downloadImages && imageDir) {
      try {
        images = await saveDownloadedImages(imageBytesByUrl, imageDir, log || (() => {}));
        if (log) log(`downloaded ${images.length} image file(s) to ${imageDir}`);
      } catch (err) {
        if (log) log(`image download failed: ${errMessage(err)}`);
      }
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

    // `takeSnapshot` lives on `window.__pagxSnapshot` thanks to
    // `SNAPSHOT_INIT_SCRIPT` registered above; the evaluate call only
    // ships the ~70-byte entry expression now.
    const snapshot = await page.evaluate(TAKE_SNAPSHOT_EXPR);

    // Write the web fonts the browser fetched to disk. The font bytes were
    // captured off the `response` event during load + settle, so they are
    // already in `fontBytesByUrl` by now (no page interaction needed). This
    // is best-effort: a decode/write failure degrades to "no fonts on disk"
    // and the rest of the snapshot still returns.
    let fonts = [];
    if (downloadFonts && fontDir) {
      try {
        fonts = await saveDownloadedFonts(fontBytesByUrl, fontDir, log || (() => {}));
        if (log) log(`downloaded ${fonts.length} font file(s) to ${fontDir}`);
      } catch (err) {
        if (log) log(`font download failed: ${errMessage(err)}`);
      }
    }
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

module.exports = { runSnapshot, makeImageCaptureListener };
