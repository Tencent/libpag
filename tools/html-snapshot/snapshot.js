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
const { parseArgs } = require('./lib/cli');
const { takeSnapshot, inlineExternalImages } = require('./lib/browser-snapshot');
const { openAndSettlePage } = require('./lib/page-loader');

async function main() {
  const opts = parseArgs(process.argv);

  if (!opts.isUrl && !fs.existsSync(opts.input)) {
    console.error(`html-snapshot: input not found: ${opts.input}`);
    process.exit(1);
  }

  // page-loader expects a fully-qualified URL. Local paths become file:// URLs
  // here so the loader stays scheme-agnostic.
  const targetUrl = opts.isUrl ? opts.input : `file://${opts.input}`;

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--font-render-hinting=none'],
  });
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

    // `takeSnapshot` is a self-contained IIFE string assembled from the
    // helpers in lib/browser-snapshot.js; `page.evaluate` accepts strings
    // and returns the expression's result.
    const result = await page.evaluate(takeSnapshot);
    fs.writeFileSync(opts.output, result.html, 'utf8');
    console.log(`html-snapshot: wrote ${opts.output} (${result.width}x${result.height})`);
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
