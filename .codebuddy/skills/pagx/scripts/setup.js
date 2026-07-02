#!/usr/bin/env node
'use strict';
// One-time environment setup / health check for the pagx skill's HTML->PAGX workflow.
//
// Single cross-platform script (macOS / Linux / Windows). `node` is already a hard
// prerequisite of the whole pipeline, so the setup itself runs on node too. Verifies
// the tools the conversion needs and prepares the snapshot tool:
//   1. node      - the runtime this script already runs on (version reported)
//   2. pagx      - the CLI that imports/renders PAGX (installed via npm if missing)
//   3. tools/html-snapshot - npm deps installed + TypeScript built (dist/)
//   4. headless Chromium - bundled with puppeteer; reported (not auto-installed) if missing
//
// Idempotent: re-running only does work that is still needed. Prints "setup: ready"
// on success. Run from anywhere inside the repository:
//   node .codebuddy/skills/pagx/scripts/setup.js

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

function say(m) { process.stdout.write(`setup: ${m}\n`); }
function warn(m) { process.stderr.write(`setup: WARNING: ${m}\n`); }
function die(m) { process.stderr.write(`setup: ERROR: ${m}\n`); process.exit(1); }

// Run a command, inheriting stdio. `shell: true` lets `npm`/`pagx` resolve to their
// platform-specific launchers (npm.cmd, pagx.cmd) on Windows without extra logic.
function run(cmd, args, opts = {}) {
  return spawnSync(cmd, args, { stdio: 'inherit', shell: true, ...opts });
}

// Probe whether a command exists by running it quietly and inspecting the result.
function commandExists(cmd, probeArgs = ['--version']) {
  const r = spawnSync(cmd, probeArgs, { stdio: 'ignore', shell: true });
  return r.status === 0;
}

// --- locate the repository root and the snapshot tool ---------------------
let repoRoot = '';
const git = spawnSync('git', ['rev-parse', '--show-toplevel'], { encoding: 'utf8', shell: true });
if (git.status === 0 && git.stdout) repoRoot = git.stdout.trim();
if (!repoRoot) {
  // Fall back to walking up from this script to find tools/html-snapshot.
  let d = __dirname;
  while (path.dirname(d) !== d && !fs.existsSync(path.join(d, 'tools', 'html-snapshot'))) {
    d = path.dirname(d);
  }
  repoRoot = d;
}
const toolDir = path.join(repoRoot, 'tools', 'html-snapshot');

// --- node -----------------------------------------------------------------
// This script is running under node, so node is present by definition.
say(`node ${process.version}`);

// --- pagx -----------------------------------------------------------------
if (!commandExists('pagx', ['-v'])) {
  say('pagx not found; installing @libpag/pagx globally via npm...');
  const r = run('npm', ['install', '-g', '@libpag/pagx']);
  if (r.status !== 0) {
    die('failed to install pagx. Install it manually: npm install -g @libpag/pagx');
  }
}
const pagxVer = spawnSync('pagx', ['-v'], { encoding: 'utf8', shell: true });
let pagxVerStr = '';
if (pagxVer.status === 0 && pagxVer.stdout) {
  const tokens = pagxVer.stdout.trim().split(/\s+/);
  pagxVerStr = tokens[tokens.length - 1];
}
say(`pagx ${pagxVerStr}`);

// --- no repository: the snapshot tool is bundled inside @libpag/pagx -------
// When tools/html-snapshot is absent (a user who only `npm install`-ed the
// package), there is nothing to build here: `pagx` carries the snapshot tool
// and installs the headless browser lazily on first use.
if (!fs.existsSync(toolDir)) {
  say('tools/html-snapshot not present; using the snapshot tool bundled in @libpag/pagx');
  say('the headless browser installs automatically on the first conversion (~150 MB, one-time)');
  say('ready');
  process.exit(0);
}

// --- snapshot tool: dependencies ------------------------------------------
if (!fs.existsSync(path.join(toolDir, 'node_modules'))) {
  say('installing snapshot tool dependencies (this downloads a Chromium build, ~150 MB)...');
  const r = run('npm', ['install'], { cwd: toolDir });
  if (r.status !== 0) die('npm install failed in tools/html-snapshot');
} else {
  say('snapshot tool dependencies present');
}

// --- snapshot tool: build (html2pagx requires dist/) ----------------------
if (!fs.existsSync(path.join(toolDir, 'dist', 'lib'))) {
  say('building snapshot tool (tsc)...');
  const r = run('npm', ['run', 'build'], { cwd: toolDir });
  if (r.status !== 0) die('npm run build failed in tools/html-snapshot');
} else {
  say('snapshot tool already built');
}

// --- headless Chromium ----------------------------------------------------
const home = process.env.HOME || process.env.USERPROFILE || '';
const cacheDir = process.env.PUPPETEER_CACHE_DIR || path.join(home, '.cache', 'puppeteer');
const chromeDir = path.join(cacheDir, 'chrome');
let chromePresent = false;
try {
  chromePresent = fs.existsSync(chromeDir) && fs.readdirSync(chromeDir).length > 0;
} catch (_e) {
  chromePresent = false;
}
if (chromePresent) {
  say('headless Chromium present');
} else {
  warn(`headless Chromium is missing from ${chromeDir}`);
  warn('install it with:');
  warn(`  PUPPETEER_CACHE_DIR="${cacheDir}" npx --prefix "${toolDir}" puppeteer browsers install chrome`);
  warn('then re-run this script.');
  process.exit(1);
}

say('ready');
