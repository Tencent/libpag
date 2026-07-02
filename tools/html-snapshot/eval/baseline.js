#!/usr/bin/env node
/**
 * baseline.js — render the original HTML in headless Chromium and capture a
 * pixel-accurate PNG that the eval pipeline treats as ground truth.
 *
 * The viewport, wait strategy, and image inlining match snapshot.js so the
 * baseline and the subset render measure the same page state.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { openAndSettlePage } = require('../dist/lib/page-loader');
const { makeFail, parseNumber } = require('../dist/lib/common');
const { launchBrowser, resolveEngine, setViewport } = require('../dist/lib/browser-engine');

const fail = makeFail('baseline');

function parseArgs(argv) {
  const opts = {
    input: '',
    output: '',
    viewportWidth: 1400,
    viewportHeight: 900,
    waitMs: 800,
    selector: '',
    // Headless browser driver — picks up HTML_SNAPSHOT_BROWSER if set, falls
    // back to puppeteer otherwise. Override per-invocation with
    // --browser-engine playwright (matches the snapshot.js flag).
    browserEngine: resolveEngine(undefined),
  };
  const positional = [];
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-o' || a === '--output') {
      opts.output = argv[++i];
    } else if (a === '--viewport-width') {
      opts.viewportWidth = parseNumber('--viewport-width', argv[++i], { min: 1, fail });
    } else if (a === '--viewport-height') {
      opts.viewportHeight = parseNumber('--viewport-height', argv[++i], { min: 1, fail });
    } else if (a === '--wait-ms') {
      opts.waitMs = parseNumber('--wait-ms', argv[++i], { min: 0, fail });
    } else if (a === '--selector') {
      opts.selector = argv[++i];
    } else if (a === '--browser-engine') {
      try { opts.browserEngine = resolveEngine(argv[++i]); }
      catch (err) { fail(err && err.message ? err.message : String(err)); }
    } else if (a.startsWith('-')) {
      fail(`unknown option '${a}'`);
    } else {
      positional.push(a);
    }
  }
  if (positional.length === 0) {
    console.error('Usage: baseline.js <input.html> -o <out.png>');
    process.exit(2);
  }
  opts.input = path.resolve(positional[0]);
  if (!opts.output) {
    const dir = path.dirname(opts.input);
    const base = path.basename(opts.input, path.extname(opts.input));
    opts.output = path.join(dir, `${base}.baseline.png`);
  } else {
    opts.output = path.resolve(opts.output);
  }
  return opts;
}

async function captureBodyRect(page) {
  // Force layout flush and read the body rect that snapshot.js will use as the
  // canvas. Anything outside this rect is not addressable in the subset output,
  // so cropping the baseline screenshot to the same rect keeps the diff fair.
  return page.evaluate(() => {
    const body = document.body;
    body.style.margin = '0';
    body.style.padding = '0';
    if (typeof window.scrollTo === 'function') {
      try { window.scrollTo(0, 0); } catch (_) { /* ignore */ }
    }
    void body.offsetHeight;
    const rect = body.getBoundingClientRect();
    const width = Math.max(body.scrollWidth, Math.round(rect.width));
    const height = Math.max(body.scrollHeight, Math.round(rect.height));
    return { width, height };
  });
}

async function main() {
  const opts = parseArgs(process.argv);
  if (!fs.existsSync(opts.input)) {
    fail(`input not found: ${opts.input}`, 1);
  }

  const engineHandle = await launchBrowser({ engine: opts.browserEngine });
  const { browser, engine } = engineHandle;
  try {
    const page = await openAndSettlePage(engineHandle, `file://${opts.input}`, {
      viewportWidth: opts.viewportWidth,
      viewportHeight: opts.viewportHeight,
      waitMs: opts.waitMs,
      selector: opts.selector,
      onPageError: (err) => {
        console.error(`page exception: ${err.message}`);
      },
    });
    const { width, height } = await captureBodyRect(page);
    // Re-size the viewport to the captured body rect so the screenshot is
    // taken at the canvas dimensions (matches `pagx render --scale 1` output
    // size). `setViewport` is engine-aware: puppeteer keeps the legacy
    // per-page setter, playwright forwards to setViewportSize.
    await setViewport(page, engine, { width, height, deviceScaleFactor: 1 });
    await new Promise((r) => setTimeout(r, 50));
    await page.screenshot({
      path: opts.output,
      type: 'png',
      clip: { x: 0, y: 0, width, height },
      omitBackground: false,
    });
    console.log(`baseline: wrote ${opts.output} (${width}x${height})`);
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
