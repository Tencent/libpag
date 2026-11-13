/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//  Licensed under the Apache License, Version 2.0.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const viewerDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const rootDir = path.resolve(viewerDir, '../..');
const require = createRequire(path.join(rootDir, 'tools/html-snapshot/package.json'));
const { chromium } = require('playwright');
const args = process.argv.slice(2);
const recordPath = args.includes('--record-reference') ? args[args.indexOf('--record-reference') + 1] : null;
const referencePath = args.includes('--reference') ? args[args.indexOf('--reference') + 1] : null;
const infix = args.includes('--st') ? '.st' : '';

const html = `<!doctype html><canvas id="canvas" width="300" height="240"></canvas>
<script type="module">
import { PAGXInit } from '/lib/pagx-viewer${infix}.esm.js';
try {
  const module = await PAGXInit({locateFile: file => '/lib/' + file});
  window.view = module.PAGXView.init('#canvas');
  window.canvas = document.getElementById('canvas');
  view.updateSize();
  window.gl = canvas.getContext('webgl2');
  window.gpuCalls = 0;
  for (const name of ['drawArrays', 'drawElements', 'clear']) {
    const original = gl[name].bind(gl);
    gl[name] = function(...args) { window.gpuCalls++; return original(...args); };
  }
  window.capture = function() {
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    const pixels = new Uint8Array(canvas.width * canvas.height * 4);
    gl.readPixels(0, 0, canvas.width, canvas.height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    return {width: canvas.width, height: canvas.height, pixels: Array.from(pixels),
            contentWidth: view.contentWidth, contentHeight: view.contentHeight};
  };
  window.ready = true;
} catch (error) { window.failure = String(error); }
</script>`;

const server = createServer(async (req, res) => {
  res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
  res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
  try {
    const pathname = new URL(req.url, 'http://localhost').pathname;
    if (pathname === '/') {
      res.setHeader('Content-Type', 'text/html');
      res.end(html);
      return;
    }
    if (!pathname.startsWith('/lib/')) {
      res.writeHead(404).end();
      return;
    }
    const filename = path.resolve(viewerDir, '.' + pathname);
    if (!filename.startsWith(path.join(viewerDir, 'lib') + path.sep)) {
      res.writeHead(403).end();
      return;
    }
    res.setHeader('Content-Type', filename.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
    res.end(await readFile(filename));
  } catch {
    res.writeHead(404).end();
  }
});

function Layer(id, x, y, width, height, color, styles = '', roundness = 0, attributes = '') {
  return `<Layer id="${id}" x="${x}" y="${y}" width="${width}" height="${height}" ${attributes}>
    <Rectangle width="100%" height="100%" roundness="${roundness}"/>
    <Fill color="${color}"/>${styles}</Layer>`;
}

function Document(content, bounds = '90,110,100,60', attributes = '') {
  return `<pagx width="1000" height="1000" targetLayer="@target" outputBounds="${bounds}" ${attributes}>${content}</pagx>`;
}

function Pixel(image, x, y) {
  const offset = ((image.height - 1 - y) * image.width + x) * 4;
  return image.pixels.slice(offset, offset + 4);
}

function Hash(image) {
  return createHash('sha256').update(Buffer.from(image.pixels)).digest('hex');
}

function ExpectTransparent(image, x, y) {
  assert.equal(Pixel(image, x, y)[3], 0, `(${x},${y}) 应透明，实际 RGBA=${Pixel(image, x, y)}，内容尺寸=${image.contentWidth}×${image.contentHeight}`);
}

async function Render(page, xml, options = {}) {
  return page.evaluate(({ xml, options }) => {
    view.pause();
    canvas.width = options.width ?? 300;
    canvas.height = options.height ?? 240;
    view.updateSize();
    view.updateZoomScaleAndOffset(1, 0, 0);
    view.clearBackgroundColor();
    view.loadPAGX(new TextEncoder().encode(xml));
    view.pause();
    view.updateZoomScaleAndOffset(options.zoom ?? 1, options.x ?? 0, options.y ?? 0);
    if (options.background) view.setBackgroundColor(options.background);
    if (options.time !== undefined) view.setCurrentTimeMicros(options.time);
    for (let frame = 0; frame < 8; frame++) view.draw();
    return capture();
  }, { xml, options });
}

let browser;
try {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({ headless: true, args: ['--enable-webgl', '--use-gl=angle', '--use-angle=swiftshader'] });
  const page = await browser.newPage();
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(() => window.ready || window.failure);
  assert.equal(await page.evaluate(() => window.failure), undefined);
  let passed = 0;
  const animation = await readFile(path.join(rootDir, 'spec/samples/animation.pagx'), 'utf8');
  const plain = `<pagx width="200" height="120">${Layer('bg', 0, 0, 200, 120, '#204080')}${Layer('shape', 40, 30, 80, 40, '#FF804080', '', 8)}</pagx>`;
  const controls = {};
  for (const [name, xml, options] of [
    ['plain', plain, {}], ['plain-background', plain, { background: '#AABBCC' }],
    ['plain-zoom', plain, { zoom: 2, x: -90, y: -50 }],
    ['animation-start', animation, { time: 0 }], ['animation-seek', animation, { time: 500000 }],
  ]) {
    const image = await Render(page, xml, options);
    controls[name] = { hash: Hash(image), width: image.contentWidth, height: image.contentHeight };
  }
  if (recordPath) {
    await writeFile(recordPath, JSON.stringify(controls, null, 2));
    console.log(`已记录 ${Object.keys(controls).length} 个无字段像素参考：${recordPath}`);
  } else {
    if (referencePath) {
      assert.deepEqual(controls, JSON.parse(await readFile(referencePath, 'utf8')), '无字段文档渲染必须逐像素不变');
      passed += Object.keys(controls).length;
    }
    assert.notEqual(controls['animation-start'].hash, controls['animation-seek'].hash, '普通动画仍可 seek');
    passed++;
    const target = Layer('target', 100, 120, 80, 40, '#FF0000', '', 8);
    const surrounding = Layer('bg', 0, 0, 1000, 1000, '#0000FF') + target + Layer('later', 100, 120, 80, 40, '#00FF00');
    const base = await Render(page, Document(surrounding));
    if (args.includes('--diagnose')) {
      console.log('输出视图诊断', await page.evaluate(() => gl.getContextAttributes()),
        [Pixel(base, 10, 40), Pixel(base, 150, 10), Pixel(base, 150, 120), Pixel(base, 31, 61)]);
    }
    assert.equal(base.contentWidth, 100);
    assert.equal(base.contentHeight, 60);
    assert.deepEqual(Pixel(base, 150, 120), [255, 0, 0, 255]);
    ExpectTransparent(base, 10, 40);
    ExpectTransparent(base, 31, 61);
    ExpectTransparent(base, 150, 10);
    passed++;

    const negative = await Render(page, Document(Layer('target', -40, -30, 80, 40, '#FF0000', '', 8), '-50,-40,100,60'));
    assert.equal(Hash(negative), Hash(base), '负原点映射应与正原点一致');
    const nested = await Render(page, Document(`<Layer x="-100" y="-100"><Layer x="60" y="70">${Layer('target', 0, 0, 80, 40, '#FF0000', '', 8)}</Layer></Layer>`, '-50,-40,100,60'));
    assert.equal(Hash(nested), Hash(base), '祖先坐标应仅应用一次');
    passed += 2;

    const large = Document(Layer('target', 50, 70, 180, 160, '#FF0000'));
    const clipped = await Render(page, large);
    assert.deepEqual(Pixel(clipped, 150, 120), [255, 0, 0, 255]);
    ExpectTransparent(clipped, 150, 10);
    ExpectTransparent(clipped, 150, 220);
    const panned = await Render(page, large, { x: 40, y: 20 });
    ExpectTransparent(panned, 10, 120);
    assert.deepEqual(Pixel(panned, 60, 120), [255, 0, 0, 255]);
    const resized = await Render(page, large, { width: 400, height: 300 });
    ExpectTransparent(resized, 200, 10);
    ExpectTransparent(resized, 200, 290);
    const zoomed = await Render(page, large, { zoom: 2, x: -100, y: -250 });
    assert.deepEqual(Pixel(zoomed, 150, 120), [255, 0, 0, 255]);
    ExpectTransparent(zoomed, 150, 239);
    passed += 4;

    const idleCalls = await page.evaluate(() => {
      const before = gpuCalls;
      view.play();
      for (let i = 0; i < 20; i++) view.draw();
      return gpuCalls - before;
    });
    assert.equal(idleCalls, 0, '静态视图空闲时不应持续提交 GPU');
    const repeat = await page.evaluate(() => {
      view.updateZoomScaleAndOffset(2, -100, -250);
      view.draw();
      return capture();
    });
    assert.equal(Hash(repeat), Hash(zoomed), '重复刷新不应累积矩阵');
    passed += 2;

    const alpha = await Render(page, Document(Layer('target', 100, 120, 80, 40, '#FF0000', '', 0, 'alpha="0.5"')));
    assert.ok(Math.abs(Pixel(alpha, 150, 120)[3] - 128) <= 1, '目标 alpha 恰好应用一次');
    const hidden = await Render(page, Document(Layer('target', 100, 120, 80, 40, '#FF0000', '', 0, 'visible="false"')));
    assert.ok(hidden.pixels.every((value, index) => index % 4 !== 3 || value === 0), '合法隐藏目标保持透明');
    const matte = await Render(page, Document(target), { background: '#00FF00' });
    assert.deepEqual(Pixel(matte, 10, 10), [0, 255, 0, 255]);
    const cleared = await page.evaluate(() => { view.clearBackgroundColor(); view.clear(); return capture(); });
    assert.equal(cleared.contentWidth, 0);
    assert.equal(cleared.contentHeight, 0);
    assert.ok(cleared.pixels.every((value, index) => index % 4 !== 3 || value === 0), 'clear 不能残留旧图');
    passed += 4;

    const invalid = [
      `<pagx width="1000" height="1000" targetLayer="@target">${target}</pagx>`,
      `<pagx width="1000" height="1000" outputBounds="90,110,100,60">${target}</pagx>`,
      Document(target).replace('@target', '@missing'),
      Document(target, '0,0,-1,60'), Document(target, 'NaN,0,100,60'),
      animation.replace('<pagx width="400" height="400">', '<pagx width="400" height="400" targetLayer="@pulseCircle" outputBounds="0,0,400,400">'),
    ];
    for (const [index, xml] of invalid.entries()) {
      for (const split of [false, true]) {
        await Render(page, Document(target));
        const result = await page.evaluate(({ xml, split }) => {
          let error = '';
          try {
            if (split) { view.parsePAGX(new TextEncoder().encode(xml)); view.buildLayers(); }
            else view.loadPAGX(new TextEncoder().encode(xml));
          } catch (failure) { error = String(failure); }
          return { error, image: capture() };
        }, { xml, split });
        assert.ok(result.error, `非法输入 ${index} 必须诊断失败`);
        assert.equal(result.image.contentWidth, 0);
        assert.ok(result.image.pixels.every((value, offset) => offset % 4 !== 3 || value === 0), '失败不能显示旧图或整页');
        passed++;
      }
    }
    const after = await Render(page, plain);
    assert.equal(Hash(after), controls.plain.hash, '切回无字段文档恢复原渲染');
    passed++;
    console.log(`输出视图浏览器回归：${passed} 项通过，WebGL2 / Chromium。`);
  }
} finally {
  if (browser) await browser.close();
  await new Promise(resolve => server.close(resolve));
}
