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

test.each(['viewport', 'document'])('unsupported %s content preserves light DOM without screenshots', async extent => {
  const page = await newPage(source, { viewport: { width: 120, height: 90 } });
  try {
    await page.setContent(`<!doctype html><body style="height:300px">
      <custom-box style="display:block"><span>Retained child</span></custom-box>
      <iframe style="width:40px;height:30px" srcdoc="<script>parent.frameRan=true</script>"></iframe>`);
    const screenshot = jest.spyOn(page, 'screenshot').mockRejectedValue(new Error('Screenshot must not run'));
    const state = await page.evaluate(collectCurrentState);
    expect(state.html).not.toMatch(/<iframe|srcdoc=|<script/);
    const result = await captureCurrentPage(page, { mirror, extent });
    expect(result.html.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ')).toContain('Retained child');
    expect(result.warnings).toEqual(expect.arrayContaining([
      expect.stringContaining('custom-box'), expect.stringContaining('iframe'),
    ]));
    expect(screenshot).not.toHaveBeenCalled();
    await expect(captureCurrentPage(page, { mirror: source })).rejects.toThrow('separate browser');
  } finally { await page.close(); }
});

test('static copies warn about geometry and missing nodes while version errors remain fatal', async () => {
  const page = await newPage(source, { viewport: { width: 120, height: 90 } });
  try {
    await page.setContent('<!doctype html><div id="box" style="width:30px;height:20px">text</div>');
    const state = await page.evaluate(collectCurrentState);
    const before = (await mirror.browser.pages()).length;
    await expect(renderCapturedPage(mirror, { ...state, version: 2 })).rejects.toThrow('version');
    const limited = await renderCapturedPage(mirror, { ...state, unsupported: ['unsupported fixture', 'unsupported fixture'] });
    expect(limited.warnings).toEqual(['unsupported fixture']);
    expect(limited.html).toContain('text');
    const box = state.boxes.find(box => box.tag === 'div#box');
    box.rect[2] += 10;
    const changed = await renderCapturedPage(mirror, state);
    expect(changed.warnings).toEqual(expect.arrayContaining([expect.stringContaining('changed layout')]));
    expect(changed.html).toContain('text');
    box.id = 9999;
    const missing = await renderCapturedPage(mirror, state);
    expect(missing.warnings).toEqual(expect.arrayContaining([expect.stringContaining('missing from static copy')]));
    expect(missing.html).toContain('text');
    expect((await mirror.browser.pages()).length).toBe(before);
  } finally { await page.close(); }
});

test('collector retains live form values while removing executable content and attributes', async () => {
  const page = await newPage(source, { viewport: { width: 320, height: 200 } });
  try {
    await page.setContent(`<!doctype html><textarea id="text">old</textarea>
      <select id="select"><option selected>a</option><option>b</option></select>
      <input id="checked" type="checkbox"><input id="unchecked" type="checkbox" checked>
      <input id="input" autofocus value="old" onclick="window.clicked=true">
      <canvas id="canvas" width="10" height="10"></canvas><script>window.executed=1</script>`);
    await page.evaluate(() => {
      document.querySelector('#text').value = 'live <value> & text';
      document.querySelector('#input').value = 'edited';
      document.querySelector('#select').selectedIndex = 1;
      document.querySelector('#checked').checked = true;
      document.querySelector('#unchecked').checked = false;
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
      return { options: [...doc.querySelectorAll('option')].map(option => option.hasAttribute('selected')),
        checked: doc.querySelector('#checked').checked, unchecked: doc.querySelector('#unchecked').checked };
    }, state.html);
    expect(selected).toEqual({ options: [false, true], checked: true, unchecked: false });
  } finally { await page.close(); }
});

test('font source lists keep captured alternatives and omit missing URLs', async () => {
  const page = await newPage(source, { viewport: { width: 320, height: 200 } });
  const font = fs.readFileSync(path.resolve(__dirname, '../../../resources/html/corpus/cli/spec/glyph_run/fonts/font_f0.woff2'));
  const uri = `data:font/woff2;base64,${font.toString('base64')}`;
  try {
    await page.setContent(`<!doctype html><style>@font-face { font-family: CapturedFont;
      src: local("missing, local font"), url("http://127.0.0.1:1/missing.woff2") format("woff2"), url("${uri}") format("woff2");
    } p { font:16px CapturedFont }</style><p>Captured font</p>`);
    await page.evaluate(() => document.fonts.ready);
    const state = await page.evaluate(collectCurrentState);
    expect(state.html).toContain(uri);
    expect(state.html).toContain('local("missing, local font")');
    expect(state.html).not.toContain('http://127.0.0.1:1/missing.woff2');
    const result = await renderCapturedPage(mirror, state);
    expect(result.warnings).toContain('Uncaptured resource omitted: http://127.0.0.1:1/missing.woff2');
    expect(result.html).toContain('Captured font');
  } finally { await page.close(); }
});

test('failed fonts fall back with warnings instead of blocking static content', async () => {
  const page = await newPage(source, { viewport: { width: 320, height: 200 } });
  try {
    await page.setContent(`<!doctype html><style>@font-face { font-family: BrokenFont;
      src:url(data:font/woff2;base64,invalid) } p { font:16px BrokenFont }</style><p>Fallback content</p>`);
    await page.evaluate(() => document.fonts.ready);
    const result = await captureCurrentPage(page, { mirror });
    expect(result.warnings).toEqual(expect.arrayContaining([expect.stringMatching(/font BrokenFont.*failed to load/)]));
    expect(result.html).toContain('Fallback content');
  } finally { await page.close(); }
});

test('older payloads cannot execute embedded scripts or pass blocked image URLs to import', async () => {
  const page = await newPage(source, { viewport: { width: 320, height: 200 } });
  try {
    await page.setContent('<!doctype html><p id="content">Original</p>');
    const state = await page.evaluate(collectCurrentState);
    state.unsupported = ['iframe: embedded content omitted'];
    state.html = state.html.replace('</head>', `<style>#content::before { content:""; display:block;
      width:10px; height:10px; background-image:url(http://127.0.0.1:1/pseudo.png) }</style></head>`);
    state.html = state.html.replace('</body>', `<iframe srcdoc="<script>parent.document.querySelector('#content').textContent='Mutated'</script>"></iframe>
      <img width="10" height="10" src="http://127.0.0.1:1/missing.png">
      <div style="width:10px;height:10px;background-image:url(http://127.0.0.1:1/missing.png)"></div></body>`);
    const result = await renderCapturedPage(mirror, state);
    expect(result.html).toContain('Original');
    expect(result.html).not.toContain('Mutated');
    expect(result.html).not.toContain('http://127.0.0.1:1/missing.png');
    expect(result.html).not.toContain('http://127.0.0.1:1/pseudo.png');
    expect(result.warnings).toContain('Uncaptured resource omitted: http://127.0.0.1:1/missing.png');
    expect(result.warnings).toContain('Uncaptured resource omitted: http://127.0.0.1:1/pseudo.png');
  } finally { await page.close(); }
});

test('local SVG references survive resource cleanup and missing external SVG images are omitted', async () => {
  const page = await newPage(source, { viewport: { width: 320, height: 200 } });
  try {
    await page.setContent(`<!doctype html><svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
      <defs><clipPath id="clip"><rect width="20" height="20"/></clipPath></defs>
      <rect width="50" height="50" fill="red" style="clip-path:url(#clip)"/>
      <image width="10" height="10" href="http://127.0.0.1:1/missing.svg"/>
    </svg>`);
    const state = await page.evaluate(collectCurrentState);
    expect(state.html).toContain('url(&quot;#clip&quot;)');
    expect(state.html).not.toContain('http://127.0.0.1:1/missing.svg');
    const result = await renderCapturedPage(mirror, state);
    expect(result.warnings.some(warning => warning.includes('about:blank'))).toBe(false);
    expect(result.html).toContain('clip');
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
