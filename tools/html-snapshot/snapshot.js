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

async function main() {
  const opts = parseArgs(process.argv);

  if (!fs.existsSync(opts.input)) {
    console.error(`html-snapshot: input not found: ${opts.input}`);
    process.exit(1);
  }

  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--font-render-hinting=none'],
  });
  try {
    const page = await browser.newPage();
    await page.setViewport({
      width: opts.viewportWidth,
      height: opts.viewportHeight,
      deviceScaleFactor: 1,
    });
    page.on('console', (msg) => {
      if (msg.type() === 'error') {
        console.error(`page error: ${msg.text()}`);
      }
    });
    page.on('pageerror', (err) => {
      console.error(`page exception: ${err.message}`);
    });

    const url = 'file://' + opts.input;
    await page.goto(url, { waitUntil: 'networkidle0', timeout: 30000 });

    if (opts.selector) {
      await page.waitForSelector(opts.selector, { timeout: 15000 });
    } else {
      // Heuristic: many React-CDN apps mount into <div id="root">. Wait for at least
      // one child if that root exists.
      try {
        await page.waitForFunction(
          'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true',
          { timeout: 10000 },
        );
      } catch (_) { /* not fatal */ }
    }

    if (opts.waitMs > 0) {
      await new Promise((r) => setTimeout(r, opts.waitMs));
    }

    // PAGX's renderer can read `data:` URIs and local files but not `http(s)://`
    // URLs. Inline every external image into a base64 data URI here so the
    // downstream `pagx render` step can decode them directly. We stash the result
    // on `data-snapshot-src` rather than mutating `src` so that the live layout
    // (which has already settled around the original image) is left untouched.
    await page.evaluate(inlineExternalImages);

    const result = await page.evaluate(takeSnapshot, { /* config placeholder */ });
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
