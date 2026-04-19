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
 * and a single Chromium process handles them sequentially. Chromium launch is the dominant
 * cost (~2-4s) so batching a large suite cuts runtime by an order of magnitude and removes
 * the cold-start races that caused sporadic `Page.captureScreenshot timed out` errors.
 *
 * Prerequisites: npx puppeteer browsers install chrome
 */

const puppeteer = require('puppeteer');
const path = require('path');
const fs = require('fs');

const SINGLE_TASK_TIMEOUT_MS = 60000;
const PROTOCOL_TIMEOUT_MS = 180000;
const MAX_RETRIES = 2;

async function captureOne(browser, task) {
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

async function captureWithRetry(browser, task, label) {
  let lastError;
  for (let attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    try {
      await captureOne(browser, task);
      if (attempt > 0) {
        console.error(`[screenshot] ${label}: succeeded on retry ${attempt}`);
      }
      return true;
    } catch (err) {
      lastError = err;
      console.error(`[screenshot] ${label}: attempt ${attempt + 1} failed: ${err.message}`);
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

  const browser = await puppeteer.launch({
    headless: true,
    protocolTimeout: PROTOCOL_TIMEOUT_MS,
    args: ['--no-sandbox', '--disable-setuid-sandbox', '--hide-scrollbars'],
  });

  let failures = 0;
  try {
    for (let i = 0; i < parsed.tasks.length; i++) {
      const task = parsed.tasks[i];
      const label = path.basename(task.html || '') + ` (${i + 1}/${parsed.tasks.length})`;
      const ok = await captureWithRetry(browser, task, label);
      if (!ok) {
        failures++;
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
