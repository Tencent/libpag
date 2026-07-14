'use strict';

/**
 * Preflight build check shared by the CLI (`snapshot.js`) and HTTP service (`server.js`).
 *
 * Both entrypoints run the *compiled* TypeScript in `dist/`, which only exists after
 * `npm install` (to fetch the `typescript`/`puppeteer` toolchain) and `npm run build`. Run
 * straight from a fresh checkout (e.g. via `pagx import`), the bare `require('./dist/...')`
 * in those files throws an opaque MODULE_NOT_FOUND stack trace. This helper detects the
 * missing pieces up front and prints a short, actionable hint instead.
 *
 * It is intentionally plain JavaScript with zero dependencies and lives at the package root
 * (not under `dist/`), because `dist/` is exactly what may be absent.
 */

const fs = require('fs');
const path = require('path');

const LOG_PREFIX = 'html-snapshot: ';

// Throws on the first missing prerequisite with a multi-line, copy-pasteable hint. Returns
// silently when everything needed to load `dist/` is present.
function ensureBuilt() {
  const root = __dirname;
  const steps = [];
  if (!fs.existsSync(path.join(root, 'node_modules'))) {
    steps.push('npm install');
  }
  if (!fs.existsSync(path.join(root, 'dist', 'lib', 'cli.js'))) {
    steps.push('npm run build');
  }
  if (steps.length === 0) {
    return;
  }
  const lines = [
    `${LOG_PREFIX}the html-snapshot tool is not built yet.`,
    `${LOG_PREFIX}run the following, then re-run your command:`,
    '',
    `    cd ${root}`,
    ...steps.map((step) => `    ${step}`),
    '',
  ];
  for (const line of lines) {
    console.error(line);
  }
  process.exit(1);
}

module.exports = { ensureBuilt };
