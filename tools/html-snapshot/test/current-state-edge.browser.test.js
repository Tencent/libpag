// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { PNG } = require('pngjs');
const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const { captureCurrentPage, collectCurrentState, renderCapturedPage } = require('../dist/lib/current-state');
const { runScenario } = require('../dist/lib/interaction');
jest.setTimeout(60000);
let source, mirror;
beforeAll(async () => { source = await launchBrowser(); mirror = await launchBrowser(); });
afterAll(async () => {
  if (source) await source.browser.close();
  if (mirror) await mirror.browser.close();
});

test('raster fallback is well formed and retains the complete viewport pixels', async () => {
  const page = await newPage(source, { viewport: { width: 120, height: 90 } });
  try {
    await page.setContent('<!doctype html><input type="checkbox" checked><div style="width:40px;height:30px;background:red"></div>');
    const pixels = PNG.sync.read(Buffer.from(await page.screenshot()));
    const result = await captureCurrentPage(page, { mirror, unsupported: 'rasterize' });
    // The 10/19 regression left the <img> start tag open. Check XML parsing as well
    // as pixels: HTML error recovery can otherwise hide a broken serialization.
    expect(result.html).toMatch(/<img\s[^>]+\/>/);
    const valid = await page.evaluate(html => {
      const xml = new DOMParser().parseFromString(html, 'application/xhtml+xml');
      return !xml.querySelector('parsererror') && xml.querySelectorAll('img').length === 1;
    }, result.html);
    expect(valid).toBe(true);
    const encoded = result.html.match(/src="data:image\/png;base64,([^\"]+)"/)[1];
    const raster = PNG.sync.read(Buffer.from(encoded, 'base64'));
    expect([raster.width, raster.height]).toEqual([120, 90]);
    expect(raster.data.equals(pixels.data)).toBe(true);
    await expect(captureCurrentPage(page, { mirror, unsupported: 'rasterize', extent: 'document' }))
      .rejects.toThrow('checkbox control');
    await expect(captureCurrentPage(page, { mirror: source })).rejects.toThrow('separate browser');
  } finally { await page.close(); }
});

test('static copies reject version mismatches and changed geometry without leaking pages', async () => {
  const page = await newPage(source, { viewport: { width: 120, height: 90 } });
  try {
    await page.setContent('<!doctype html><div id="box" style="width:30px;height:20px">text</div>');
    const state = await page.evaluate(collectCurrentState);
    const before = (await mirror.browser.pages()).length;
    await expect(renderCapturedPage(mirror, { ...state, version: 2 })).rejects.toThrow('version');
    await expect(renderCapturedPage(mirror, { ...state, unsupported: ['unsupported fixture'] })).rejects.toThrow('unsupported fixture');
    const box = state.boxes.find(box => box.tag === 'div#box');
    box.rect[2] += 10;
    await expect(renderCapturedPage(mirror, state)).rejects.toThrow('changed layout');
    box.id = 9999;
    await expect(renderCapturedPage(mirror, state)).rejects.toThrow('missing from static copy');
    expect((await mirror.browser.pages()).length).toBe(before);
  } finally { await page.close(); }
});

test('collector retains live form values while removing executable content and attributes', async () => {
  const page = await newPage(source, { viewport: { width: 320, height: 200 } });
  try {
    await page.setContent(`<!doctype html><textarea id="text">old</textarea>
      <select id="select"><option selected>a</option><option>b</option></select>
      <input id="input" autofocus value="old" onclick="window.clicked=true">
      <canvas id="canvas" width="10" height="10"></canvas><script>window.executed=1</script>`);
    await page.evaluate(() => {
      document.querySelector('#text').value = 'live <value> & text';
      document.querySelector('#input').value = 'edited';
      document.querySelector('#select').selectedIndex = 1;
      const ctx = document.querySelector('#canvas').getContext('2d');
      ctx.fillStyle = 'red'; ctx.fillRect(0, 0, 10, 10);
    });
    const state = await page.evaluate(collectCurrentState);
    expect(state.html).not.toMatch(/<script|onclick=|autofocus/);
    expect(state.html).toContain('live &lt;value&gt; &amp; text');
    expect(state.html).toContain('value="edited"');
    expect(state.html).toContain('data:image/png;base64,');
    expect(await page.evaluate(() => window.executed)).toBe(1);
    const selected = await page.evaluate(html => {
      const doc = new DOMParser().parseFromString(html, 'text/html');
      return [...doc.querySelectorAll('option')].map(option => option.hasAttribute('selected'));
    }, state.html);
    expect(selected).toEqual([false, true]);
  } finally { await page.close(); }
});

test('scenario downloads deduplicated image bytes and safely escapes their filesystem paths', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'capture & "images-'));
  const png = new PNG({ width: 2, height: 2 });
  png.data.fill(255);
  const uri = `data:image/png;base64,${PNG.sync.write(png).toString('base64')}`;
  try {
    const html = `<!doctype html><img src="${uri}"><img src="${uri}">`;
    const results = await runScenario(source, `data:text/html,${encodeURIComponent(html)}`,
      { version: 1, stableMs: 0, steps: [{ type: 'capture', name: 'images' }] },
      { downloadImages: true, imageDir: dir });
    const files = fs.readdirSync(dir);
    expect(files).toHaveLength(1);
    const saved = PNG.sync.read(fs.readFileSync(path.join(dir, files[0])));
    expect(saved.data.equals(png.data)).toBe(true);
    expect(results[0].html).not.toContain('data:image/png;base64,');
    expect(results[0].html).toContain('&amp;');
    expect(results[0].html).toContain('&quot;');
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
});
