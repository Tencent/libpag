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
const puppeteer = require('puppeteer');
const { openAndSettlePage } = require('../lib/page-loader');

function parseArgs(argv) {
  const opts = {
    input: '',
    output: '',
    viewportWidth: 1400,
    viewportHeight: 900,
    waitMs: 800,
    selector: '',
  };
  const positional = [];
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-o' || a === '--output') {
      opts.output = argv[++i];
    } else if (a === '--viewport-width') {
      opts.viewportWidth = Number(argv[++i]);
    } else if (a === '--viewport-height') {
      opts.viewportHeight = Number(argv[++i]);
    } else if (a === '--wait-ms') {
      opts.waitMs = Number(argv[++i]);
    } else if (a === '--selector') {
      opts.selector = argv[++i];
    } else if (a.startsWith('-')) {
      console.error(`baseline: unknown option '${a}'`);
      process.exit(2);
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
    console.error(`baseline: input not found: ${opts.input}`);
    process.exit(1);
  }

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--font-render-hinting=none'],
  });
  try {
    const page = await openAndSettlePage(browser, `file://${opts.input}`, {
      viewportWidth: opts.viewportWidth,
      viewportHeight: opts.viewportHeight,
      waitMs: opts.waitMs,
      selector: opts.selector,
      onPageError: (err) => {
        console.error(`page exception: ${err.message}`);
      },
    });
    const { width, height } = await captureBodyRect(page);
    // Re-set the viewport to the captured size so the screenshot is taken at
    // the canvas dimensions (matches `pagx render --scale 1` output size).
    await page.setViewport({ width, height, deviceScaleFactor: 1 });
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
