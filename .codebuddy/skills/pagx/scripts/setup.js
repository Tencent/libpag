#!/usr/bin/env node
'use strict';
// Repository HTML workflow setup. Use the complete npm package outside a checkout.
// --check is read-only; normal setup rebuilds the snapshot TypeScript before checking it.

const fs = require('fs');
const path = require('path');
const { createRequire } = require('module');
const { spawnSync } = require('child_process');

const usage = `Usage: node setup.js [options]

Check a libpag checkout's native CLI and HTML capture runtime.
Normal setup rebuilds TypeScript; missing dependencies/browser are reported, not installed.

  --repo <dir>                 Explicit libpag checkout (otherwise locate from script/cwd)
  --pagx-bin <path>            Native binary (or PAGX_BIN; default: cmake-build-debug/pagx)
  --browser-engine <name>      puppeteer or playwright (or HTML_SNAPSHOT_BROWSER)
  --check                     Check without rebuilding or installing anything
  -h, --help                  Show help

Build the native CLI first using your configured CMake build directory.
This script does not install a global CLI or prepare the separate live-preview viewer.
`;

function say(message) { process.stdout.write(`setup: ${message}\n`); }
function fail(message) { throw new Error(message); }

function parseArgs(args) {
  const options = { repo: '', binary: process.env.PAGX_BIN || '',
    engine: process.env.HTML_SNAPSHOT_BROWSER || 'puppeteer', check: false };
  const flags = { '--repo': 'repo', '--pagx-bin': 'binary', '--browser-engine': 'engine' };
  for (let i = 0; i < args.length; i++) {
    const flag = args[i];
    if (flag === '--help' || flag === '-h') return null;
    if (flag === '--check') { options.check = true; continue; }
    if (!Object.hasOwn(flags, flag)) fail(`unknown option: ${flag}`);
    const key = flags[flag];
    const value = args[++i];
    if (!value || value.startsWith('-')) fail(`${flag} requires a value`);
    options[key] = value;
  }
  if (!['puppeteer', 'playwright'].includes(options.engine)) {
    fail('--browser-engine must be puppeteer or playwright');
  }
  return options;
}

function isFile(file) {
  try { return fs.statSync(file).isFile(); } catch { return false; }
}

function isRepo(dir) {
  return isFile(path.join(dir, 'tools/html-snapshot/package.json')) &&
    isFile(path.join(dir, 'src/cli/main.cpp'));
}

function findRepo(start) {
  let dir = path.resolve(start);
  while (true) {
    if (isRepo(dir)) return dir;
    const parent = path.dirname(dir);
    if (parent === dir) return '';
    dir = parent;
  }
}

function run(binary, args, cwd, inherit = false) {
  const result = spawnSync(binary, args, {
    cwd, encoding: 'utf8', stdio: inherit ? 'inherit' : 'pipe',
    env: { ...process.env, PAGX_NODE_BIN: process.env.PAGX_NODE_BIN || process.execPath },
  });
  if (result.error) fail(`cannot run ${binary}: ${result.error.message}`);
  if (result.status !== 0) {
    if (!inherit && result.stderr) process.stderr.write(result.stderr);
    fail(`${path.basename(binary)} ${args.join(' ')} failed (${result.signal || result.status})`);
  }
  return result.stdout || '';
}

function collectSources(dir) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(entry => {
    const file = path.join(dir, entry.name);
    return entry.isDirectory() ? collectSources(file) : (entry.name.endsWith('.ts') ? [file] : []);
  });
}

function checkBuild(toolDir) {
  const sources = collectSources(path.join(toolDir, 'lib'));
  let oldestOutput = Infinity;
  for (const source of sources.filter(file => !file.endsWith('.d.ts'))) {
    const relative = path.relative(toolDir, source).replace(/\.ts$/, '.js');
    const output = path.join(toolDir, 'dist', relative);
    if (!isFile(output) || fs.statSync(source).mtimeMs > fs.statSync(output).mtimeMs) {
      fail(`snapshot build missing or stale: ${relative}; rerun setup without --check`);
    }
    oldestOutput = Math.min(oldestOutput, fs.statSync(output).mtimeMs);
  }
  for (const source of [path.join(toolDir, 'tsconfig.json'),
    ...sources.filter(file => file.endsWith('.d.ts'))]) {
    if (fs.statSync(source).mtimeMs > oldestOutput) {
      fail('snapshot configuration/types changed; rerun setup without --check');
    }
  }
}

function main() {
  const options = parseArgs(process.argv.slice(2));
  if (!options) { process.stdout.write(usage); return; }
  const repo = options.repo ? path.resolve(options.repo) : findRepo(__dirname) || findRepo(process.cwd());
  if (!repo || !isRepo(repo)) {
    fail('libpag checkout not found; pass --repo <dir>. Installed @libpag/pagx users do not need this repository setup script.');
  }
  const binary = options.binary ? path.resolve(options.binary) :
    path.join(repo, 'cmake-build-debug', process.platform === 'win32' ? 'pagx.exe' : 'pagx');
  if (!isFile(binary)) {
    fail(`native CLI not found: ${binary}; build pagx and pass its path with --pagx-bin`);
  }
  say(`node ${process.version}`);
  say(`native CLI ${binary}`);
  say(run(binary, ['--version'], repo).trim());
  const help = run(binary, ['--help'], repo);
  for (const command of ['import', 'capture', 'render', 'verify']) {
    if (!new RegExp(`^\\s+${command}\\s`, 'm').test(help)) {
      fail(`native CLI does not advertise ${command}; rebuild a CLI supporting this HTML workflow`);
    }
  }

  const toolDir = path.join(repo, 'tools/html-snapshot');
  const toolRequire = createRequire(path.join(toolDir, 'package.json'));
  const pkg = toolRequire('./package.json');
  let compiler;
  try {
    for (const dependency of Object.keys(pkg.dependencies || {})) {
      if (!['puppeteer', 'playwright'].includes(dependency)) toolRequire.resolve(dependency);
    }
    toolRequire.resolve(options.engine);
    if (!options.check) compiler = toolRequire.resolve('typescript/bin/tsc');
  } catch (error) {
    fail(`snapshot dependencies unavailable: ${error.message}\nRun npm install in ${toolDir}; include development dependencies and the selected browser driver.`);
  }
  if (!options.check) {
    say('building snapshot TypeScript');
    run(process.execPath, [compiler, '-p', path.join(toolDir, 'tsconfig.json')], toolDir, true);
  }
  checkBuild(toolDir);
  // Exercises the repository capture modules without starting a browser.
  run(process.execPath, [path.join(toolDir, 'capture.js'), '--help'], toolDir);
  const { checkBrowserRuntime } = toolRequire('./dist/lib/browser-engine.js');
  try {
    checkBrowserRuntime(options.engine);
  } catch (error) {
    const command = options.engine === 'playwright'
      ? 'npx playwright install chromium' : 'npx puppeteer browsers install chrome';
    const override = options.engine === 'playwright'
      ? 'PLAYWRIGHT_EXECUTABLE_PATH' : 'PUPPETEER_EXECUTABLE_PATH';
    fail(`${error.message}\nFrom ${toolDir}, run: ${command}\nIf ${override} is set, correct that path or unset it.`);
  }
  // Also check that the native bridge can find its configured runtime and Node.
  run(binary, ['capture', '--help'], repo);
  say(`${options.engine} browser executable present (launch not tested)`);
  say('ready');
}

try { main(); }
catch (error) {
  process.stderr.write(`setup: ERROR: ${error.message}\n`);
  process.exitCode = 1;
}
