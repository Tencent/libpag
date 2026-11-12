#!/usr/bin/env node
/**
 * SVG browser-fidelity gate (D6).
 *
 * Pipeline: pagx export -> Chromium screenshot -> pagx render reference -> pixel metrics.
 * The environment is fixed: Playwright's bundled Chromium, viewport 1400x900, white background,
 * deviceScaleFactor 1. Pixel comparison uses pixelmatch threshold 0.1 (shared with the HTML eval).
 *
 * Usage:
 *   node test/svg_browser_gate.js --pagx <file.pagx> [--pagx-bin <path>] [--out <dir>]
 *        [--time <seconds>] [--max-pixel-diff 0.05] [--max-rgb-delta 10] [--min-ssim 0.4]
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const repoRoot = path.resolve(__dirname, '..');
const { chromium } = require(path.join(repoRoot, 'tools/html-snapshot/node_modules/playwright'));
const { pixelMetrics } = require(path.join(repoRoot, 'tools/html-snapshot/eval/compare.js'));

function parseArgs(argv) {
  const args = {
    pagx: '',
    pagxBin: path.join(repoRoot, 'cmake-build-debug/pagx'),
    outDir: path.join(repoRoot, 'test/out/svg-browser-gate'),
    time: null,
    maxPixelDiff: 0.05,
    maxRgbDelta: 10,
    minSsim: 0.4,
  };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--pagx' && i + 1 < argv.length) {
      args.pagx = path.resolve(argv[++i]);
    } else if (arg === '--pagx-bin' && i + 1 < argv.length) {
      args.pagxBin = path.resolve(argv[++i]);
    } else if (arg === '--out' && i + 1 < argv.length) {
      args.outDir = path.resolve(argv[++i]);
    } else if (arg === '--time' && i + 1 < argv.length) {
      args.time = Number(argv[++i]);
    } else if (arg === '--max-pixel-diff' && i + 1 < argv.length) {
      args.maxPixelDiff = Number(argv[++i]);
    } else if (arg === '--max-rgb-delta' && i + 1 < argv.length) {
      args.maxRgbDelta = Number(argv[++i]);
    } else if (arg === '--min-ssim' && i + 1 < argv.length) {
      args.minSsim = Number(argv[++i]);
    }
  }
  return args;
}

function runPagx(args, commandArgs) {
  try {
    execFileSync(args.pagxBin, commandArgs, { stdio: ['ignore', 'pipe', 'pipe'] });
  } catch (error) {
    const stderr = error.stderr ? error.stderr.toString() : '';
    throw new Error(`pagx failed (${commandArgs.join(' ')}): ${stderr || error.message}`);
  }
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (!args.pagx) {
    console.error('svg_browser_gate: --pagx is required');
    process.exit(2);
  }
  fs.mkdirSync(args.outDir, { recursive: true });
  const stem = path.basename(args.pagx, path.extname(args.pagx));
  const svgPath = path.join(args.outDir, `${stem}.svg`);
  const shotPath = path.join(args.outDir, `${stem}.browser.png`);
  const refPath = path.join(args.outDir, `${stem}.pagx.png`);
  const diffPath = path.join(args.outDir, `${stem}.diff.png`);
  const resultPath = path.join(args.outDir, `${stem}.result.json`);

  runPagx(args, ['export', '--input', args.pagx, '--format', 'svg', '--output', svgPath]);
  const renderArgs = ['render', args.pagx, '--output', refPath, '--background', '#FFFFFF', '--scale', '1'];
  if (args.time !== null && Number.isFinite(args.time)) {
    renderArgs.push('--time', String(args.time));
  }
  runPagx(args, renderArgs);

  const bundledChromium = chromium.executablePath();
  const launchOptions = { headless: true };
  if (fs.existsSync(bundledChromium)) {
    launchOptions.executablePath = bundledChromium;
  } else {
    // Keep the gate usable on machines without Playwright's bundled browser. The version is then
    // host-dependent; CI should run `npx playwright install chromium` for a fixed build.
    launchOptions.channel = 'chrome';
  }
  const browser = await chromium.launch(launchOptions);
  let box = null;
  try {
    const page = await browser.newPage({
      viewport: { width: 1400, height: 900 },
      deviceScaleFactor: 1,
    });
    await page.goto('file://' + svgPath, { waitUntil: 'domcontentloaded' });
    // Pause before the timeline can run to completion: seeking a filled transform animation
    // backward after it finished leaves Chrome's painted output stale even though getScreenCTM
    // already reports the new transform.
    await page.evaluate(() => {
      const svg = document.querySelector('svg');
      if (svg && typeof svg.pauseAnimations === 'function') {
        svg.pauseAnimations();
        svg.setCurrentTime(0);
      }
    });
    await page.evaluate(() => {
      document.documentElement.style.background = '#fff';
      document.documentElement.style.margin = '0';
      if (document.body) {
        document.body.style.background = '#fff';
        document.body.style.margin = '0';
      }
      const svg = document.querySelector('svg');
      if (svg) {
        svg.style.background = '#fff';
        svg.style.display = 'block';
      }
    });
    await page.evaluate(async () => {
      if (document.fonts && document.fonts.ready) {
        await document.fonts.ready;
      }
      const svg = document.querySelector('svg');
      if (svg && typeof svg.pauseAnimations === 'function') {
        svg.pauseAnimations();
      }
    });
    if (args.time !== null && Number.isFinite(args.time)) {
      await page.evaluate((time) => {
        const svg = document.querySelector('svg');
        if (svg && typeof svg.setCurrentTime === 'function') {
          svg.setCurrentTime(time);
        }
      }, args.time);
      await page.waitForTimeout(100);
    }
    box = await page.evaluate(() => {
      const svg = document.querySelector('svg');
      if (!svg) {
        return null;
      }
      const rect = svg.getBoundingClientRect();
      return { x: rect.x, y: rect.y, width: rect.width, height: rect.height };
    });
    if (box === null || box.width <= 0 || box.height <= 0) {
      throw new Error('rendered SVG has no measurable bounds');
    }
    await page.screenshot({
      path: shotPath,
      clip: {
        x: Math.max(0, Math.floor(box.x)),
        y: Math.max(0, Math.floor(box.y)),
        width: Math.max(1, Math.ceil(box.width)),
        height: Math.max(1, Math.ceil(box.height)),
      },
      omitBackground: false,
    });
  } finally {
    await browser.close();
  }

  const metrics = pixelMetrics(refPath, shotPath, diffPath);
  const result = {
    pagx: args.pagx,
    svg: svgPath,
    browserScreenshot: shotPath,
    reference: refPath,
    diff: diffPath,
    time: args.time,
    box,
    metrics,
    thresholds: {
      maxPixelDiff: args.maxPixelDiff,
      maxRgbDelta: args.maxRgbDelta,
      minSsim: args.minSsim,
    },
  };
  const passed =
    metrics.pixelDiffRatio <= args.maxPixelDiff &&
    metrics.meanRgbDelta <= args.maxRgbDelta &&
    metrics.ssim >= args.minSsim;
  result.passed = passed;
  fs.writeFileSync(resultPath, JSON.stringify(result, null, 2));
  console.log(JSON.stringify(result, null, 2));
  process.exit(passed ? 0 : 1);
}

main().catch((error) => {
  console.error('svg_browser_gate: ' + error.message);
  process.exit(2);
});
