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
 * period (delay + duration) across the page's animations, including GSAP / anime.js timelines that
 * never surface in `document.getAnimations()` — so a page mixing a 2s and a 4s loop is sampled over
 * the full 4s and every animation is exercised. The chosen times (in seconds) are
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
// Shared "clock": the same global-duration measurement, deterministic seek, and
// pause init-script the animation *capture* (lib/animation-capture.ts) uses, so
// the baseline and the imported PAGX timeline are sampled on one clock. These
// are browser-side functions injected via `page.evaluate(fn, arg)`.
const {
  pagxMeasureGlobalDurationMs,
  pagxCaptureAnimeInstances,
  pagxSeekAllToTime,
  pagxSampleTimesMs,
  PAGX_ANIM_PAUSE_INIT_SCRIPT,
} = require('../dist/lib/animation-capture');

const fail = makeFail('baseline-frames');

// Wait at least one display frame (≈ 16 ms at 60 Hz) plus a small margin for the
// engine's compositor to flush before screenshotting. Headless Chromium's
// `screenshot()` reads from the latest committed frame; without a settle the
// shot would race the seek and intermittently capture the previous frame's
// pixels. 60 ms is a comfortable two-frame budget that has held across the eval
// corpus; keep it as a named constant so the rationale is obvious to future
// edits.
const SEEK_SETTLE_MS = 60;

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

// Read the body rect that snapshot.js will use as the canvas. Must match the canvas size
// browser-snapshot.ts (snapshot.js) records, otherwise the baseline frames and the PAGX render
// land on different-sized PNGs and the per-frame diff is shifted before comparison even starts.
//
// The snapshot path measures the body AFTER cancelling every running CSS / WAAPI animation
// (browser-snapshot.ts → snapshotMain), so its rect reflects the "base" (un-animated) layout.
// This path runs with BASELINE_PAUSE_INIT_SCRIPT installed, which keeps every animation paused
// at its current playback time so the seek loop later can drive `currentTime` deterministically.
// At t=0 that pause exposes the 0% keyframe state of every animation: an animation whose 0%
// keyframe pushes a child off the body via `transform: translateY(...)` (or any other
// position-shifting property) inflates `body.scrollHeight` by the offset. The PAGX side never
// applies that offset to its canvas, so the two PNGs come out different sizes.
//
// Fix: temporarily suppress every animation / transition / transform via `!important` author
// rules during measurement. Author-important rules outrank both CSS animations and WAAPI
// `element.animate()` effects (cascade origin: author-important > animation > author), so a
// WAAPI animation that wrote `transform: translateY(80px)` is overridden back to `none` for the
// duration of the read, and `body.scrollHeight` reflects the base layout the snapshot path also
// records. Removing the override afterwards restores the paused / WAAPI state untouched —
// `currentTime` and the Animation objects survive — so the subsequent seek loop still works.
async function captureBodyRect(page) {
  return page.evaluate(() => {
    const body = document.body;
    body.style.margin = '0';
    body.style.padding = '0';
    if (typeof window.scrollTo === 'function') {
      try { window.scrollTo(0, 0); } catch (_) { /* ignore */ }
    }
    const measureStyle = document.createElement('style');
    measureStyle.textContent =
      '*, *::before, *::after { animation: none !important; ' +
      'transition: none !important; transform: none !important; }';
    const parent = document.head || document.documentElement;
    if (parent) parent.appendChild(measureStyle);
    void body.offsetHeight;
    const rect = body.getBoundingClientRect();
    const width = Math.max(body.scrollWidth, Math.round(rect.width));
    const height = Math.max(body.scrollHeight, Math.round(rect.height));
    if (measureStyle.parentNode) measureStyle.parentNode.removeChild(measureStyle);
    void body.offsetHeight;
    return { width, height };
  });
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
      // Pause every CSS / WAAPI animation as soon as it is created so finite
      // animations cannot run to completion during settle (and thus disappear
      // from `document.getAnimations()` before the seek loop sees them). The
      // capture pipeline installs the identical script (lib/snapshot-runner.ts),
      // so both sides observe the same suspended animations. See
      // PAGX_ANIM_PAUSE_INIT_SCRIPT (lib/animation-clock.ts) for the rationale.
      initScripts: [PAGX_ANIM_PAUSE_INIT_SCRIPT],
      onPageError: (err) => {
        console.error(`page exception: ${err.message}`);
      },
    });

    const { width, height } = await captureBodyRect(page);
    await setViewport(page, engine, { width, height, deviceScaleFactor: 1 });

    // Measure + seek via the shared clock (lib/animation-clock.ts) so the
    // baseline and the animation capture sample on one timeline. The functions
    // are self-contained browser-side code injected with `page.evaluate(fn)`.
    const globalDurationMs = await page.evaluate(pagxMeasureGlobalDurationMs);
    await page.evaluate(pagxCaptureAnimeInstances);
    const timesMs = pagxSampleTimesMs(globalDurationMs, opts.samples);

    const samples = [];
    for (let i = 0; i < timesMs.length; i++) {
      await page.evaluate(pagxSeekAllToTime, timesMs[i]);
      // Give the engine a frame to flush the seeked styles before the screenshot.
      await new Promise((r) => setTimeout(r, SEEK_SETTLE_MS));
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
