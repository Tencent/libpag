#!/usr/bin/env node
/**
 * svg-frames
 *
 * Render every frame of an SVG SMIL animation in headless Chromium and save PNGs.
 * This is the "ground truth" side of the svg-vs-pagx comparison: the browser is the
 * reference renderer for SMIL animation, while PAGX frames come from `pagx render-frames`.
 *
 * The SVG document clock is driven per-frame via SVGSVGElement.setCurrentTime(t), so the
 * animation is sampled at deterministic instants instead of relying on wall-clock playback.
 *
 * Usage:
 *   node svg-frames.js --input in.svg --output frames/ [--fps 60]
 */

'use strict';

const fs = require('fs');
const path = require('path');
const puppeteer = require(path.join(__dirname, '../html-snapshot/node_modules/puppeteer'));

function parseArgs(argv) {
  const opts = { input: '', output: 'svg_frames', fps: 0 };
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--input' || arg === '-i') opts.input = argv[++i];
    else if (arg === '--output' || arg === '-o') opts.output = argv[++i];
    else if (arg === '--fps') opts.fps = parseFloat(argv[++i]);
    else if (arg === '--help' || arg === '-h') {
      console.log('Usage: node svg-frames.js --input in.svg --output frames/ [--fps 60]');
      process.exit(0);
    }
  }
  if (!opts.input) {
    console.error('svg-frames: missing --input');
    process.exit(1);
  }
  return opts;
}

// Parse an SMIL clock value (e.g. "2s", "1.5s", "250ms") into seconds. Returns 0 for
// empty/unparseable values. Duplicated inside the page.evaluate below because browser
// context code cannot see Node-scope functions.
function parseClockSeconds(str) {
  if (!str) return 0;
  const m = /^\s*([0-9]*\.?[0-9]+)\s*(ms|s|min|h)?\s*$/.exec(str);
  if (!m) return 0;
  const value = parseFloat(m[1]);
  const unit = m[2] || 's';
  if (unit === 'ms') return value / 1000;
  if (unit === 'min') return value * 60;
  if (unit === 'h') return value * 3600;
  return value;
}

async function main() {
  const opts = parseArgs(process.argv);
  const inputPath = path.resolve(opts.input);
  if (!fs.existsSync(inputPath)) {
    console.error(`svg-frames: input not found: ${opts.input}`);
    process.exit(1);
  }

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--font-render-hinting=none'],
  });

  try {
    const page = await browser.newPage();
    await page.goto(`file://${inputPath}`, { waitUntil: 'load' });

    // Read geometry and duration from the in-page SVG. The helpers are inlined because
    // browser-context code has no access to Node-scope functions.
    const info = await page.evaluate(() => {
      const parseClockSeconds = (str) => {
        if (!str) return 0;
        const m = /^\s*([0-9]*\.?[0-9]+)\s*(ms|s|min|h)?\s*$/.exec(str);
        if (!m) return 0;
        const value = parseFloat(m[1]);
        const unit = m[2] || 's';
        if (unit === 'ms') return value / 1000;
        if (unit === 'min') return value * 60;
        if (unit === 'h') return value * 3600;
        return value;
      };
      const svg = document.querySelector('svg');
      if (!svg) return null;
      const rect = svg.getBoundingClientRect();
      const animated = svg.querySelectorAll('animate, animateTransform, animateMotion, set');
      let maxEnd = 0;
      let hasDur = false;
      for (const el of animated) {
        const dur = parseClockSeconds(el.getAttribute('dur'));
        const begin = parseClockSeconds(el.getAttribute('begin'));
        if (el.getAttribute('dur')) hasDur = true;
        const end = begin + (dur > 0 ? dur : 0);
        if (end > maxEnd) maxEnd = end;
      }
      return {
        width: Math.round(rect.width),
        height: Math.round(rect.height),
        left: rect.left,
        top: rect.top,
        duration: hasDur ? maxEnd : 2,
      };
    });
    if (!info) {
      console.error('svg-frames: no <svg> element found');
      process.exit(1);
    }
    if (info.width <= 0 || info.height <= 0) {
      console.error('svg-frames: invalid SVG dimensions');
      process.exit(1);
    }

    const fps = opts.fps > 0 ? opts.fps : 60;
    const totalFrames = Math.round(info.duration * fps);

    fs.mkdirSync(opts.output, { recursive: true });

    for (let f = 0; f <= totalFrames; f++) {
      const t = f / fps;
      await page.evaluate((seconds) => {
        const svg = document.querySelector('svg');
        svg.setCurrentTime(seconds);
      }, t);
      const name = `frame_${String(f).padStart(3, '0')}.png`;
      const outPath = path.join(opts.output, name);
      await page.screenshot({
        path: outPath,
        clip: { x: info.left, y: info.top, width: info.width, height: info.height },
      });
    }

    console.log(`svg-frames: wrote ${totalFrames + 1} frames to ${opts.output}`);
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
