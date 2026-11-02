'use strict';

const assert = require('node:assert/strict');
const { describe, it, before, after, beforeEach, afterEach } = require('node:test');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { captureControlPaints, installControlPaints } = require('../../dist/lib/control-capture');
const { capturePseudoContents, captureTextLayout, installTextLayout } = require('../../dist/lib/text-layout');
const { materializeDecorativePseudoElements, takeSnapshot } = require('../../dist/lib/browser-snapshot');

describe('form control capture', () => {
  let browser, page;
  before(async () => { ({ browser } = await launchBrowser()); });
  after(async () => { if (browser) await browser.close(); });
  beforeEach(async () => {
    page = await browser.newPage();
    await page.setViewport({ width: 600, height: 500, deviceScaleFactor: 2 });
  });
  afterEach(async () => { await page.close(); });

  async function load(html) {
    await page.setContent(`<style>body{margin:20px;font:16px Arial}
      input,textarea{font:16px Arial;box-sizing:border-box}</style>${html}`);
    await page.evaluate(() => document.fonts.ready);
  }

  it('exports the rendered password mask and live textarea line boxes without the secret', async () => {
    const secret = 'private-PASSWORD-你好';
    await load(`<div style="transform:rotate(15deg)">
      <input type=password value="old"><textarea style="width:130px;height:90px;padding:8px">
      old</textarea></div>`);
    await page.evaluate(value => {
      document.querySelector('input').value = value;
      document.querySelector('textarea').value = '  Alpha beta gamma delta\n第二行 description';
    }, secret);
    const data = await captureTextLayout(page, 'puppeteer');
    assert.ok(!JSON.stringify(data).includes(secret));
    assert.equal(data.controls.length, 2);
    assert.match(data.controls[0].fragments.map(f => f.text).join(''), /^[•●]+$/u);
    const fragments = data.controls[1].fragments;
    assert.ok(new Set(fragments.map(f => f.y)).size >= 3);
    assert.ok(fragments[0].x >= 8 && fragments[0].x < 12);
    await page.evaluate(installTextLayout, data);
    const result = await page.evaluate(takeSnapshot);
    assert.ok(!result.html.includes(secret));
    assert.ok(!result.html.includes('>old<'));
    assert.match(result.html, /white-space: pre/);
    assert.match(result.html, /overflow: hidden/);
    assert.match(result.html, /第二行/);
  });

  it('keeps a disabled control layout slot when no paint provider is installed', async () => {
    await load('<label style="display:flex;align-items:center;gap:6px"><input type=radio disabled>Frozen</label>');
    const result = await page.evaluate(takeSnapshot);
    assert.ok(result.warnings.some(w => w.includes('native paint unavailable')));
    await page.setContent(result.html);
    const boxes = await page.evaluate(() => {
      const text = [...document.querySelectorAll('span')].find(el => el.textContent === 'Frozen');
      const rect = text.getBoundingClientRect();
      return { x: rect.x - text.parentElement.getBoundingClientRect().x };
    });
    assert.ok(boxes.x >= 20, JSON.stringify(boxes));
  });

  it('isolates native paint from ancestors, siblings and through-effects, restoring the DOM', async () => {
    await load(`<div style="background:red;opacity:.4;filter:blur(1px);transform:rotate(12deg)">
      <input type=radio checked style="width:24px;height:24px;opacity:.5;accent-color:blue">
      <span style="position:absolute;inset:0;background:lime;visibility:visible!important"></span></div>`);
    const before = await page.content();
    const paints = await captureControlPaints(page, 'puppeteer');
    assert.equal(paints.length, 1);
    assert.equal(await page.content(), before);
    const png = PNG.sync.read(Buffer.from(paints[0].src.split(',')[1], 'base64'));
    const at = (x, y) => [...png.data.subarray((y * png.width + x) * 4, (y * png.width + x) * 4 + 4)];
    assert.equal(at(0, 0)[3], 0, 'the ancestor background must not be baked into the corner');
    const center = at(Math.floor(png.width / 2), Math.floor(png.height / 2));
    assert.ok(center[2] > center[0] + 100 && center[3] > 240, JSON.stringify(center));
    await page.evaluate(installControlPaints, paints);
    const result = await page.evaluate(takeSnapshot);
    assert.match(result.html, /data:image\/png;base64,/);
    assert.match(result.html, /opacity: 0.5/);
  });

  it('captures live native states and keeps appearance:none custom controls as CSS', async () => {
    await load(`<input type=checkbox><input type=range min=10 max=20 value=10>
      <input type=color value="#ff0000"><input type=date value="2026-10-08">
      <select><option>First</option><option>Second</option></select>
      <progress max=100 value=1></progress><meter min=10 max=20 value=15></meter>
      <input type=radio style="appearance:none;border:2px solid green;width:16px;height:16px">`);
    await page.evaluate(() => {
      document.querySelector('[type=checkbox]').indeterminate = true;
      document.querySelector('[type=range]').value = '17';
      document.querySelector('select').selectedIndex = 1;
      document.querySelector('progress').value = 75;
    });
    const paints = await captureControlPaints(page, 'puppeteer');
    assert.equal(paints.length, 7);
    await page.evaluate(installControlPaints, paints);
    const result = await page.evaluate(takeSnapshot);
    assert.ok(!result.warnings.some(w => w.includes('native paint unavailable')));
    assert.equal((result.html.match(/data:image\/png;base64,/g) || []).length, 7);
    assert.match(result.html, /rgb\(0, 128, 0\)/);
  });

  it('preserves fieldset legend cutouts without baking descendant paint', async () => {
    await load(`<fieldset style="width:300px;border:3px solid blue;padding:12px">
      <legend style="padding:0 10px">Editable legend</legend>
      <div style="height:40px;background:red">Editable contents</div></fieldset>`);
    const paints = await captureControlPaints(page, 'puppeteer');
    const png = PNG.sync.read(Buffer.from(paints[0].src.split(',')[1], 'base64'));
    for (let i = 0; i < png.data.length; i += 4) {
      assert.ok(!(png.data[i] > 200 && png.data[i + 2] < 50 && png.data[i + 3] > 200),
        'fieldset image must not contain its red child');
    }
    await page.evaluate(installControlPaints, paints);
    const result = await page.evaluate(takeSnapshot);
    assert.match(result.html, /Editable legend/);
    assert.match(result.html, /Editable contents/);
  });

  it('retains native outlines and captures partly off-canvas controls', async () => {
    await load(`<input type=checkbox checked style="position:absolute;left:-8px;top:-4px;margin:0;
      width:24px;height:24px;outline:3px solid red;outline-offset:2px">`);
    const paints = await captureControlPaints(page, 'puppeteer');
    assert.equal(paints.length, 1);
    assert.equal(paints[0].x, -5);
    assert.equal(paints[0].y, -5);
    assert.equal(paints[0].width, 34);
    const png = PNG.sync.read(Buffer.from(paints[0].src.split(',')[1], 'base64'));
    const at = (Math.floor(png.height / 2) * png.width + 1) * 4;
    assert.deepEqual([...png.data.subarray(at, at + 4)], [255, 0, 0, 255]);
  });

  it('keeps a captured fieldset surface below negative-z descendants in its context', async () => {
    await load(`<fieldset style="position:relative;isolation:isolate;width:200px;height:100px;
      background:blue;border:0;margin:0;padding:0">
      <legend>Legend</legend><div style="position:absolute;left:20px;top:30px;
      width:50px;height:50px;background:red;z-index:-1"></div></fieldset>`);
    await page.evaluate(installControlPaints, await captureControlPaints(page, 'puppeteer'));
    const result = await page.evaluate(takeSnapshot);
    await page.setContent(result.html);
    await page.evaluate(() => {
      document.documentElement.style.cssText = 'margin:0;padding:0;display:block';
    });
    const png = PNG.sync.read(Buffer.from(await page.screenshot()));
    const at = (50 * 2 * png.width + 40 * 2) * 4;
    assert.deepEqual([...png.data.subarray(at, at + 4)], [255, 0, 0, 255]);
  });

  it('restores styles and generated capture sheets when image capture fails', async () => {
    await load('<input type=checkbox style="transform:rotate(12deg)">');
    const before = await page.content();
    const screenshot = page.screenshot;
    page.screenshot = async () => { throw new Error('injected screenshot failure'); };
    try {
      await assert.rejects(captureControlPaints(page, 'puppeteer'), /injected screenshot failure/);
    } finally {
      page.screenshot = screenshot;
    }
    assert.equal(await page.content(), before);
  });

  it('materializes generated text, counters and attr() in normal flow once', async () => {
    await load(`<style>
      main{counter-reset:item 4} label{counter-increment:item;display:block}
      label::before{content:counter(item) ". ";color:red}
      label::after{content:" (" attr(data-unit) ")";color:blue}
    </style><main><label data-unit="kg">Mass <b>required</b></label></main>`);
    const before = await page.evaluate(() => {
      const r = document.querySelector('b').getBoundingClientRect();
      return { x: r.x, y: r.y, width: r.width };
    });
    const contents = await capturePseudoContents(page, 'puppeteer');
    assert.equal(contents.map(entry => entry.text).join(''), '5.  (kg)');
    await page.evaluate(materializeDecorativePseudoElements, contents);
    await page.evaluate(materializeDecorativePseudoElements, contents);
    const after = await page.evaluate(() => {
      const r = document.querySelector('b').getBoundingClientRect();
      return { x: r.x, y: r.y, width: r.width };
    });
    assert.deepEqual(after, before);
    const result = await page.evaluate(takeSnapshot);
    assert.equal((result.html.match(/5\./g) || []).length, 1);
    assert.equal((result.html.match(/\(kg\)/g) || []).length, 1);
    assert.match(result.html, /Mass/);
  });
});
