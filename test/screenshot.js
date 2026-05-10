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
 * On startup the script ensures NotoSansSC-Bold.ttf is present under
 * `test/out/PAGXHtmlTest/fonts/` so the wrapped HTML's Bold @font-face resolves locally
 * instead of hitting Google Fonts on every capture. If the local file is missing or its
 * md5 does not match the expected v40 checksum, the script downloads it from the fonts
 * gstatic URL. When the download fails (offline, CDN degraded, etc.) the script prints a
 * WARNING and falls back to the original slow path (fresh page + `networkidle0`), which
 * keeps rendering correct at the cost of ~2.5 s/capture waiting for Chromium to time out
 * the Bold CDN request. A successful fast path is ~15 x faster (~120 ms/capture).
 *
 * Prerequisites: npx puppeteer browsers install chrome
 */

const puppeteer = require('puppeteer');
const path = require('path');
const fs = require('fs');
const https = require('https');
const crypto = require('crypto');

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

// Google Fonts CDN URL for Noto Sans SC Bold v40. The md5 is the on-disk hash of the TTF
// body as served by that URL — if the CDN ever rotates its asset hashing scheme this will
// mismatch and we must re-generate the baseline. Keeping the version pinned matters because
// the Bold glyph outlines affect every text sample's pixel-exact baseline.
const BOLD_FONT_URL =
    'https://fonts.gstatic.com/s/notosanssc/v40/' +
    'k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaGzjCnYw.ttf';
const BOLD_FONT_MD5 = 'b605bafe45c31a6fe5bcc8984f9e59a7';
const BOLD_FONT_RELPATH = 'NotoSansSC-Bold.ttf';
const FONTS_DIR_RELATIVE_TO_OUT = 'fonts';

function md5File(filePath) {
  const h = crypto.createHash('md5');
  h.update(fs.readFileSync(filePath));
  return h.digest('hex');
}

function download(url, destPath) {
  return new Promise((resolve, reject) => {
    const tmp = destPath + '.download';
    const out = fs.createWriteStream(tmp);
    const request = (loc) => {
      https.get(loc, (res) => {
        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          res.resume();
          return request(new URL(res.headers.location, loc).toString());
        }
        if (res.statusCode !== 200) {
          res.resume();
          out.close(() => fs.unlinkSync(tmp));
          return reject(new Error(`HTTP ${res.statusCode} from ${loc}`));
        }
        res.pipe(out);
        out.on('finish', () => {
          out.close(() => {
            fs.renameSync(tmp, destPath);
            resolve();
          });
        });
      }).on('error', (err) => {
        out.close(() => { try { fs.unlinkSync(tmp); } catch {} });
        reject(err);
      });
    };
    request(url);
  });
}

// Returns the absolute path of the local Bold font if it exists and matches the expected
// md5, downloading it on demand. Returns null on any failure so callers can degrade to the
// slow (CDN-dependent) path without raising.
async function ensureBoldFont(fontsDir) {
  const destPath = path.join(fontsDir, BOLD_FONT_RELPATH);
  try {
    if (fs.existsSync(destPath) && md5File(destPath) === BOLD_FONT_MD5) {
      return destPath;
    }
  } catch (err) {
    console.error(
        `[screenshot] cannot read cached Noto Sans SC Bold v40 at ${destPath}: ${err.message}`);
  }
  fs.mkdirSync(fontsDir, { recursive: true });
  console.error(
      `[screenshot] downloading Noto Sans SC Bold v40 (used by HTML @font-face for ` +
      `font-weight:bold) from Google Fonts CDN to ${destPath}`);
  const tStart = Date.now();
  try {
    await download(BOLD_FONT_URL, destPath);
  } catch (err) {
    console.error(
        `[screenshot] WARNING: Noto Sans SC Bold v40 download failed (${err.message}); ` +
        `falling back to slow capture path (HTML will fetch Bold from CDN on every page load, ` +
        `adding ~1 s per sample). Check network connectivity to fonts.gstatic.com.`);
    return null;
  }
  let md5;
  try {
    md5 = md5File(destPath);
  } catch (err) {
    console.error(
        `[screenshot] WARNING: cannot hash downloaded Noto Sans SC Bold v40 ` +
        `(${err.message}); falling back to slow capture path.`);
    return null;
  }
  if (md5 !== BOLD_FONT_MD5) {
    console.error(
        `[screenshot] WARNING: Noto Sans SC Bold v40 md5 mismatch ` +
        `(got ${md5}, expected ${BOLD_FONT_MD5}); falling back to slow capture path. ` +
        `The CDN may have rotated this asset — delete ${destPath} and re-run to refetch, ` +
        `or update BOLD_FONT_MD5 in this script if the new hash is intentional.`);
    // Leave the bad file in place; a follow-up run will overwrite via the download branch.
    return null;
  }
  console.error(
      `[screenshot] Noto Sans SC Bold v40 cached at ${destPath} ` +
      `(${Math.round((Date.now() - tStart) / 100) / 10} s)`);
  return destPath;
}

// Given the batch tasks, guess the directory that contains `fonts/` so ensureBoldFont can
// cache the Bold font alongside the other bundled fonts. WrapHtmlDocument (C++ side) puts
// every task's HTML next to the fonts/ directory, so the first task's parent directory is
// the canonical location.
function inferFontsDir(tasks) {
  if (!tasks.length) return null;
  const outDir = path.dirname(path.resolve(tasks[0].html));
  return path.join(outDir, FONTS_DIR_RELATIVE_TO_OUT);
}

async function captureFresh(browser, task) {
  const { html, png, width, height, scale } = task;
  const page = await browser.newPage();
  try {
    await page.setViewport({
      width,
      height,
      deviceScaleFactor: scale || 1,
    });
    const fileUrl = 'file://' + path.resolve(html);
    await page.goto(fileUrl, { waitUntil: 'networkidle0', timeout: SINGLE_TASK_TIMEOUT_MS });
    // Wait until every @font-face is resolved so the first paint uses our repo-bundled
    // Noto Sans SC file instead of whatever Chromium substitutes while the font is loading.
    await page.evaluate(() => document.fonts.ready);
    await page.screenshot({
      path: path.resolve(png),
      type: 'png',
      omitBackground: true,
      clip: { x: 0, y: 0, width, height },
      timeout: SINGLE_TASK_TIMEOUT_MS,
    });
  } finally {
    // Always close the page even on failure so Chromium does not accumulate targets across
    // retries; keeping pages around has been observed to starve the DevTools protocol.
    await page.close().catch(() => {});
  }
}

// Fast-path capture: reuses a single shared Page across samples and returns as soon as the
// local resources finish loading. Valid only when the Bold @font-face resolves to a local
// file — otherwise the `load` event waits for Chromium to abort the Bold CDN request, which
// can take tens of seconds per sample.
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
    //     between CI machines. Pairs with the test harness's explicit @font-face rules
    //     to make screenshots reproducible across laptops, CI runners, and reviewers.
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
      if (state.fastPath) {
        await captureShared(state.sharedPage, task);
      } else {
        await captureFresh(state.browser, task);
      }
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

  // Decide up-front whether the Bold font is available locally. If yes, every HTML will
  // hit the local file first in its Bold @font-face src list and we can switch to the fast
  // capture path; if no, the Bold src falls back to the CDN, which means the fast `load`
  // event would stall waiting on gstatic, so we must stay on the slow `networkidle0` path.
  const fontsDir = inferFontsDir(parsed.tasks);
  const fastPath =
      fontsDir !== null && (await ensureBoldFont(fontsDir)) !== null;
  if (parsed.mode === 'batch') {
    console.error(
        `[screenshot] capture mode: ${fastPath ?
            'fast (shared page reused across captures, page.goto waits for `load`)' :
            'fallback (fresh page per capture, page.goto waits for `networkidle0` ' +
                'while Chromium times out the Bold CDN request)'}`);
  }

  let browser = await launchBrowser();
  let sharedPage = fastPath ? await browser.newPage() : null;
  let capturesSinceLaunch = 0;
  const state = {
    get browser() { return browser; },
    get sharedPage() { return sharedPage; },
    fastPath,
    restartBrowser: async () => {
      await browser.close().catch(() => {});
      browser = await launchBrowser();
      sharedPage = fastPath ? await browser.newPage() : null;
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
      if (i + 1 < parsed.tasks.length) {
        if (capturesSinceLaunch >= RESTART_EVERY) {
          console.error(
              `[screenshot] rolling Chromium after ${capturesSinceLaunch} captures (scheduled)`);
          await state.restartBrowser();
        } else if (!fastPath) {
          // Slow-path cooldown: give Chromium a brief window to tear down the just-closed
          // target before the next newPage() starts. Without this cooldown the DevTools
          // protocol occasionally races mid-batch (macOS tends to be the first to show
          // "Target closed" bursts), which the retry loop then papers over at the cost of
          // tens of seconds per flaky sample. Fast path reuses a single page, so no churn
          // and no cooldown is needed.
          await new Promise((resolve) => setTimeout(resolve, 50));
        }
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
