#!/usr/bin/env node
/**
 * baseline-frames.js — render the original HTML in headless Chromium and capture a *sequence* of
 * pixel-accurate PNGs, one per animation sample time. These frames are the animation ground truth
 * the eval-animation pipeline diffs the `pagx render --time` frames against.
 *
 * The static `eval/baseline.js` captures a single frame; this captures N frames by seeking the
 * page's animations to deterministic playback times before each screenshot. The seek mirrors the
 * Web Animations bridge `lib/animation-capture.ts` uses for capture: pause every animation reported
 * by `document.getAnimations()` and set its `currentTime`. CSS `@keyframes`, WAAPI and Motion One
 * all surface there, so the same time points the snapshot captures are the time points we seek to.
 *
 * The sample grid is derived from a *global* timeline duration — the longest single-iteration
 * period (delay + duration) across the page's animations — so a page mixing a 2s and a 4s loop is
 * sampled over the full 4s and every animation is exercised. The chosen times (in seconds) are
 * written to `samples.json` so `run.js` can pass the identical values to `pagx render --time`,
 * keeping the baseline and the PAGX render on the same clock.
 *
 * Output (into the directory given by -o):
 *   frame-000.png ... frame-00N.png   one screenshot per sample, cropped to the body rect
 *   samples.json                      { width, height, globalDurationMs, samples: [{ index, timeSec }] }
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { openAndSettlePage } = require('../dist/lib/page-loader');
const { makeFail, parseNumber } = require('../dist/lib/common');
const { launchBrowser, resolveEngine, setViewport } = require('../dist/lib/browser-engine');

const fail = makeFail('baseline-frames');

function parseArgs(argv) {
  const opts = {
    input: '',
    outDir: '',
    samples: 5,
    viewportWidth: 1400,
    viewportHeight: 900,
    waitMs: 800,
    selector: '',
    browserEngine: resolveEngine(undefined),
  };
  const positional = [];
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-o' || a === '--out-dir') {
      opts.outDir = argv[++i];
    } else if (a === '--samples') {
      opts.samples = parseNumber('--samples', argv[++i], { min: 1, fail });
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
    console.error('Usage: baseline-frames.js <input.html> -o <out-dir> [--samples N]');
    process.exit(2);
  }
  opts.input = path.resolve(positional[0]);
  if (!opts.outDir) {
    fail('-o <out-dir> is required');
  }
  opts.outDir = path.resolve(opts.outDir);
  return opts;
}

// Read the body rect that snapshot.js will use as the canvas (identical to eval/baseline.js).
// Anything outside this rect is not addressable in the subset, so cropping the baseline frames to
// it keeps the per-frame diff fair.
async function captureBodyRect(page) {
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

// Longest single-iteration period (delay + duration) in ms across every animation the page reports.
// One period is enough: looping animations realign every period, so sampling [0, period] covers a
// full visual cycle and matches how `pagx render --time` loops the imported timelines.
async function measureGlobalDurationMs(page) {
  return page.evaluate(() => {
    let anims = [];
    try {
      anims = document.getAnimations ? document.getAnimations() : [];
    } catch (_) {
      anims = [];
    }
    let maxMs = 0;
    for (const a of anims) {
      let ct = null;
      try {
        ct = a.effect && a.effect.getComputedTiming ? a.effect.getComputedTiming() : null;
      } catch (_) {
        ct = null;
      }
      if (!ct) continue;
      const dur = typeof ct.duration === 'number' && isFinite(ct.duration) ? ct.duration : 0;
      if (dur <= 0) continue;
      const delay = typeof ct.delay === 'number' && isFinite(ct.delay) ? ct.delay : 0;
      const end = delay + dur;
      if (end > maxMs) maxMs = end;
    }
    return maxMs;
  });
}

// Pause every animation and pin its playback time to `timeMs`. Setting `currentTime` on a WAAPI
// animation honours its own delay / iteration / direction, so a single absolute time seeks all
// animations to a consistent frame regardless of their individual periods.
async function seekToTime(page, timeMs) {
  await page.evaluate((t) => {
    let anims = [];
    try {
      anims = document.getAnimations ? document.getAnimations() : [];
    } catch (_) {
      anims = [];
    }
    for (const a of anims) {
      try {
        a.pause();
        a.currentTime = t;
      } catch (_) {
        /* skip this animation */
      }
    }
  }, timeMs);
}

function sampleTimesMs(globalMs, samples) {
  if (!(globalMs > 0) || samples <= 1) return [0];
  const out = [];
  for (let i = 0; i < samples; i++) {
    out.push((i / (samples - 1)) * globalMs);
  }
  return out;
}

async function main() {
  const opts = parseArgs(process.argv);
  if (!fs.existsSync(opts.input)) {
    fail(`input not found: ${opts.input}`, 1);
  }
  fs.mkdirSync(opts.outDir, { recursive: true });

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
    await setViewport(page, engine, { width, height, deviceScaleFactor: 1 });

    const globalDurationMs = await measureGlobalDurationMs(page);
    const timesMs = sampleTimesMs(globalDurationMs, opts.samples);

    const samples = [];
    for (let i = 0; i < timesMs.length; i++) {
      await seekToTime(page, timesMs[i]);
      // Give the engine a frame to flush the seeked styles before the screenshot.
      await new Promise((r) => setTimeout(r, 60));
      const framePath = path.join(opts.outDir, `frame-${String(i).padStart(3, '0')}.png`);
      await page.screenshot({
        path: framePath,
        type: 'png',
        clip: { x: 0, y: 0, width, height },
        omitBackground: false,
      });
      samples.push({ index: i, timeSec: timesMs[i] / 1000 });
    }

    fs.writeFileSync(
      path.join(opts.outDir, 'samples.json'),
      JSON.stringify({ width, height, globalDurationMs, samples }, null, 2),
      'utf8',
    );
    console.log(
      `baseline-frames: wrote ${samples.length} frame(s) to ${opts.outDir} ` +
      `(${width}x${height}, globalDuration=${globalDurationMs.toFixed(0)}ms)`,
    );
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
