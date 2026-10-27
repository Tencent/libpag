// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFile, spawn } = require('child_process');
const { promisify } = require('util');
const { PNG } = require('pngjs');
const exec = promisify(execFile);
const root = path.resolve(__dirname, '../../..');
const pagx = process.env.PAGX_TEST_BIN || path.join(root, 'cmake-build-debug', process.platform === 'win32' ? 'pagx.exe' : 'pagx');
const nativeTest = fs.existsSync(pagx) ? test : test.skip;
const fixture = path.join(__dirname, 'fixtures/interaction.html');
const scenario = path.join(__dirname, 'fixtures/interaction.scenario.json');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), "capture 中文 ' & ")); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });
jest.setTimeout(60000);
const run = (args, env = {}) => exec(pagx, args, { cwd: dir, timeout: 50000, maxBuffer: 8 * 1024 * 1024,
  env: { ...process.env, PAGX_NODE_BIN: process.execPath, ...env } });

nativeTest('native bridge preserves arguments, child exit code, and the invoking binary', async () => {
  const bridge = path.join(dir, 'bridge 中文.js');
  fs.writeFileSync(bridge, 'process.stdout.write(JSON.stringify(process.argv.slice(2))); process.exit(7);');
  const args = ['capture', 'page 中文.html', '--scenario', "states ' & $x.json", '-o', 'out space', '--preview'];
  const error = await run(args, { PAGX_HTML_CAPTURE_BIN: bridge, PAGX_BIN: '/wrong/version' }).catch(e => e);
  expect(error.code).toBe(7);
  expect(JSON.parse(error.stdout)).toEqual(['--pagx-bin', fs.realpathSync(pagx), ...args.slice(1)]);
});

nativeTest('explicit missing runtime and node overrides fail without falling back', async () => {
  await expect(run(['capture', '--help'], { PAGX_HTML_CAPTURE_BIN: path.join(dir, 'absent.js') }))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('runtime not found') });
  await expect(run(['capture', '--help'], { PAGX_NODE_BIN: path.join(dir, 'absent-node') }))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('failed to start Node') });
});

nativeTest('help and invalid options work outside the checkout without browser startup', async () => {
  expect((await run(['capture', '--help'], { PUPPETEER_EXECUTABLE_PATH: '/missing' })).stdout).toContain('pagx capture');
  await expect(run(['capture', fixture, '--scenario', scenario, '--checkpoint', 'absent'],
    { PUPPETEER_EXECUTABLE_PATH: '/missing' })).rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('Unknown checkpoint') });
});

nativeTest('native and legacy entrypoints produce the same checkpoint pixels and preview', async () => {
  const oldOut = path.join(dir, 'legacy'), newOut = path.join(dir, 'native');
  const args = [fixture, '--scenario', scenario, '--preview', '--pagx-images', 'embed'];
  await exec(process.execPath, [path.join(root, 'tools/html-snapshot/html2pagx'), ...args,
    '-o', oldOut, '--pagx-bin', pagx], { cwd: dir, timeout: 50000 });
  await run(['capture', ...args, '-o', newOut], { PAGX_BIN: path.join(dir, 'wrong-version') });
  const oldManifest = JSON.parse(fs.readFileSync(path.join(oldOut, 'interaction.captures.json')));
  const newManifest = JSON.parse(fs.readFileSync(path.join(newOut, 'interaction.captures.json')));
  expect(newManifest.status).toBe('complete');
  expect(newManifest.outputs.map(o => o.name)).toEqual(['initial', 'advanced']);
  expect(newManifest.preview).toEqual(oldManifest.preview);
  for (const file of ['interaction.initial.png', 'interaction.advanced.png', 'interaction.preview/sheet-001.png']) {
    const before = PNG.sync.read(fs.readFileSync(path.join(oldOut, file)));
    const after = PNG.sync.read(fs.readFileSync(path.join(newOut, file)));
    expect([after.width, after.height]).toEqual([before.width, before.height]);
    expect(after.data.equals(before.data)).toBe(true);
  }
  await run(['capture', ...args, '-o', newOut, '--checkpoint', 'advanced']);
  const selected = JSON.parse(fs.readFileSync(path.join(newOut, 'interaction.captures.json')));
  expect(selected.outputs.map(o => o.name)).toEqual(['advanced']);
  expect(selected.preview.pages[0].items).toEqual(['advanced']);
});

nativeTest('URL scenario loads the source once including preview generation', async () => {
  let visits = 0;
  const server = http.createServer((req, res) => {
    if (req.url === '/') visits++;
    res.setHeader('Content-Type', 'text/html');
    res.end('<!doctype html><body><button id="next" onclick="this.textContent=\'Done\'">Next</button></body>');
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  try {
    const plan = path.join(dir, 'states.json');
    fs.writeFileSync(plan, JSON.stringify({ version: 1, viewport: { width: 320, height: 200 }, steps: [
      { type: 'capture', name: 'preview' }, { type: 'click', target: { css: '#next' } }, { type: 'capture', name: 'done' },
    ] }));
    await run(['capture', `http://127.0.0.1:${server.address().port}/`, '--output-name', 'web',
      '--scenario', plan, '-o', dir, '--preview']);
    expect(visits).toBe(1);
    const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'web.captures.json')));
    expect(manifest.preview.pages[0].items).toEqual(['preview', 'done']);
  } finally { await new Promise(resolve => server.close(resolve)); }
});

nativeTest('a later failed interaction preserves the earlier checkpoint without replay', async () => {
  const plan = path.join(dir, 'partial.json');
  fs.writeFileSync(plan, JSON.stringify({ version: 1, viewport: { width: 320, height: 200 }, steps: [
    { type: 'capture', name: 'initial' },
    { type: 'waitFor', target: { css: '#missing' }, state: 'visible', timeoutMs: 100 },
    { type: 'capture', name: 'later' },
  ] }));
  await expect(run(['capture', fixture, '--scenario', plan, '-o', dir]))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('Scenario step 2') });
  const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'interaction.captures.json')));
  expect(manifest).toMatchObject({ status: 'failed', outputs: [{ name: 'initial' }] });
  expect(manifest.outputs).toHaveLength(1);
  expect(fs.existsSync(manifest.outputs[0].pagx)).toBe(true);
  expect(PNG.sync.read(fs.readFileSync(manifest.outputs[0].png)).width).toBe(320);
});

nativeTest('ordinary and batch previews use newly rendered files with legacy output paths', async () => {
  const input = path.join(dir, 'pages'), out = path.join(dir, 'batch'), single = path.join(dir, 'single');
  fs.mkdirSync(input);
  fs.writeFileSync(path.join(input, 'a # 中文.html'), '<!doctype html><body style="margin:0;background:red">First</body>');
  fs.writeFileSync(path.join(input, 'b.html'), '<!doctype html><body style="margin:0;background:blue">Second</body>');
  await run(['capture', path.join(input, 'b.html'), '-o', single, '--preview']);
  expect(fs.existsSync(path.join(single, 'b.subset.html'))).toBe(true);
  expect(PNG.sync.read(fs.readFileSync(path.join(single, 'b.preview/sheet-001.png'))).width).toBeGreaterThan(0);
  await run(['capture', '--batch', input, '--output-root', out, '--preview']);
  expect(fs.existsSync(path.join(out, 'a # 中文.pagx'))).toBe(true);
  expect(fs.existsSync(path.join(out, 'b.pagx'))).toBe(true);
  expect(PNG.sync.read(fs.readFileSync(path.join(out, 'preview/sheet-001.png'))).width).toBeGreaterThan(0);
});

(process.platform !== 'win32' ? nativeTest : test.skip)('SIGTERM cancels the active session and records failure', async () => {
  const plan = path.join(dir, 'waiting.json');
  fs.writeFileSync(plan, JSON.stringify({ version: 1, steps: [
    { type: 'waitFor', target: { css: '#never' }, state: 'visible', timeoutMs: 30000 },
    { type: 'capture', name: 'done' },
  ] }));
  const child = spawn(pagx, ['capture', fixture, '--scenario', plan, '-o', dir], {
    env: { ...process.env, PAGX_NODE_BIN: process.execPath }, stdio: ['ignore', 'pipe', 'pipe'],
  });
  let stderr = '', signalled = false;
  const timer = setTimeout(() => child.kill('SIGKILL'), 15000);
  child.stderr.on('data', chunk => {
    stderr += chunk;
    if (!signalled && stderr.includes('step 1/2: waitFor')) { signalled = true; child.kill('SIGTERM'); }
  });
  try {
    const code = await new Promise((resolve, reject) => { child.on('error', reject); child.on('close', resolve); });
    expect(signalled).toBe(true);
    expect(code).toBe(143);
    const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'interaction.captures.json')));
    expect(manifest).toMatchObject({ status: 'failed', outputs: [], error: expect.stringContaining('SIGTERM') });
  } finally { clearTimeout(timer); }
});
