#!/usr/bin/env node
/**
 * html-snapshot
 *
 * Render an arbitrary HTML page (typically a React/Tailwind/lucide app that materialises
 * the DOM at runtime) in a headless browser and emit a flat, absolute-positioned snapshot
 * that conforms to spec/html_subset.md.
 *
 * This file is the Node-side CLI driver. CLI parsing lives in `lib/cli.js`; the pipeline
 * itself (open page → inline images/canvases/icons → evaluate `takeSnapshot`) is shared
 * with the HTTP service (`server.js`) via `lib/snapshot-runner.js`. The in-page snapshot
 * logic lives in `lib/browser-snapshot.js` and is shipped to Chromium via
 * `page.evaluate(...)`. See those files for details.
 *
 * Pipeline:
 *   1. Launch headless Chromium, navigate to the source file.
 *   2. Wait for the page to settle (networkidle + selector + a short delay).
 *   3. Inline every external <img> as a data: URI so the output is self-contained.
 *   4. Walk the rendered DOM (browser-side) and emit a flat HTML string.
 *   5. Write the resulting <!DOCTYPE html>…</html> string next to the input.
 *
 * The output is intended to be fed directly to `pagx import --format html`. The libpag
 * HTMLSubsetTransformer (src/pagx/html/HTMLSubsetTransformer.cpp) handles the final
 * cleanup (unit conversion, dropping any leftover unsupported properties, etc.).
 */

'use strict';

const fs = require('fs');
const { parseArgs, LOG_PREFIX, errMessage } = require('./lib/cli');
const { launchBrowser } = require('./lib/browser-engine');
const { runSnapshot } = require('./lib/snapshot-runner');

async function main() {
  const opts = parseArgs(process.argv);

  if (!opts.isUrl && !fs.existsSync(opts.input)) {
    console.error(`${LOG_PREFIX}input not found: ${opts.input}`);
    process.exit(1);
  }

  // The runner expects a fully-qualified URL. Local paths become file:// URLs
  // here so the runner stays scheme-agnostic.
  const targetUrl = opts.isUrl ? opts.input : `file://${opts.input}`;

  let engineHandle;
  try {
    engineHandle = await launchBrowser({ engine: opts.browserEngine });
  } catch (err) {
    // Most launch failures are missing-browser errors after a sandboxed
    // `npm install` redirected the engine's download cache. Surface a one-line
    // hint so users can fix it without reading the full stack trace; the
    // original error still propagates so non-cache failures aren't masked.
    const msg = errMessage(err);
    if (/Could not find Chrome|Failed to launch the browser process|Executable doesn't exist/i.test(msg)) {
      console.error(`${LOG_PREFIX}failed to launch headless Chromium for engine '${opts.browserEngine}'. Try:`);
      if (opts.browserEngine === 'playwright') {
        console.error(`  npx --prefix tools/html-snapshot playwright install chromium`);
      } else {
        console.error(`  PUPPETEER_CACHE_DIR="$HOME/.cache/puppeteer" \\`);
        console.error(`    npx --prefix tools/html-snapshot puppeteer browsers install chrome`);
      }
    }
    throw err;
  }

  try {
    const result = await runSnapshot(engineHandle, targetUrl, {
      viewportWidth: opts.viewportWidth,
      viewportHeight: opts.viewportHeight,
      waitMs: opts.waitMs,
      selector: opts.selector,
      cookies: opts.cookies,
      headers: opts.headers,
      inlineIconFonts: opts.inlineIconFonts,
      log: (msg) => console.error(`${LOG_PREFIX}${msg}`),
    });

    if (opts.outputToStdout) {
      // Stdout mode (triggered by `-o -`, used by pagx's `--html-snapshot`
      // integration): the HTML is the sole stdout payload, and the
      // "wrote ..." progress line goes to stderr so the parent process can
      // read stdout as a pure HTML string without parsing.
      process.stdout.write(result.html);
      console.error(`${LOG_PREFIX}wrote stdout (${result.width}x${result.height})`);
    } else {
      fs.writeFileSync(opts.output, result.html, 'utf8');
      console.log(`${LOG_PREFIX}wrote ${opts.output} (${result.width}x${result.height})`);
    }
  } finally {
    await engineHandle.browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
