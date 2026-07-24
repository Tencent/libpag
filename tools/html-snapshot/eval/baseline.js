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
const { makeFail, parseNumber, MAX_CAPTURE_HEIGHT_PX } = require('../dist/lib/common');
const { launchBrowser, resolveEngine } = require('../dist/lib/browser-engine');
const { applyCanvasViewport } = require('../dist/lib/canvas-viewport');

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

async function captureBodyRect(page, maxHeightPx) {
  // Force layout flush and read the body rect that snapshot.js will use as the
  // canvas. Anything outside this rect is not addressable in the subset output,
  // so cropping the baseline screenshot to the same rect keeps the diff fair.
  return page.evaluate((maxHeight) => {
    const body = document.body;
    body.style.margin = '0';
    body.style.padding = '0';
    // Pin <body> to the viewport origin. The snapshot's `<html>` carries
    // `padding:32px 0` plus `display:flex; justify-content:center` purely so a
    // browser preview of the subset has breathing room, but `pagx render` roots
    // at `<body>` and ignores the `<html>` box — the subset PNG therefore places
    // body content at (0,0). Left as-is, Chromium would honour the `<html>`
    // padding/centring and push the baseline's body down 32px (and, for a body
    // narrower than the viewport, right by the centring gap), so every diff
    // would be offset. Neutralising the `<html>` box here reproduces pagx's
    // body-rooted framing and — unlike clipping from the body's own origin —
    // keeps the screenshot clip inside the viewport, so it behaves identically
    // on both the puppeteer and playwright engines.
    const de = document.documentElement;
    if (de && de.style) {
      de.style.margin = '0';
      de.style.padding = '0';
      de.style.display = 'block';
    }
    // Mirror prepareBodyForSnapshot (browser-snapshot.ts): force the scroll
    // reset instant so `html { scroll-behavior: smooth }` can't leave the
    // scrollTo(0, 0) below animating. Kept symmetric with the subset so the two
    // "prepare" steps stay mirror images even though this one's outputs (body
    // dimensions + a captureBeyondViewport clip at the document origin) are
    // already scroll-independent.
    if (de && de.style) de.style.setProperty('scroll-behavior', 'auto', 'important');
    if (body && body.style) body.style.setProperty('scroll-behavior', 'auto', 'important');
    if (typeof window.scrollTo === 'function') {
      try { window.scrollTo(0, 0); } catch (_) { /* ignore */ }
    }
    void body.offsetHeight;
    const rect = body.getBoundingClientRect();
    const width = Math.max(body.scrollWidth, Math.round(rect.width));
    // Clamp to the same renderable maximum snapshot.js applies to the subset
    // canvas. Infinite-scroll feeds inflate the body past what a single GL
    // render surface can hold; without this the baseline PNG (ground truth)
    // would be taller than the subset and the diff would compare mismatched
    // canvases. Clamping both to MAX_CAPTURE_HEIGHT_PX keeps them aligned.
    const height = Math.min(
      maxHeight,
      Math.max(body.scrollHeight, Math.round(rect.height)),
    );
    return { width, height };
  }, maxHeightPx);
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
    const { width, height } = await captureBodyRect(page, MAX_CAPTURE_HEIGHT_PX);
    // Resize the viewport to the captured body rect so the screenshot is taken
    // at the canvas dimensions (matches `pagx render --scale 1` output size),
    // then keep it only if it was safe. `applyCanvasViewport` is the SAME guard
    // the subset uses (lib/canvas-viewport.ts): it reverts to the settle
    // viewport for fluid / viewport-driven pages whose `vh` sections balloon or
    // whose scroll-driven content resets under a full-canvas viewport. Sharing
    // it keeps the baseline and subset in lock-step — both resize, or both
    // revert, so the diff compares the same page state.
    await applyCanvasViewport(
      page,
      engine,
      { width, height },
      { width: opts.viewportWidth, height: opts.viewportHeight },
      (msg) => console.error(msg),
    );
    // captureBodyRect neutralised the `<html>` box, so <body> sits at the
    // viewport origin — the same body-rooted framing `pagx render` produces for
    // the subset. Clip at (0,0) to the canvas rect measured before any resize:
    // it equals the subset's emitted canvas in both branches (kept: viewport ==
    // canvas; reverted: the subset re-measures at the settle viewport, which is
    // where this rect was measured). `captureBeyondViewport` lets the reverted
    // case screenshot the full canvas height at the shorter settle viewport
    // WITHOUT resizing the layout back up (which is exactly what would re-balloon
    // a `vh` page).
    await page.screenshot({
      path: opts.output,
      type: 'png',
      clip: { x: 0, y: 0, width, height },
      omitBackground: false,
      // The eval harness supports Puppeteer >= 23.0.0 only (the minimum declared
      // in tools/html-snapshot/package.json). Keep ad-hoc installs at or above
      // that tested floor when using this full-page clipping option.
      captureBeyondViewport: true,
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
