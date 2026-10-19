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
jest.setTimeout(90000);
const run = (args, env = {}) => exec(pagx, ['import', ...args], { cwd: dir, timeout: 80000, maxBuffer: 8 * 1024 * 1024,
  env: { ...process.env, PAGX_NODE_BIN: process.execPath, ...env } });
const read = file => fs.readFileSync(file, 'utf8');
const manifest = out => JSON.parse(read(path.join(out, 'manifest.json')));
const svg = file => { fs.writeFileSync(file, '<svg width="32" height="24"><rect width="32" height="24" fill="#ff0000"/></svg>'); return file; };

nativeTest('help and SVG conversion/rendering do not need Node or Chromium', async () => {
  const env = { PAGX_NODE_BIN: '/absent/node', PAGX_HTML_IMPORT_BIN: '/absent/adapter' };
  expect((await run(['--help'], env)).stdout).toContain('--output-dir');
  const input = svg(path.join(dir, 'icon.svg'));
  await run([input, '-o', 'icon.pagx', '--render'], env);
  expect(read(path.join(dir, 'icon.pagx'))).toContain('<pagx');
  const png = PNG.sync.read(fs.readFileSync(path.join(dir, 'icon.png')));
  expect([png.width, png.height]).toEqual([32, 24]);
  await expect(exec(pagx, ['capture', '--help'], { cwd: dir })).rejects.toMatchObject({
    stderr: expect.stringContaining("unknown command 'capture'"),
  });
});

nativeTest('scenario dry-run validates and plans all checkpoints without browser or output mutations', async () => {
  const out = path.join(dir, 'out');
  const result = await run([fixture, '--scenario', scenario, '--output-dir', out, '--preview', '--dry-run'],
    { PUPPETEER_EXECUTABLE_PATH: '/absent/browser' });
  const plan = JSON.parse(result.stdout);
  expect(plan.status).toBe('planned');
  expect(plan.outputs.map(o => o.checkpoint)).toEqual(['initial', 'advanced']);
  expect(fs.existsSync(out)).toBe(false);
  await expect(run([fixture, '--scenario', scenario, '-o', 'one.pagx']))
    .rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('--output-dir') });
  await expect(run([fixture, '--scenario', scenario, '--checkpoint', 'absent', '--output-dir', out]))
    .rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('Unknown checkpoint') });
  expect(fs.existsSync(out)).toBe(false);
});

nativeTest('scenario exports all states and preview uses only this run, including after overwrite', async () => {
  const out = path.join(dir, 'out');
  await run([fixture, '--scenario', scenario, '--output-dir', out, '--preview', '--keep-snapshot'],
    { PAGX_BIN: '/wrong/version', PAGX_HTML_SNAPSHOT: '0' });
  let data = manifest(out);
  expect(data.status).toBe('complete');
  expect(data.outputs.map(o => [o.checkpoint, o.status])).toEqual([['initial', 'complete'], ['advanced', 'complete']]);
  expect(data.preview.pages[0].items).toEqual(['initial', 'advanced']);
  const first = PNG.sync.read(fs.readFileSync(path.join(out, data.outputs[0].png)));
  const second = PNG.sync.read(fs.readFileSync(path.join(out, data.outputs[1].png)));
  expect(first.data.equals(second.data)).toBe(false);
  expect(read(path.join(out, data.outputs[1].snapshot))).toContain('PAGX');
  await run([fixture, '--scenario', scenario, '--checkpoint', 'advanced', '--output-dir', out, '--preview', '--overwrite']);
  data = manifest(out);
  expect(data.outputs.map(o => o.checkpoint)).toEqual(['advanced']);
  expect(data.preview.pages[0].items).toEqual(['advanced']);
  expect(fs.existsSync(path.join(out, 'interaction.initial.png'))).toBe(true);
});

nativeTest.each(['puppeteer', 'playwright'])('single checkpoint with %s has an exact file target and no default PNG', async engine => {
  const out = path.join(dir, 'selected.pagx');
  await run([fixture, '--scenario', scenario, '--checkpoint', 'advanced', '-o', out, '--browser-engine', engine]);
  expect(read(out)).toContain('PAGX');
  expect(fs.existsSync(path.join(dir, 'selected.png'))).toBe(false);
  expect(fs.readdirSync(dir)).toEqual(['selected.pagx']);
});

nativeTest('URL scenario visits the source once; preview does not revisit it', async () => {
  let visits = 0;
  const server = http.createServer((req, res) => {
    if (req.url === '/') visits++;
    res.setHeader('Content-Type', 'text/html');
    res.end('<!doctype html><body><button id="next" onclick="this.textContent=\'Done\'">Next</button></body>');
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  try {
    const states = path.join(dir, 'states.json'), out = path.join(dir, 'out');
    fs.writeFileSync(states, JSON.stringify({ version: 1, viewport: { width: 320, height: 200 }, steps: [
      { type: 'capture', name: 'initial' }, { type: 'click', target: { css: '#next' } }, { type: 'capture', name: 'done' },
    ] }));
    await run([`http://127.0.0.1:${server.address().port}/`, '--output-name', 'web', '--scenario', states,
      '--output-dir', out, '--preview']);
    expect(visits).toBe(1);
    expect(manifest(out).preview.pages[0].items).toEqual(['initial', 'done']);
  } finally { await new Promise(resolve => server.close(resolve)); }
});

nativeTest('failed later scenario step preserves already captured exports and marks failure', async () => {
  const states = path.join(dir, 'states.json'), out = path.join(dir, 'out');
  fs.writeFileSync(states, JSON.stringify({ version: 1, steps: [
    { type: 'capture', name: 'initial' },
    { type: 'waitFor', target: { css: '#never' }, state: 'visible', timeoutMs: 50 },
    { type: 'capture', name: 'unreachable' },
  ] }));
  await expect(run([fixture, '--scenario', states, '--output-dir', out])).rejects.toMatchObject({ code: 1 });
  const data = manifest(out);
  expect(data.status).toBe('failed');
  expect(data.outputs.map(o => o.status)).toEqual(['complete', 'failed']);
  expect(fs.existsSync(path.join(out, data.outputs[0].pagx))).toBe(true);
  expect(fs.existsSync(path.join(out, data.outputs[1].pagx))).toBe(false);
});

nativeTest('batch preserves relative paths and isolates storage between HTML inputs', async () => {
  const input = path.join(dir, 'input'), out = path.join(dir, 'out');
  fs.mkdirSync(path.join(input, 'nested'), { recursive: true });
  fs.writeFileSync(path.join(input, 'a.html'), '<!doctype html><body><div>First</div><script>localStorage.setItem("marker","DIRTY")</script></body>');
  fs.writeFileSync(path.join(input, 'b.html'), '<!doctype html><body><div id="value"></div><script>document.querySelector("#value").textContent=localStorage.getItem("marker")||"CLEAN"</script></body>');
  svg(path.join(input, 'nested/icon.svg'));
  await run(['--batch', input, '--output-dir', out]);
  const data = manifest(out);
  expect(data.status).toBe('complete');
  expect(data.outputs.map(o => o.pagx)).toEqual(['a.pagx', 'b.pagx', 'nested/icon.pagx']);
  expect(read(path.join(out, 'b.pagx'))).toContain('CLEAN');
  expect(read(path.join(out, 'b.pagx'))).not.toContain('DIRTY');
  expect(data.outputs.every(o => !o.png)).toBe(true);
});

nativeTest('batch collisions fail before browser startup or creating the output directory', async () => {
  const input = path.join(dir, 'input'), out = path.join(dir, 'out'); fs.mkdirSync(input);
  fs.writeFileSync(path.join(input, 'a.html'), '<body>html</body>'); svg(path.join(input, 'a.svg'));
  await expect(run(['--batch', input, '--output-dir', out], { PAGX_NODE_BIN: '/absent/node' }))
    .rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('collision') });
  expect(fs.existsSync(out)).toBe(false);
});

nativeTest('prepared HTML resolves original relative resources and honors both image storage modes', async () => {
  const input = path.join(dir, 'source'), out = path.join(dir, 'output'); fs.mkdirSync(input);
  const png = new PNG({ width: 2, height: 2 }); png.data.fill(255);
  fs.writeFileSync(path.join(input, 'pixel.png'), PNG.sync.write(png));
  const subset = path.join(input, 'page.html');
  fs.writeFileSync(subset, '<html><body style="width:20px;height:20px"><img src="pixel.png" style="width:20px;height:20px"/></body></html>');
  const env = { PAGX_NODE_BIN: '/absent/node' };
  await run([subset, '--html-source', 'snapshot', '--output-dir', out, '--render'], env);
  let data = manifest(out);
  expect(data.outputs[0].assets).toHaveLength(1);
  expect(read(path.join(out, 'page.pagx'))).toContain('page.assets/image-1.png');
  expect(fs.existsSync(path.join(out, data.outputs[0].assets[0]))).toBe(true);
  await run([subset, '--html-source', 'snapshot', '-o', path.join(dir, 'embedded.pagx'), '--images', 'embed', '--no-resolve'], env);
  expect(read(path.join(dir, 'embedded.pagx'))).toContain('data:image/png;base64,');
  fs.unlinkSync(path.join(input, 'pixel.png'));
  await expect(run([subset, '--html-source', 'snapshot', '-o', path.join(dir, 'missing.pagx'), '--no-resolve'], env))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('image resource') });
  expect(fs.existsSync(path.join(dir, 'missing.pagx'))).toBe(false);
});

nativeTest('existing files require overwrite, and failed imports preserve the old file', async () => {
  const input = svg(path.join(dir, 'input.svg')), out = path.join(dir, 'output.pagx');
  fs.writeFileSync(out, 'original');
  await expect(run([input, '-o', out])).rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('--overwrite') });
  expect(read(out)).toBe('original');
  fs.writeFileSync(input, 'invalid SVG');
  await expect(run([input, '-o', out, '--overwrite'])).rejects.toMatchObject({ code: 1 });
  expect(read(out)).toBe('original');
  svg(input);
  await run([input, '-o', out, '--overwrite']);
  expect(read(out)).toContain('<pagx');
});

(process.platform !== 'win32' ? nativeTest : test.skip).each([['SIGTERM', 143], ['SIGINT', 130]])('%s returns %i and records cancellation after browser cleanup', async (signal, expectedCode) => {
  const states = path.join(dir, 'states.json'), out = path.join(dir, 'out');
  fs.writeFileSync(states, JSON.stringify({ version: 1, steps: [
    { type: 'waitFor', target: { css: '#never' }, state: 'visible', timeoutMs: 30000 }, { type: 'capture', name: 'done' },
  ] }));
  const child = spawn(pagx, ['import', fixture, '--scenario', states, '--output-dir', out], {
    env: { ...process.env, PAGX_NODE_BIN: process.execPath }, stdio: ['ignore', 'pipe', 'pipe'],
  });
  let stderr = '', signalled = false;
  const timer = setTimeout(() => child.kill('SIGKILL'), 20000);
  child.stderr.on('data', chunk => {
    stderr += chunk;
    if (!signalled && stderr.includes('step 1/2: waitFor')) { signalled = true; child.kill(signal); }
  });
  try {
    const code = await new Promise((resolve, reject) => { child.on('error', reject); child.on('close', resolve); });
    expect(signalled).toBe(true);
    expect(code).toBe(expectedCode);
    const data = manifest(out);
    expect(data.status).toBe('cancelled');
    expect(data.outputs[0].status).toBe('cancelled');
    expect(fs.readdirSync(out)).toEqual(['manifest.json']);
  } finally { clearTimeout(timer); }
});

nativeTest('preview failure retains completed PAGX/PNG outputs and a failed manifest', async () => {
  const input = svg(path.join(dir, 'icon.svg')), out = path.join(dir, 'out');
  const adapter = path.join(dir, 'failing-preview.js');
  fs.writeFileSync(adapter, 'process.stderr.write("preview fixture failure\\n"); process.exit(7);');
  await expect(run([input, '--output-dir', out, '--preview'], { PAGX_HTML_IMPORT_BIN: adapter }))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('preview generation failed') });
  const data = manifest(out);
  expect(data.status).toBe('failed');
  expect(data.outputs[0].status).toBe('complete');
  expect(data.preview.status).toBe('failed');
  expect(fs.existsSync(path.join(out, data.outputs[0].pagx))).toBe(true);
  expect(fs.existsSync(path.join(out, data.outputs[0].png))).toBe(true);
});

nativeTest('native font embedding works for SVG without Node', async () => {
  const input = path.join(dir, 'text.svg');
  fs.writeFileSync(input, '<svg width="120" height="40"><text x="2" y="25" font-family="Arial" font-size="20">Hello</text></svg>');
  await run([input, '-o', 'text.pagx', '--embed-fonts', '--render'], { PAGX_NODE_BIN: '/absent/node' });
  const xml = read(path.join(dir, 'text.pagx'));
  expect(xml).toContain('<Glyph');
  expect(fs.existsSync(path.join(dir, 'text.png'))).toBe(true);
});

nativeTest('HTTP prepared-source adapter returns image bytes that survive temporary cleanup', async () => {
  const png = new PNG({ width: 2, height: 2 }); png.data.fill(255);
  const uri = 'data:image/png;base64,' + PNG.sync.write(png).toString('base64');
  const { runPagxImport } = require('../dist/lib/pagx-runner');
  const result = await runPagxImport({ pagxBin: pagx,
    html: `<html><body style="width:20px;height:20px"><img src="${uri}" style="width:20px;height:20px"/></body></html>` });
  expect(result).toContain(uri);
  expect(result).not.toContain('.assets/');
});
