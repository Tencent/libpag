// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

// Shared lazy bootstrap for snapshot (single HTML stdout) and import (artifact set).
// Runtime validation happens before any install, browser launch, or cache mutation.
const fs = require('fs');
const os = require('os');
const path = require('path');
const { runProcess } = require('../process');
const HERE = __dirname;
const log = message => process.stderr.write(`pagx html-snapshot: ${message}\n`);

function cacheRoot() {
  if (process.env.PAGX_HTML_SNAPSHOT_CACHE) return path.resolve(process.env.PAGX_HTML_SNAPSHOT_CACHE);
  return process.platform === 'win32'
    ? path.join(process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'), 'libpag-pagx', 'html-snapshot')
    : path.join(os.homedir(), '.cache', 'libpag-pagx', 'html-snapshot');
}

function browserReady(engine, cache) {
  try {
    const entry = require.resolve(engine, { paths: [HERE, cache] });
    const mod = require(entry);
    const executable = engine === 'playwright'
      ? process.env.PLAYWRIGHT_EXECUTABLE_PATH || mod.chromium.executablePath()
      : process.env.PUPPETEER_EXECUTABLE_PATH || mod.executablePath();
    return !!executable && fs.statSync(executable).isFile();
  } catch (_) { return false; }
}

function npmCommand() {
  // npm.cmd requires cmd.exe on Windows. Invoke npm's JS CLI with the current
  // Node instead, preserving Unicode/metacharacters without shell interpolation.
  const base = path.dirname(process.execPath);
  const candidates = [process.env.npm_execpath, path.join(base, 'node_modules/npm/bin/npm-cli.js'),
    path.join(base, '../lib/node_modules/npm/bin/npm-cli.js')];
  const cli = candidates.find(file => file && file.endsWith('.js') && fs.existsSync(file));
  if (cli) return { command: process.execPath, prefix: [cli] };
  if (process.platform !== 'win32') return { command: 'npm', prefix: [] };
  throw new Error('npm CLI not found; install Node.js with npm, or install the selected browser manually');
}

async function prepareBrowser(engine) {
  const cache = cacheRoot();
  // Set these BEFORE requiring the driver: the parent readiness check and child
  // launch must agree on cache locations, including an explicitly supplied path.
  process.env.PUPPETEER_CACHE_DIR ||= path.join(cache, 'puppeteer');
  process.env.PLAYWRIGHT_BROWSERS_PATH ||= path.join(cache, 'playwright');
  const env = { ...process.env,
    NODE_PATH: [path.join(cache, 'node_modules'), process.env.NODE_PATH].filter(Boolean).join(path.delimiter) };
  if (browserReady(engine, cache)) return env;

  const manifest = JSON.parse(fs.readFileSync(path.join(HERE, 'package.json'), 'utf8'));
  const version = manifest.browserDependencies?.[engine];
  if (!version) throw new Error(`browser dependency missing from package metadata: ${engine}`);
  if (process.env.PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL === '1') {
    const quote = value => process.platform === 'win32'
      ? "'" + value.replaceAll("'", "''") + "'" : "'" + value.replaceAll("'", "'\\''") + "'";
    const variable = engine === 'puppeteer' ? 'PUPPETEER_CACHE_DIR' : 'PLAYWRIGHT_BROWSERS_PATH';
    const assignment = process.platform === 'win32'
      ? `$env:${variable} = ${quote(process.env[variable])}; ` : `${variable}=${quote(process.env[variable])} `;
    throw new Error(`${engine} browser is unavailable (auto-install disabled). Install it once:\n`
      + `  ${assignment}npm install --prefix ${quote(cache)} ${quote(engine + '@' + version)}\n`
      + `  ${assignment}npm exec --prefix ${quote(cache)} -- ${engine} `
      + (engine === 'puppeteer' ? 'browsers install chrome' : 'install chromium'));
  }
  fs.mkdirSync(cache, { recursive: true });
  if (!fs.existsSync(path.join(cache, 'package.json'))) {
    fs.writeFileSync(path.join(cache, 'package.json'), JSON.stringify({ name: 'pagx-browser-cache', private: true }));
  }
  const npm = npmCommand();
  const install = async args => {
    const code = await runProcess(npm.command, [...npm.prefix, ...args], {
      cwd: cache, env, stdio: ['ignore', process.stderr, process.stderr],
    });
    if (code !== 0) {
      const error = new Error(`browser installation failed (exit ${code})`);
      error.exitCode = code;
      throw error;
    }
  };
  log(`installing ${engine} and its browser in ${cache}...`);
  await install(['install', '--no-audit', '--no-fund', '--no-package-lock', '--omit=dev', '--omit=optional', `${engine}@${version}`]);
  if (!browserReady(engine, cache)) {
    await install(['exec', '--', engine, ...(engine === 'puppeteer' ? ['browsers', 'install', 'chrome'] : ['install', 'chromium'])]);
  }
  if (!browserReady(engine, cache)) throw new Error(`${engine} browser is still unavailable after installation`);
  return env;
}

function validateSnapshot(args) {
  const { parseArgs } = require('./dist/lib/cli');
  const { parseScenario, selectCheckpoint } = require('./dist/lib/interaction');
  const opts = parseArgs([process.execPath, path.join(HERE, 'snapshot.js'), ...args]);
  if (opts.scenario) {
    const scenario = parseScenario(JSON.parse(fs.readFileSync(opts.scenario, 'utf8')));
    selectCheckpoint(scenario, opts.checkpoint);
    if (opts.browserEngine !== 'playwright' && scenario.steps.some(step => step.target?.role)) {
      throw new Error('Role locators require --browser-engine playwright');
    }
  }
  if (!opts.isUrl && !fs.statSync(opts.input, { throwIfNoEntry: false })?.isFile()) {
    throw new Error(`input not found: ${opts.input}`);
  }
  return opts;
}

async function launchBundled(kind, args = process.argv.slice(2)) {
  try {
    let opts;
    let needsBrowser = true;
    if (kind === 'import') {
      const runtime = require('./dist/lib/import-request');
      opts = runtime.validateImportRequest(runtime.readImportRequest(args));
      runtime.preflightImport(opts);
      needsBrowser = runtime.importNeedsBrowser(opts);
    } else if (kind === 'snapshot') {
      opts = validateSnapshot(args);
    } else {
      throw new Error('unsupported HTML runtime operation');
    }
    const env = needsBrowser ? await prepareBrowser(opts.browserEngine) : process.env;
    return await runProcess(process.execPath, [path.join(HERE, kind === 'import' ? 'import.js' : 'snapshot.js'), ...args], { env });
  } catch (error) {
    const message = kind === 'import' ? require('./dist/lib/import-request').importErrorMessage(error) : error.message;
    log(message);
    return error.exitCode || (error.code === 2 ? 2 : 1);
  }
}

module.exports = { browserReady, cacheRoot, npmCommand, prepareBrowser, launchBundled };
