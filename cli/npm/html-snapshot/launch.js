#!/usr/bin/env node
'use strict';

// Lazy bootstrap for the bundled html-snapshot tool.
//
// The native `pagx` binary runs `node <PAGX_HTML_SNAPSHOT_BIN> <input> -o -`
// for `pagx import --html-snapshot` / URL inputs. The package's bin/pagx.js
// wrapper points PAGX_HTML_SNAPSHOT_BIN at this file, so we get a hook to make
// the snapshot tool self-sufficient on a machine that only `npm install`-ed
// @libpag/pagx (no git checkout, no puppeteer).
//
// Responsibilities:
//   1. Keep the package install light: puppeteer + its ~150 MB Chromium are
//      NOT shipped. On first use we install them into a per-user cache dir.
//   2. Make `require('puppeteer')` (from dist/lib/browser-engine.js) resolve to
//      that cache by spawning snapshot.js with NODE_PATH pointed at it.
//   3. Keep stdout pristine: the native binary reads our stdout as the HTML
//      payload, so every progress/install line goes to stderr (fd 2). Only the
//      child snapshot.js is allowed to write to stdout (fd 1).
//
// Opt out of auto-install with PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1 (we then
// print the exact manual command and exit non-zero).

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const HERE = __dirname;
const SNAPSHOT_JS = path.join(HERE, 'snapshot.js');
const PUPPETEER_SPEC = 'puppeteer@^23.0.0';
const NPM = process.platform === 'win32' ? 'npm.cmd' : 'npm';

function log(msg) {
  process.stderr.write(`pagx html-snapshot: ${msg}\n`);
}

function die(msg, code) {
  process.stderr.write(`pagx html-snapshot: error: ${msg}\n`);
  process.exit(code == null ? 1 : code);
}

// Per-user cache for the lazily-installed puppeteer + Chromium. Overridable so
// CI / locked-down hosts can point it somewhere writable.
function cacheRoot() {
  if (process.env.PAGX_HTML_SNAPSHOT_CACHE) {
    return process.env.PAGX_HTML_SNAPSHOT_CACHE;
  }
  if (process.platform === 'win32') {
    const base = process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local');
    return path.join(base, 'libpag-pagx', 'html-snapshot');
  }
  return path.join(os.homedir(), '.cache', 'libpag-pagx', 'html-snapshot');
}

const CACHE_DIR = cacheRoot();
const CACHE_NODE_MODULES = path.join(CACHE_DIR, 'node_modules');
// Persist the Chromium build across runs (puppeteer reads this env at both
// install time and launch time).
const PUPPETEER_CACHE_DIR = process.env.PUPPETEER_CACHE_DIR || path.join(CACHE_DIR, 'puppeteer');

function resolvePuppeteer() {
  try {
    return require.resolve('puppeteer', { paths: [CACHE_NODE_MODULES] });
  } catch (_) {
    return null;
  }
}

// True when puppeteer is installed AND its browser binary exists on disk.
function chromiumReady() {
  const entry = resolvePuppeteer();
  if (!entry) return false;
  try {
    // Resolve puppeteer relative to the cache so we read the right copy.
    // eslint-disable-next-line import/no-dynamic-require, global-require
    const puppeteer = require(entry);
    const exe = typeof puppeteer.executablePath === 'function' ? puppeteer.executablePath() : '';
    return !!exe && fs.existsSync(exe);
  } catch (_) {
    return false;
  }
}

// All install chatter must land on stderr (fd 2) so stdout stays pure HTML.
function runToStderr(cmd, args, cwd, extraEnv) {
  const res = spawnSync(cmd, args, {
    cwd,
    stdio: ['ignore', process.stderr.fd, process.stderr.fd],
    env: Object.assign({}, process.env, { PUPPETEER_CACHE_DIR }, extraEnv || {}),
  });
  return res.status === 0;
}

function manualHint() {
  log('to enable HTML snapshotting, install the headless browser once:');
  log(`  mkdir -p "${CACHE_DIR}"`);
  log(`  PUPPETEER_CACHE_DIR="${PUPPETEER_CACHE_DIR}" npm install --prefix "${CACHE_DIR}" ${PUPPETEER_SPEC}`);
  log('  (re-run your pagx command afterwards)');
}

function ensureChromium() {
  if (chromiumReady()) return;

  if (process.env.PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL === '1') {
    manualHint();
    die('headless browser not installed (auto-install disabled)');
  }

  log('first run: installing the headless browser (~150 MB, one-time)...');
  fs.mkdirSync(CACHE_DIR, { recursive: true });
  fs.mkdirSync(PUPPETEER_CACHE_DIR, { recursive: true });

  // A bare package.json keeps npm from walking up into unrelated projects.
  const cachePkg = path.join(CACHE_DIR, 'package.json');
  if (!fs.existsSync(cachePkg)) {
    fs.writeFileSync(
      cachePkg,
      JSON.stringify({ name: 'pagx-html-snapshot-cache', version: '0.0.0', private: true }, null, 2) + '\n',
      'utf8',
    );
  }

  // puppeteer's postinstall downloads Chromium into PUPPETEER_CACHE_DIR.
  const installed = runToStderr(
    NPM,
    ['install', '--no-audit', '--no-fund', '--no-package-lock', '--omit=dev', '--omit=optional', PUPPETEER_SPEC],
    CACHE_DIR,
  );
  if (!installed || !resolvePuppeteer()) {
    manualHint();
    die('failed to install puppeteer');
  }

  // If the postinstall was skipped (e.g. PUPPETEER_SKIP_DOWNLOAD in the
  // environment), fetch the browser explicitly.
  if (!chromiumReady()) {
    log('downloading Chromium build...');
    const ok = runToStderr(NPM, ['exec', '--prefix', CACHE_DIR, '--', 'puppeteer', 'browsers', 'install', 'chrome'], CACHE_DIR);
    if (!ok || !chromiumReady()) {
      manualHint();
      die('failed to download the Chromium build');
    }
  }
  log('headless browser ready.');
}

function main() {
  if (!fs.existsSync(SNAPSHOT_JS)) {
    die(`bundled snapshot.js missing at ${SNAPSHOT_JS} (package build incomplete)`);
  }

  ensureChromium();

  // Spawn the real snapshot driver. NODE_PATH (read at child startup) lets its
  // `require('puppeteer')` resolve from the cache; stdout (fd 1) is inherited
  // so the HTML payload reaches the native binary untouched.
  const childEnv = Object.assign({}, process.env, {
    PUPPETEER_CACHE_DIR,
    NODE_PATH: [CACHE_NODE_MODULES, process.env.NODE_PATH].filter(Boolean).join(path.delimiter),
  });

  const res = spawnSync(process.execPath, [SNAPSHOT_JS, ...process.argv.slice(2)], {
    stdio: 'inherit',
    env: childEnv,
  });
  if (res.error) {
    die(res.error.message);
  }
  process.exit(res.status == null ? 1 : res.status);
}

main();
