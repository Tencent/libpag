#!/usr/bin/env node
//
// baseline-blank.js — measures the floor cost of opening a blank page in
// headless Chromium, with no html-snapshot work whatsoever.
//
// Why this exists:
//   Every per-case row in `results.jsonl` is dominated by a fixed
//   "browser cold-start" cost: spawn `node`, exec chromium, set up the
//   zygote / GPU / utility processes, parse `about:blank`, then tear
//   the whole tree down. Without a separate floor measurement, it's
//   impossible to tell from the report whether a 1.8s wall-time row is
//   "this page is fast" (close to floor) or "this page is slow" (real
//   work amortised badly across cold start).
//
//   Running this script under the same `sampler.js` wrapper as the
//   real cases gives us a directly-comparable row labelled
//   `__baseline:blank/<n>`, which `summarize.js` / `summarize-html.js`
//   render in their own section above the aggregate stats.
//
// What it does:
//   1. Launch the same browser engine `snapshot.js` uses (puppeteer,
//      via lib/browser-engine.js) with the same flags
//      (`--no-sandbox`, `--font-render-hinting=none`). Resolved through
//      `--snapshot-dir` so the script works both inside the Docker
//      container (snapshot dir = /app) and natively (snapshot dir =
//      tools/html-snapshot).
//   2. Open one page, navigate to `about:blank`, wait for `load`.
//   3. Hold the page open for `--hold-ms` (default 200ms) so the
//      sampler captures a stable peak before teardown — without this
//      a fast machine can finish before sampler.js's first 50ms tick.
//   4. Close the browser.
//
// Anything we change about how snapshot.js launches the browser
// (cached profile, extra flags, different engine) automatically
// reflects in the baseline because both go through the same
// `lib/browser-engine.js` module.
//
// Usage:
//   baseline-blank.js [--snapshot-dir DIR] [--hold-ms N] [--browser-engine NAME]

'use strict';

const path = require('node:path');

const args = process.argv.slice(2);
let snapshotDir = path.resolve(__dirname, '..');
let holdMs = 200;
// `engine` defaults to undefined so launchBrowser() falls through to its
// own resolution chain (HTML_SNAPSHOT_BROWSER env → DEFAULT_ENGINE),
// matching how snapshot.js behaves when --browser-engine is omitted.
let engine;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--snapshot-dir') {
    snapshotDir = path.resolve(args[++i]);
    continue;
  }
  if (args[i] === '--hold-ms') {
    holdMs = Number(args[++i]);
    continue;
  }
  if (args[i] === '--browser-engine') {
    engine = args[++i];
    continue;
  }
  if (args[i] === '-h' || args[i] === '--help') {
    process.stdout.write(
      'usage: baseline-blank.js [--snapshot-dir DIR] [--hold-ms N] '
      + '[--browser-engine puppeteer|playwright]\n',
    );
    process.exit(0);
  }
  // Anything else is a typo or a stale flag — fail fast rather than
  // silently drop it (the previous behaviour caused run-cases.sh's
  // --browser-engine value to be ignored entirely).
  process.stderr.write(`baseline-blank.js: unknown argument '${args[i]}'\n`);
  process.exit(2);
}

// We deliberately re-use snapshot.js's adapter rather than calling
// puppeteer directly: that way `--no-sandbox`, the engine-selection
// logic, and any future launch tweaks land in the baseline number
// without a second source of truth to keep in sync.
const enginePath = path.join(snapshotDir, 'lib', 'browser-engine.js');
const { launchBrowser, newPage } = require(enginePath);

async function main() {
  const handle = await launchBrowser({ engine });
  try {
    const page = await newPage(handle, {
      // The exact viewport doesn't matter for an empty page, but using
      // the same default shape snapshot.js uses keeps the Chromium
      // window-creation path identical between the baseline and the
      // real cases.
      viewport: { width: 1280, height: 800 },
    });
    await page.goto('about:blank', { waitUntil: 'load' });
    if (holdMs > 0) {
      await new Promise((resolve) => setTimeout(resolve, holdMs));
    }
  } finally {
    await handle.browser.close();
  }
  // sampler.js redirects this child's stdout to its stderr, so this
  // line surfaces in the wrapper's logs without polluting results.jsonl.
  console.error('baseline-blank: opened about:blank and closed cleanly');
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
