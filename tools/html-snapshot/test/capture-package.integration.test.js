// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { createRequire } = require('module');
const { execFile, spawn } = require('child_process');
const { promisify } = require('util');
const { checkCapturePackage } = require('../../../cli/npm/scripts/check-capture');
const exec = promisify(execFile);
// Run against an unpacked/installed tarball, never implicitly against source staging.
const packageDir = process.env.PAGX_PACKAGE_TEST_DIR;
const packagedTest = packageDir ? test : test.skip;
const sourceModules = path.resolve(__dirname, '../node_modules');
let dir, cache, env;
jest.setTimeout(60000);

// Copy only the selected drivers' dependency closure to an external cache. The
// installed package's processes receive no repository path or repository symlink.
function copyDriver(name, output, copied = new Set()) {
  const copy = moduleDir => {
    if (copied.has(moduleDir)) return;
    copied.add(moduleDir);
    const relative = path.relative(sourceModules, moduleDir);
    if (relative.startsWith('..')) throw new Error(`driver dependency escaped fixture modules: ${moduleDir}`);
    const target = path.join(output, 'node_modules', relative);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.cpSync(moduleDir, target, { recursive: true, dereference: true });
    const manifest = path.join(moduleDir, 'package.json');
    const pkg = JSON.parse(fs.readFileSync(manifest));
    const resolve = createRequire(manifest).resolve;
    for (const dependency of Object.keys({ ...pkg.dependencies, ...pkg.optionalDependencies })) {
      const candidate = resolve.paths('__pagx_dependency__').map(base => path.join(base, dependency))
        .find(base => fs.existsSync(path.join(base, 'package.json')));
      if (candidate) copy(candidate);
      else if (pkg.dependencies?.[dependency]) throw new Error(`missing fixture dependency: ${dependency}`);
    }
  };
  copy(path.join(sourceModules, name));
}

beforeAll(() => {
  if (!packageDir) return;
  dir = fs.mkdtempSync(path.join(os.tmpdir(), "installed capture 中文 ' "));
  cache = path.join(dir, 'browser-cache');
  copyDriver('puppeteer', cache);
  copyDriver('playwright', cache);
  env = { ...process.env, NODE_PATH: '', PAGX_HTML_SNAPSHOT_CACHE: cache,
    PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL: '1', PAGX_BIN: path.join(dir, 'wrong-version'),
    PUPPETEER_EXECUTABLE_PATH: require('puppeteer').executablePath(),
    PLAYWRIGHT_EXECUTABLE_PATH: require('playwright').chromium.executablePath() };
  for (const key of ['PAGX_HTML_SNAPSHOT_BIN', 'PAGX_HTML_CAPTURE_BIN', 'PAGX_NODE_BIN']) delete env[key];
  fs.copyFileSync(path.join(__dirname, 'fixtures/interaction.html'), path.join(dir, 'page.html'));
  fs.copyFileSync(path.join(__dirname, 'fixtures/interaction.scenario.json'), path.join(dir, 'states.json'));
});
afterAll(() => { if (dir) fs.rmSync(dir, { recursive: true, force: true }); });
const run = (args, extraEnv = {}) => exec(process.execPath, [path.join(packageDir, 'bin/pagx.js'), ...args],
  { cwd: dir, env: { ...env, ...extraEnv }, timeout: 45000, maxBuffer: 8 * 1024 * 1024 });

packagedTest('packed files, runtime dependencies and native version pass release checks', () => {
  expect(() => checkCapturePackage(packageDir)).not.toThrow();
});

packagedTest('help, dry-run and invalid scenario do not initialize an empty browser cache', async () => {
  const emptyCache = path.join(dir, 'untouched-cache');
  const empty = { PAGX_HTML_SNAPSHOT_CACHE: emptyCache, PUPPETEER_EXECUTABLE_PATH: '/absent' };
  expect((await run(['capture', '--help'], empty)).stdout).toContain('--preview');
  await run(['capture', '--batch', dir, '--dry-run', '--output-root', path.join(dir, 'dry-output')], empty);
  await expect(run(['capture', 'page.html', '--scenario', 'states.json', '--checkpoint', 'missing'], empty))
    .rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('Unknown checkpoint') });
  const snapshot = await exec(process.execPath, [path.join(packageDir, 'html-snapshot/launch.js'), '--help'],
    { cwd: dir, env: { ...env, ...empty }, timeout: 15000 });
  expect(snapshot.stdout).toContain('Usage: html-snapshot');
  expect(fs.existsSync(emptyCache)).toBe(false);
  expect(fs.existsSync(path.join(dir, 'dry-output'))).toBe(false);
});

packagedTest('missing browser returns an actionable failure without enabling auto-install', async () => {
  const emptyCache = path.join(dir, 'no-browser');
  await expect(run(['capture', 'page.html', '-o', path.join(dir, 'missing-out')], {
    PAGX_HTML_SNAPSHOT_CACHE: emptyCache, PUPPETEER_EXECUTABLE_PATH: '/absent',
  })).rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('auto-install disabled') });
  expect(fs.existsSync(emptyCache)).toBe(false);
});

packagedTest.each(['puppeteer', 'playwright'])('installed capture uses the external %s cache and its own native binary', async engine => {
  const out = path.join(dir, engine);
  await run(['capture', 'page.html', '--scenario', 'states.json', '-o', out, '--preview', '--browser-engine', engine]);
  const manifest = JSON.parse(fs.readFileSync(path.join(out, 'page.captures.json')));
  expect(manifest.status).toBe('complete');
  expect(manifest.outputs.map(o => o.name)).toEqual(['initial', 'advanced']);
  expect(manifest.preview.status).toBe('complete');
  expect(fs.existsSync(path.join(out, manifest.preview.pages[0].file))).toBe(true);
  expect(fs.readFileSync(path.join(out, 'page.advanced.pagx'), 'utf8')).toContain('Result: PAGX');
});

packagedTest('bundled import retains single-checkpoint HTML stdout bridging', async () => {
  const output = path.join(dir, 'single.pagx');
  await run(['import', '--input', 'page.html', '--scenario', 'states.json', '--checkpoint', 'advanced', '--output', output]);
  expect(fs.readFileSync(output, 'utf8')).toContain('Result: PAGX');
});

packagedTest('installed native binary locates its bundled runtime without the npm wrapper', async () => {
  const executable = [process.platform + '-' + process.arch, process.platform]
    .map(platform => path.join(packageDir, 'bin', platform, process.platform === 'win32' ? 'pagx.exe' : 'pagx'))
    .find(file => fs.existsSync(file));
  const out = path.join(dir, 'direct-native');
  await exec(executable, ['capture', 'page.html', '--scenario', 'states.json', '--checkpoint', 'initial', '-o', out, '--no-render'],
    { cwd: dir, env: { ...env, PAGX_NODE_BIN: process.execPath }, timeout: 45000 });
  const manifest = JSON.parse(fs.readFileSync(path.join(out, 'page.captures.json')));
  expect(manifest).toMatchObject({ status: 'complete', outputs: [{ name: 'initial', png: null }] });
});

(process.platform === 'win32' ? test.skip : packagedTest)('npm launcher cancellation reaches the source session and manifest', async () => {
  const plan = path.join(dir, 'waiting.json'), out = path.join(dir, 'cancelled');
  fs.writeFileSync(plan, JSON.stringify({ version: 1, steps: [
    { type: 'waitFor', target: { css: '#never' }, state: 'visible', timeoutMs: 30000 },
    { type: 'capture', name: 'done' },
  ] }));
  const child = spawn(process.execPath, [path.join(packageDir, 'bin/pagx.js'), 'capture', 'page.html', '--scenario', plan, '-o', out],
    { cwd: dir, env, stdio: ['ignore', 'pipe', 'pipe'] });
  let stderr = '', signalled = false;
  const timeout = setTimeout(() => child.kill('SIGKILL'), 15000);
  child.stderr.on('data', chunk => {
    stderr += chunk;
    if (!signalled && stderr.includes('step 1/2: waitFor')) { signalled = true; child.kill('SIGTERM'); }
  });
  try {
    const code = await new Promise((resolve, reject) => { child.on('close', resolve); child.on('error', reject); });
    expect(signalled).toBe(true);
    expect(code).toBe(143);
    expect(JSON.parse(fs.readFileSync(path.join(out, 'page.captures.json')))).toMatchObject({ status: 'failed', outputs: [] });
  } finally { clearTimeout(timeout); }
});
