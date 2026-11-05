// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFile } = require('child_process');
const { promisify } = require('util');
const { PNG } = require('pngjs');
const { writeZip } = require('./archive-helpers');
const exec = promisify(execFile), root = path.resolve(__dirname, '../../..');
const binary = process.env.PAGX_TEST_BIN || path.join(root, 'cmake-build-debug/pagx');
const nativeTest = fs.existsSync(binary) ? test : test.skip;
jest.setTimeout(180000);
let dir, archive;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-native 中文 # ')); archive = path.join(dir, 'site.zip'); });
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));
const run = (args, env = {}) => exec(binary, args, { timeout: 120000, maxBuffer: 8 * 1024 * 1024, cwd: dir,
  env: { ...process.env, PAGX_NODE_BIN: process.execPath, PAGX_IMPORT_BIN: path.resolve(__dirname, '../import.js'),
    HTML_SNAPSHOT_NO_AUTOSCROLL: '1', HTML_SNAPSHOT_TIMEOUT_MS: '30000', HTML_SNAPSHOT_SETTLE_TIMEOUT_MS: '20000', ...env } });

function entries(color = 'red') {
  const png = new PNG({ width: 2, height: 2 }); png.data.fill(255);
  const html = '<!doctype html><meta charset="utf-8"><base href="/"><link rel="stylesheet" href="css/style.css">' +
    '<body><div id="state"></div><span>שלום</span><img src="images/%E4%B8%AD%E6%96%87%20%23%25.png" width="20" height="20">' +
    '<script type="module" src="scripts/main.mjs"></script></body>';
  return { 'site/index.html': html, 'site/nested/孤立 #%.htm': html,
    'site/css/style.css': `@import './extra.css'; body { width:320px;height:180px;margin:0;background:${color};font-family:ZipFace; }`,
    'site/css/extra.css': '@font-face { font-family:ZipFace;src:url(../fonts/local.ttf); } #state {font-size:20px}',
    'site/fonts/local.ttf': fs.readFileSync(path.join(root, 'resources/font/NotoSansHebrew-Regular.ttf')),
    'site/images/中文 #%.png': PNG.sync.write(png), 'site/data.json': '{"value":42}',
    'site/scripts/main.mjs': "const {label}=await import('./label.mjs');const data=await fetch('/data.json').then(r=>r.json());" +
      "document.querySelector('#state').textContent=label+' '+data.value+' '+(localStorage.getItem('seen')||'Fresh');" +
      "localStorage.setItem('seen','Leaked');document.body.dataset.ready='yes';",
    'site/scripts/label.mjs': "export const label='Ready';" };
}

nativeTest.each(['puppeteer', 'playwright'])('%s imports nested pages, modules, fonts and images; outputs survive source removal', async engine => {
  await writeZip(archive, entries());
  const output = path.join(dir, 'out');
  const args = ['import', archive, '--html-root', 'site', '--output-dir', output, '--render', '--html-keep-subset',
    '--html-download-images', '--html-browser-engine', engine, '--html-selector', 'body[data-ready=yes]',
    '--html-wait-ms', '0', '--html-viewport-width', '320', '--html-viewport-height', '180'];
  await run(args);
  const manifestFile = path.join(output, 'import-manifest.json');
  let manifest = JSON.parse(fs.readFileSync(manifestFile));
  expect(manifest.status).toBe('complete'); expect(manifest.outputs).toHaveLength(2);
  for (const item of manifest.outputs) {
    const xml = fs.readFileSync(path.join(output, item.pagx), 'utf8');
    expect(xml).toContain('Ready 42 Fresh'); expect(xml).not.toContain('Leaked');
    expect(xml).toContain('<Font'); expect(xml).not.toContain('127.0.0.1'); expect(xml).not.toContain('pagx-website-');
    const subset = fs.readFileSync(path.join(output, item.source + '.subset.html'), 'utf8');
    expect(subset).not.toContain('127.0.0.1'); expect(subset).not.toContain(dir);
  }
  expect((await run([...args, '--skip-existing'])).stderr).toContain('processed=0 skipped=2');
  await writeZip(archive, entries('blue'));
  expect((await run([...args, '--skip-existing'])).stderr).toContain('processed=2 skipped=0');
  manifest = JSON.parse(fs.readFileSync(manifestFile));
  fs.unlinkSync(archive);
  for (const [index, item] of manifest.outputs.entries()) {
    const rendered = path.join(dir, `again-${index}.png`);
    await run(['render', path.join(output, item.pagx), '--output', rendered]);
    const before = PNG.sync.read(fs.readFileSync(path.join(output, item.png)));
    const after = PNG.sync.read(fs.readFileSync(rendered));
    expect(after.data.equals(before.data)).toBe(true);
  }
});

nativeTest('ZIP preflight rejects invalid input and multi-page -o before browser initialization', async () => {
  await writeZip(archive, { 'a.html': 'a', 'b.html': 'b' });
  const env = { PUPPETEER_EXECUTABLE_PATH: '/missing-browser' };
  const output = path.join(dir, 'out');
  expect((await run(['import', archive, '--output-dir', output, '--dry-run'], env)).stderr).toContain('would process');
  expect(fs.existsSync(output)).toBe(false);
  await expect(run(['import', archive, '-o', path.join(dir, 'one.pagx')], env))
    .rejects.toMatchObject({ code: 2, stderr: expect.stringContaining('Multiple ZIP pages') });
  fs.writeFileSync(archive, 'not a ZIP');
  await expect(run(['import', archive, '--output-dir', output], env)).rejects.toMatchObject({ code: 2 });
  expect(fs.existsSync(output)).toBe(false);
});

nativeTest('records page failures and partial resources while continuing later pages', async () => {
  await writeZip(archive, {
    'a-failed.html': '<script>location.replace("/missing.html")</script>',
    'b-partial.html': '<body style="width:200px;height:100px"><img src="/missing.png" width="20" height="20">partial</body>',
    'c-good.html': '<body style="width:200px;height:100px">Surviving page</body>',
  });
  const output = path.join(dir, 'out');
  await expect(run(['import', archive, '--output-dir', output, '--html-wait-ms', '0']))
    .rejects.toMatchObject({ code: 1 });
  const manifest = JSON.parse(fs.readFileSync(path.join(output, 'import-manifest.json')));
  expect(manifest.outputs.map(item => item.status)).toEqual(['failed', 'partial', 'complete']);
  expect(fs.readFileSync(path.join(output, 'c-good.html.pagx'), 'utf8')).toContain('Surviving page');
  expect(manifest.outputs[1].warnings.join('\n')).toContain('missing.png');
});

nativeTest.each(['puppeteer', 'playwright'])('%s worker requests obey the archive origin policy', async engine => {
  let hits = 0;
  const external = http.createServer((_, res) => {
    hits++; res.setHeader('Access-Control-Allow-Origin', '*'); res.end('External allowed');
  });
  await new Promise(resolve => external.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${external.address().port}`;
  try {
    await writeZip(archive, {
      'index.html': '<body style="width:240px;height:100px"><div id="state"></div><script>' +
        'const w=new Worker("/worker.js");w.onmessage=e=>{document.querySelector("#state").textContent=e.data;document.body.dataset.ready="yes";};</script></body>',
      'worker.js': `fetch(${JSON.stringify(origin)}).then(r=>r.ok?r.text():Promise.reject()).then(t=>postMessage(t)).catch(()=>postMessage('External blocked'));`,
    });
    const args = ['import', archive, '--html-browser-engine', engine, '--html-wait-ms', '0', '--html-selector', 'body[data-ready=yes]'];
    const offline = path.join(dir, 'offline.pagx');
    await run([...args, '-o', offline]);
    expect(hits).toBe(0); expect(fs.readFileSync(offline, 'utf8')).toContain('External blocked');
    const online = path.join(dir, 'online.pagx');
    await run([...args, '-o', online, '--html-allow-origin', origin]);
    expect(hits).toBeGreaterThan(0); expect(fs.readFileSync(online, 'utf8')).toContain('External allowed');
  } finally { await new Promise(resolve => external.close(resolve)); }
});
