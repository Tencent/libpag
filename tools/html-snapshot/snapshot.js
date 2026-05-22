#!/usr/bin/env node
/**
 * html-snapshot
 *
 * Render an arbitrary HTML page (typically a React/Tailwind/lucide app that materialises
 * the DOM at runtime) in a headless browser and emit a flat, absolute-positioned snapshot
 * that conforms to spec/html_subset.md.
 *
 * This file is the Node-side driver only. CLI parsing lives in `lib/cli.js`; the actual
 * in-page snapshot logic lives in `lib/browser-snapshot.js` and is shipped to Chromium
 * via `page.evaluate(...)`. See those files for details.
 *
 * Pipeline:
 *   1. Launch headless Chromium, navigate to the source file.
 *   2. Wait for the page to settle (networkidle + selector + a short delay).
 *   3. Inline every external <img> as a data: URI so the output is self-contained.
 *   4. Walk the rendered DOM (browser-side) and emit a flat HTML string.
 *   5. Write the resulting <!DOCTYPE html>…</html> string next to the input.
 *
 * The output is intended to be fed directly to `pagx import --format html`. The libpag
 * HTMLSubsetTransformer (src/pagx/html/HTMLSubsetTransformer.cpp) handles the final
 * cleanup (unit conversion, dropping any leftover unsupported properties, etc.).
 */

'use strict';

const fs = require('fs');
const puppeteer = require('puppeteer');
const { parseArgs, LOG_PREFIX } = require('./lib/cli');
const { takeSnapshot, inlineExternalImages, inlineCanvases } = require('./lib/browser-snapshot');
const { openAndSettlePage } = require('./lib/page-loader');

async function main() {
  const opts = parseArgs(process.argv);

  if (!opts.isUrl && !fs.existsSync(opts.input)) {
    console.error(`${LOG_PREFIX}input not found: ${opts.input}`);
    process.exit(1);
  }

  // page-loader expects a fully-qualified URL. Local paths become file:// URLs
  // here so the loader stays scheme-agnostic.
  const targetUrl = opts.isUrl ? opts.input : `file://${opts.input}`;

  let browser;
  try {
    browser = await puppeteer.launch({
      headless: true,
      args: ['--no-sandbox', '--font-render-hinting=none'],
    });
  } catch (err) {
    // Most launch failures are missing-Chromium errors after a sandboxed
    // `npm install` redirected the puppeteer cache. Surface a one-line hint
    // so users can fix it without reading the full stack trace; the original
    // error still propagates so non-cache failures aren't masked.
    const msg = (err && err.message) || String(err);
    if (/Could not find Chrome|Failed to launch the browser process/i.test(msg)) {
      console.error(`${LOG_PREFIX}failed to launch headless Chromium. Run:`);
      console.error(`  PUPPETEER_CACHE_DIR="$HOME/.cache/puppeteer" \\`);
      console.error(`    npx --prefix tools/html-snapshot puppeteer browsers install chrome`);
    }
    throw err;
  }
  try {
    const page = await openAndSettlePage(browser, targetUrl, {
      viewportWidth: opts.viewportWidth,
      viewportHeight: opts.viewportHeight,
      waitMs: opts.waitMs,
      selector: opts.selector,
      cookies: opts.cookies,
      headers: opts.headers,
      onConsole: (msg) => {
        if (msg.type() === 'error') {
          console.error(`page error: ${msg.text()}`);
        }
      },
      onPageError: (err) => {
        console.error(`page exception: ${err.message}`);
      },
    });

    // PAGX's renderer can read `data:` URIs and local files but not `http(s)://`
    // URLs. Inline every external image into a base64 data URI here so the
    // downstream `pagx render` step can decode them directly. We stash the result
    // on `data-snapshot-src` rather than mutating `src` so that the live layout
    // (which has already settled around the original image) is left untouched.
    await page.evaluate(inlineExternalImages);

    // Capture each <canvas>'s live bitmap as a data URI so the snapshot
    // walker can emit it as an <img>. Without this, every chart / scripted
    // graphic on the page (ECharts, Chart.js, etc.) becomes an empty box.
    await page.evaluate(inlineCanvases);

    // `takeSnapshot` is a self-contained IIFE string assembled from the
    // helpers in lib/browser-snapshot.js; `page.evaluate` accepts strings
    // and returns the expression's result.
    const result = await page.evaluate(takeSnapshot);
    if (opts.outputToStdout) {
      // Stdout mode (triggered by `-o -`, used by pagx's `--html-snapshot`
      // integration): the HTML is the sole stdout payload, and the
      // "wrote ..." progress line goes to stderr so the parent process can
      // read stdout as a pure HTML string without parsing.
      process.stdout.write(result.html);
      console.error(`${LOG_PREFIX}wrote stdout (${result.width}x${result.height})`);
    } else {
      fs.writeFileSync(opts.output, result.html, 'utf8');
      console.log(`${LOG_PREFIX}wrote ${opts.output} (${result.width}x${result.height})`);
    }
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
