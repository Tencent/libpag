/**
 * Puppeteer screenshot script for PAGX-to-HTML visual comparison.
 *
 * Two invocation modes:
 *
 *   Single:  node screenshot.js <html> <png> <width> <height> [scale]
 *   Batch:   node screenshot.js --batch <tasks.json>
 *
 * In batch mode the input file contains a JSON array of
 *   [{ html, png, width, height, scale }]
 * and Chromium is reused across captures but rolled every RESTART_EVERY tasks (and on any
 * fatal protocol error) to stay ahead of headless Chromium's shared-memory / GPU-context leak
 * that otherwise kills the process around 50-70 pages in. Chromium launch is the dominant
 * per-capture cost (~2-4s) so batching still cuts runtime by an order of magnitude versus
 * launching per task, while the periodic roll absorbs the "Connection closed" bursts that
 * used to sink the second half of the suite on macOS.
 *
 * Dependencies are auto-installed on first run via `npm ci`.
 */

const path = require('path');
const fs = require('fs');
const { execSync } = require('child_process');

// Auto-install puppeteer if node_modules is missing (fresh clone without manual npm ci).
const nodeModulesDir = path.join(__dirname, 'node_modules', 'puppeteer');
if (!fs.existsSync(nodeModulesDir)) {
  console.log('puppeteer not found, running npm ci...');
  execSync('npm ci', { cwd: __dirname, stdio: 'inherit' });
}

const puppeteer = require('puppeteer');

const SINGLE_TASK_TIMEOUT_MS = 60000;
const PROTOCOL_TIMEOUT_MS = 180000;
const MAX_RETRIES = 2;
// Roll the Chromium process every N captures. Long-running headless Chromium on macOS leaks
// shared memory / GPU context state and starts returning "Connection closed" / "Target closed"
// after roughly 50-70 pages, which the per-task retry loop then cannot recover from because
// the entire browser is dead. Restarting on a fixed cadence well below the empirical failure
// floor keeps the batch self-healing even on systems where Chromium would otherwise asphyxiate
// midway through.
const RESTART_EVERY = 30;
// Substrings in error messages that indicate the browser as a whole is unusable, not just the
// current page. Encountering any of these forces an immediate restart instead of burning the
// remaining retry budget on a dead process.
const FATAL_ERROR_PATTERNS = [
  'Connection closed',
  'Target closed',
  'frame was detached',
  'Browser closed',
];

async function captureShared(page, task) {
  const { html, png, width, height, scale } = task;
  await page.setViewport({
    width,
    height,
    deviceScaleFactor: scale || 1,
  });
  const fileUrl = 'file://' + path.resolve(html);
  await page.goto(fileUrl, { waitUntil: 'load', timeout: SINGLE_TASK_TIMEOUT_MS });
  await page.evaluate(() => document.fonts.ready);
  await page.screenshot({
    path: path.resolve(png),
    type: 'png',
    omitBackground: true,
    clip: { x: 0, y: 0, width, height },
    timeout: SINGLE_TASK_TIMEOUT_MS,
  });
}

function isFatalBrowserError(err) {
  const msg = err && err.message ? err.message : String(err);
  return FATAL_ERROR_PATTERNS.some((pattern) => msg.includes(pattern));
}

async function launchBrowser() {
  return puppeteer.launch({
    headless: true,
    protocolTimeout: PROTOCOL_TIMEOUT_MS,
    // The flag set deliberately targets headless CI containers as well as local macOS:
    //   --no-sandbox / --disable-setuid-sandbox — required when running as root in Docker,
    //     and a no-op on a normal desktop user account.
    //   --hide-scrollbars — avoids scrollbar gutters stealing viewport pixels.
    //   --disable-dev-shm-usage — Docker defaults /dev/shm to 64 MB, which is not enough
    //     for Chromium's shared memory; switching to /tmp prevents the "Target closed"
    //     bursts that otherwise hit after a handful of pages. Harmless on macOS.
    //   --disable-gpu — no GPU in headless CI; skipping the probe trims launch latency
    //     and sidesteps driver-specific hangs.
    //   --font-render-hinting=none — disables hinting-based glyph adjustments that vary
    //     between CI machines for reproducible screenshots.
    args: [
      '--no-sandbox',
      '--disable-setuid-sandbox',
      '--hide-scrollbars',
      '--disable-dev-shm-usage',
      '--disable-gpu',
      '--font-render-hinting=none',
    ],
  });
}

async function captureWithRetry(state, task, label) {
  let lastError;
  for (let attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    try {
      await captureShared(state.sharedPage, task);
      if (attempt > 0) {
        console.error(`[screenshot] ${label}: succeeded on retry ${attempt}`);
      }
      return true;
    } catch (err) {
      lastError = err;
      console.error(`[screenshot] ${label}: attempt ${attempt + 1} failed: ${err.message}`);
      // If Chromium itself is gone, burning the remaining retries against the same dead
      // process is pointless. Restart before the next attempt.
      if (isFatalBrowserError(err) && attempt < MAX_RETRIES) {
        console.error(`[screenshot] ${label}: browser appears dead, restarting Chromium`);
        await state.restartBrowser();
      }
    }
  }
  console.error(`[screenshot] ${label}: giving up after ${MAX_RETRIES + 1} attempts`);
  console.error(lastError && lastError.stack ? lastError.stack : String(lastError));
  return false;
}

function parseArgs(argv) {
  if (argv[0] === '--batch') {
    if (argv.length < 2) {
      throw new Error('--batch requires a tasks.json path');
    }
    const tasks = JSON.parse(fs.readFileSync(argv[1], 'utf8'));
    if (!Array.isArray(tasks)) {
      throw new Error('tasks.json must contain a JSON array');
    }
    return { mode: 'batch', tasks };
  }
  if (argv.length < 4) {
    throw new Error('Usage: node screenshot.js <html> <png> <width> <height> [scale]');
  }
  const [htmlFile, outputPng, widthStr, heightStr, scaleStr] = argv;
  return {
    mode: 'single',
    tasks: [
      {
        html: htmlFile,
        png: outputPng,
        width: parseInt(widthStr, 10),
        height: parseInt(heightStr, 10),
        scale: scaleStr ? parseInt(scaleStr, 10) : 1,
      },
    ],
  };
}

(async () => {
  let parsed;
  try {
    parsed = parseArgs(process.argv.slice(2));
  } catch (err) {
    console.error(err.message);
    process.exit(1);
  }

  let browser = await launchBrowser();
  let sharedPage = await browser.newPage();
  let capturesSinceLaunch = 0;
  const state = {
    get sharedPage() { return sharedPage; },
    restartBrowser: async () => {
      await browser.close().catch(() => {});
      browser = await launchBrowser();
      sharedPage = await browser.newPage();
      capturesSinceLaunch = 0;
    },
  };

  let failures = 0;
  try {
    for (let i = 0; i < parsed.tasks.length; i++) {
      const task = parsed.tasks[i];
      const label = path.basename(task.html || '') + ` (${i + 1}/${parsed.tasks.length})`;
      const ok = await captureWithRetry(state, task, label);
      if (!ok) {
        failures++;
      }
      capturesSinceLaunch++;
      // Scheduled roll: restart the browser every RESTART_EVERY captures to stay ahead of the
      // macOS Chromium leak curve. Skipped after the last task since the browser closes next.
      if (i + 1 < parsed.tasks.length && capturesSinceLaunch >= RESTART_EVERY) {
        console.error(
            `[screenshot] rolling Chromium after ${capturesSinceLaunch} captures (scheduled)`);
        await state.restartBrowser();
      }
    }
  } finally {
    await browser.close().catch(() => {});
  }

  if (parsed.mode === 'batch') {
    console.error(
        `[screenshot] batch complete: ${parsed.tasks.length - failures}/${parsed.tasks.length} ` +
        `succeeded`);
  }
  process.exit(failures === 0 ? 0 : 1);
})().catch((err) => {
  console.error(err && err.stack ? err.stack : String(err));
  process.exit(1);
});
