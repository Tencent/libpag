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
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), "import 中文 ' & ")); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });
jest.setTimeout(60000);
const run = (args, env = {}) => exec(pagx, args, { cwd: dir, timeout: 50000, maxBuffer: 8 * 1024 * 1024,
  env: { ...process.env, PAGX_NODE_BIN: process.execPath, ...env } });

nativeTest('native bridge preserves arguments, child exit code, and the invoking binary', async () => {
  const bridge = path.join(dir, 'bridge 中文.js');
  fs.writeFileSync(bridge, 'process.stdout.write(JSON.stringify(process.argv.slice(2))); process.exit(7);');
  const args = ['import', 'page 中文.html', '--html-scenario', "states ' & $x.json", '--output-dir', 'out space',
    '--preview', '--render-scale', '1.234567891234567'];
  const error = await run(args, { PAGX_IMPORT_BIN: bridge, PAGX_BIN: '/wrong/version' }).catch(e => e);
  expect(error.code).toBe(7);
  const forwarded = JSON.parse(error.stdout);
  expect(forwarded[0]).toBe('--request');
  expect(JSON.parse(forwarded[1])).toMatchObject({ pagxBin: fs.realpathSync(pagx),
    input: 'page 中文.html', scenario: "states ' & $x.json", outputDir: 'out space', preview: true,
    scale: 1.234567891234567 });
});

nativeTest('output aliases never overwrite the source', async () => {
  const source = path.join(dir, 'source.svg');
  const content = '<svg width="16" height="12"><rect width="16" height="12"/></svg>';
  fs.writeFileSync(source, content);
  const aliases = [path.join(dir, 'hardlink.pagx')];
  fs.linkSync(source, aliases[0]);
  if (process.platform !== 'win32') {
    aliases.push(path.join(dir, 'symlink.pagx'));
    fs.symlinkSync(source, aliases[1]);
  }
  for (const output of aliases) {
    await expect(run(['import', source, '-o', output])).rejects.toMatchObject({
      code: 2, stderr: expect.stringContaining('output must differ from input'),
    });
    expect(fs.readFileSync(source, 'utf8')).toBe(content);
  }
});

nativeTest('explicit missing runtime and node overrides fail without falling back', async () => {
  await expect(run(['import', fixture], { PAGX_IMPORT_BIN: path.join(dir, 'absent.js') }))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('runtime not found') });
  await expect(run(['import', fixture], { PAGX_NODE_BIN: path.join(dir, 'absent-node') }))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('failed to start Node') });
});

nativeTest('help and invalid options work outside the checkout without browser startup', async () => {
  expect((await run(['import', '--help'], { PUPPETEER_EXECUTABLE_PATH: '/missing' })).stdout).toContain('pagx import');
  await expect(run(['import', fixture, '--html-scenario', scenario, '--html-checkpoint', 'absent'],
    { PUPPETEER_EXECUTABLE_PATH: '/missing' })).rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('Unknown checkpoint') });
});

nativeTest('scenario outputs match direct subset conversion and selected outputs use exact paths', async () => {
  const out = path.join(dir, 'native');
  await run(['import', fixture, '--html-scenario', scenario, '--output-dir', out,
    '--preview', '--html-keep-subset', '--images', 'embed'], { PAGX_BIN: '/wrong-version' });
  const manifest = JSON.parse(fs.readFileSync(path.join(out, 'interaction.captures.json')));
  expect(manifest.status).toBe('complete');
  expect(manifest.outputs.map(o => o.name)).toEqual(['initial', 'advanced']);
  expect(manifest.preview.pages[0].items).toEqual(['initial', 'advanced']);
  await run(['import', path.join(out, 'interaction.initial.subset.html'), '--html-no-snapshot',
    '-o', path.join(dir, 'direct.pagx'), '--render', '--images', 'embed']);
  const captured = PNG.sync.read(fs.readFileSync(path.join(out, 'interaction.initial.png')));
  const direct = PNG.sync.read(fs.readFileSync(path.join(dir, 'direct.png')));
  expect(direct.data.equals(captured.data)).toBe(true);
  const selected = path.join(dir, 'selected.pagx');
  await run(['import', fixture, '--html-scenario', scenario, '--html-checkpoint', 'advanced',
    '-o', selected, '--preview']);
  const selectedManifest = JSON.parse(fs.readFileSync(path.join(dir, 'selected.captures.json')));
  expect(selectedManifest.outputs).toHaveLength(1);
  expect(selectedManifest.outputs[0]).toMatchObject({ name: 'advanced', pagx: selected, subsetHtml: null });
  expect(selectedManifest.preview.pages[0].items).toEqual(['advanced']);
});

nativeTest('multi-result file output and invalid arguments fail before browser startup', async () => {
  const env = { PUPPETEER_EXECUTABLE_PATH: '/missing-browser' };
  await expect(run(['import', fixture, '--html-scenario', scenario, '-o', path.join(dir, 'all.pagx')], env))
    .rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('multiple checkpoints require --output-dir') });
  for (const flags of [['--scenario', scenario], ['--browser-engine', 'puppeteer'],
    ['--html-checkpoint', 'ready'], ['--preview', '--no-resolve'], ['--output-dir', dir, '-o', 'x.pagx'],
    ['--html-no-snapshot', '--html-scenario', scenario], ['--preview-columns', '3'],
    ['--html-scenario', scenario, '--html-viewport-width', '800']]) {
    await expect(run(['import', fixture, ...flags], env)).rejects.toMatchObject({ code: 2 });
  }
  expect(fs.readdirSync(dir)).toEqual([]);
});

nativeTest('SVG artifacts need no browser and ordinary HTML defaults to PAGX only', async () => {
  const svg = path.join(dir, 'icon.svg');
  fs.writeFileSync(svg, '<svg width="16" height="12"><rect width="16" height="12" fill="red"/></svg>');
  await run(['import', svg, '--output-dir', dir, '--preview'], { PUPPETEER_EXECUTABLE_PATH: '/missing' });
  expect(PNG.sync.read(fs.readFileSync(path.join(dir, 'icon.png'))).width).toBe(16);
  await expect(run(['import', svg, '--html-browser-engine', 'puppeteer'])).rejects.toMatchObject({ code: 2 });
  await run(['import', fixture, '-o', path.join(dir, 'plain.pagx')]);
  expect(fs.existsSync(path.join(dir, 'plain.pagx'))).toBe(true);
  expect(fs.existsSync(path.join(dir, 'plain.png'))).toBe(false);
  expect(fs.existsSync(path.join(dir, 'plain.subset.html'))).toBe(false);
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
    await run(['import', `http://127.0.0.1:${server.address().port}/`, '--output-name', 'web',
      '--html-scenario', plan, '--output-dir', dir, '--preview']);
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
  await expect(run(['import', fixture, '--html-scenario', plan, '--output-dir', dir, '--render']))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('Scenario step 2') });
  const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'interaction.captures.json')));
  expect(manifest).toMatchObject({ status: 'failed', outputs: [{ name: 'initial' }] });
  expect(manifest.outputs).toHaveLength(1);
  expect(fs.existsSync(manifest.outputs[0].pagx)).toBe(true);
  expect(PNG.sync.read(fs.readFileSync(manifest.outputs[0].png)).width).toBe(320);
});

nativeTest('ordinary and batch previews use current outputs and complete-artifact skip records', async () => {
  const input = path.join(dir, 'pages'), out = path.join(dir, 'batch'), single = path.join(dir, 'single');
  fs.mkdirSync(input);
  fs.writeFileSync(path.join(input, 'a # 中文.html'), '<!doctype html><body style="margin:0;background:red">First</body>');
  fs.writeFileSync(path.join(input, 'b.html'), '<!doctype html><body style="margin:0;background:blue">Second</body>');
  await run(['import', path.join(input, 'b.html'), '--output-dir', single, '--preview']);
  expect(fs.existsSync(path.join(single, 'b.subset.html'))).toBe(false);
  expect(PNG.sync.read(fs.readFileSync(path.join(single, 'b.preview/sheet-001.png'))).width).toBeGreaterThan(0);
  await run(['import', '--html-batch', input, '--output-dir', out, '--preview']);
  expect(fs.existsSync(path.join(out, 'a # 中文.pagx'))).toBe(true);
  expect(fs.existsSync(path.join(out, 'b.pagx'))).toBe(true);
  expect(PNG.sync.read(fs.readFileSync(path.join(out, 'preview/sheet-001.png'))).width).toBeGreaterThan(0);
  const batchArgs = ['import', '--html-batch', input, '--output-dir', out, '--preview', '--skip-existing'];
  expect((await run(batchArgs)).stderr).toContain('processed=0 skipped=2');
  fs.unlinkSync(path.join(out, 'b.png'));
  expect((await run(batchArgs)).stderr).toContain('processed=1 skipped=1');
  expect(fs.existsSync(path.join(out, 'b.png'))).toBe(true);

});

(process.platform !== 'win32' ? nativeTest : test.skip)('SIGTERM cancels the active session and records failure', async () => {
  const plan = path.join(dir, 'waiting.json');
  fs.writeFileSync(plan, JSON.stringify({ version: 1, steps: [
    { type: 'waitFor', target: { css: '#never' }, state: 'visible', timeoutMs: 30000 },
    { type: 'capture', name: 'done' },
  ] }));
  const child = spawn(pagx, ['import', fixture, '--html-scenario', plan, '--output-dir', dir], {
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
