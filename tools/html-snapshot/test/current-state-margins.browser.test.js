// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const { collectCurrentState, renderCapturedPage } = require('../dist/lib/current-state');

jest.setTimeout(60000);
let source, mirror;
beforeAll(async () => { source = await launchBrowser(); mirror = await launchBrowser(); });
afterAll(async () => {
  if (source) await source.browser.close();
  if (mirror) await mirror.browser.close();
});

async function restoreAndCheck(page, captured) {
  await page.setContent(captured.html);
  // The renderer parses and serializes the collector's HTML before loading it.
  // Check another round trip so logical/physical declaration ordering cannot hide a bug.
  await page.setContent(await page.content());
  const rectangles = await page.evaluate(boxes => boxes.map(box => {
    const node = document.querySelector(`[data-pagx-capture-id="${box.id}"]`);
    if (!node) return null;
    const rect = node.getBoundingClientRect();
    return [rect.x, rect.y, rect.width, rect.height];
  }), captured.boxes);
  for (let i = 0; i < captured.boxes.length; i++) {
    expect(rectangles[i]).not.toBeNull();
    for (let axis = 0; axis < 4; axis++) {
      expect(rectangles[i][axis]).toBeCloseTo(captured.boxes[i].rect[axis], 1);
    }
  }
}

const layouts = [
  ['block center', '.item{width:200px;height:50px;margin:20px auto}'],
  ['one auto side', '.item{width:200px;height:50px;margin-left:auto;margin-right:15px}'],
  ['percentage and negative margins', '.item{width:200px;height:50px;margin:3% 5% -10px -8px}'],
  ['overflowing block', '.item{width:800px;height:50px;margin:10px auto}'],
  ['flex main axis', '.container{display:flex;height:150px;gap:13px}.item{width:90px;height:50px}.item:first-child{margin-left:auto}'],
  ['flex cross axis', '.container{display:flex;height:150px;align-items:flex-start}.item{width:90px;height:50px;margin-block:auto}'],
  ['flex column', '.container{display:flex;flex-direction:column;height:240px}.item{width:90px;height:50px}.item:first-child{margin-top:auto}'],
  ['flex wrapping', '.container{display:flex;flex-wrap:wrap;width:250px;height:180px;gap:8px}.item{width:150px;height:50px;margin-left:auto}'],
  ['grid both axes', '.container{display:grid;grid-template-columns:1fr 1fr;height:180px}.item{width:90px;height:50px;margin:auto}'],
  ['grid one side', '.container{display:grid;grid-template-columns:1fr 1fr;height:180px}.item{width:90px;height:50px;margin-inline-start:auto;align-self:start}'],
  ['RTL logical margins', '.container{direction:rtl}.item{width:200px;height:50px;margin-inline-start:auto;margin-inline-end:17px}'],
  ['vertical-rl', '.container{writing-mode:vertical-rl;width:550px;height:280px}.item{inline-size:80px;block-size:50px;margin-inline:auto}'],
  ['vertical-lr RTL', '.container{writing-mode:vertical-lr;direction:rtl;width:550px;height:280px}.item{inline-size:80px;block-size:50px;margin-inline-start:auto;margin-inline-end:13px}'],
  ['logical and physical cascade', '.container{direction:rtl}.item{width:200px;height:50px;margin-left:11px;margin-inline-end:auto;margin-right:23px}'],
  ['absolute center', '.container{position:relative;height:250px}.item{position:absolute;inset:0;width:90px;height:50px;margin:auto}'],
  ['absolute one side', '.container{position:relative;height:250px}.item{position:absolute;inset:20px;width:90px;height:50px;margin-left:auto;margin-right:13px}'],
  ['transforms', '.container{transform:translate(13px,7px) scale(.85)}.item{width:200px;height:50px;margin:auto;transform:rotate(5deg)}'],
  ['variables and inheritance', '.container{--m:auto;margin-left:var(--m);margin-right:var(--m);width:500px}.item{width:200px;height:50px;margin-inline:inherit}'],
  ['scroll container', '.container{height:120px;overflow:auto}.item{width:200px;height:150px;margin:10px auto}'],
];

test.each(layouts)('retains margin geometry through serialization: %s', async (_name, css) => {
  const page = await newPage(source, { viewport: { width:640, height:480 } });
  const preview = await newPage(mirror, { viewport: { width:640, height:480 } });
  try {
    await page.setContent(`<!doctype html><style>*{box-sizing:border-box}body{margin:0}
      .container{width:600px}${css}</style><div class="container">
      <div class="item">one</div><div class="item">two</div></div>`);
    const before = await page.content();
    const captured = await page.evaluate(collectCurrentState);
    expect(await page.content()).toBe(before);
    await restoreAndCheck(preview, captured);
  } finally { await page.close(); await preview.close(); }
});

test('keeps a centered box when CSSOM reports zero for its auto margins', async () => {
  const page = await newPage(source, { viewport: { width:1400, height:900 } });
  const preview = await newPage(mirror, { viewport: { width:1400, height:900 } });
  try {
    await page.setContent(`<!doctype html><style>body{margin:0}
      #center{width:90%;height:240px;margin:20px auto;display:flex}
      #zero{width:30px;height:30px;margin:0}</style>
      <div id="center"><span>Centered content</span></div><div id="zero"></div>`);
    // Reproduce the observed Chromium CSSOM result deterministically, including
    // on engines that already report the used margins correctly.
    await page.evaluate(() => {
      const native = window.getComputedStyle;
      window.getComputedStyle = (element, pseudo) => {
        const style = native(element, pseudo);
        if (element.id !== 'center' || pseudo) return style;
        return new Proxy(style, {
          get(target, key) {
            if (key === 'getPropertyValue') return property =>
              /^margin-(left|right|inline-start|inline-end)$/.test(property)
                ? '0px' : target.getPropertyValue(property);
            const value = Reflect.get(target, key, target);
            return typeof value === 'function' ? value.bind(target) : value;
          },
        });
      };
    });
    const captured = await page.evaluate(collectCurrentState);
    expect(captured.boxes.find(box => box.tag === 'div#center').rect[0]).toBe(70);
    await restoreAndCheck(preview, captured);
    expect(await preview.$eval('#center', el => [el.style.marginLeft, el.style.marginRight]))
      .toEqual(['auto', 'auto']);
    expect(await preview.$eval('#zero', el => el.style.marginLeft)).toBe('0px');
    const result = await renderCapturedPage(mirror, captured);
    expect(result.warnings).toEqual([]);
    expect(result.html).toContain('Centered content');
  } finally { await page.close(); await preview.close(); }
});

test.each(['missing', 'map throws', 'property throws'])('falls back to resolved margins when Typed OM is %s', async mode => {
  const page = await newPage(source, { viewport: { width:640, height:480 } });
  const preview = await newPage(mirror, { viewport: { width:640, height:480 } });
  try {
    await page.setContent(`<!doctype html><style>body{margin:0}
      #center{width:200px;height:50px;margin:10px auto}</style><div id="center">Fallback</div>`);
    await page.evaluate(mode => {
      const element = document.querySelector('#center');
      const native = element.computedStyleMap.bind(element);
      if (mode === 'missing') element.computedStyleMap = undefined;
      else if (mode === 'map throws') element.computedStyleMap = () => { throw new Error('Unavailable'); };
      else element.computedStyleMap = () => ({ get(property) {
        if (property === 'margin-left') throw new Error('Unsupported property');
        return native().get(property);
      } });
    }, mode);
    const captured = await page.evaluate(collectCurrentState);
    await restoreAndCheck(preview, captured);
    expect(await preview.$eval('#center', el => el.style.marginLeft)).toBe('220px');
  } finally { await page.close(); await preview.close(); }
});

test('copies pseudo margins independently from their auto-margin host', async () => {
  const page = await newPage(source, { viewport: { width:640, height:480 } });
  const preview = await newPage(mirror, { viewport: { width:640, height:480 } });
  const readPseudos = () => ['::before', '::after'].map(pseudo => {
    const style = getComputedStyle(document.querySelector('#host'), pseudo);
    return [style.marginTop, style.marginRight, style.marginBottom, style.marginLeft];
  });
  try {
    await page.setContent(`<!doctype html><style>body{margin:0}
      #host{width:200px;height:80px;margin:10px auto}
      #host::before,#host::after{content:"";display:block;width:20px;height:10px;background:red}
      #host::before{margin:3px 19px 5px 7px}#host::after{margin-inline:auto}
      </style><div id="host"></div>`);
    const before = await page.evaluate(readPseudos);
    const captured = await page.evaluate(collectCurrentState);
    await restoreAndCheck(preview, captured);
    expect(await preview.evaluate(readPseudos)).toEqual(before);
    expect(before).toEqual([['3px', '19px', '5px', '7px'], ['0px', '90px', '0px', '90px']]);
  } finally { await page.close(); await preview.close(); }
});

test('the standalone browser bundle preserves automatic margins', async () => {
  execFileSync(process.execPath, [path.resolve(__dirname, '../build-browser-bundle.js')], { timeout:30000 });
  const bundle = fs.readFileSync(path.resolve(__dirname, '../dist/html-snapshot.umd.js'), 'utf8');
  const page = await newPage(source, { viewport: { width:640, height:480 } });
  const preview = await newPage(mirror, { viewport: { width:640, height:480 } });
  try {
    await page.setContent(`<!doctype html><style>body{margin:0}#center{width:200px;height:50px;margin:auto}</style>
      <div id="center">Browser bundle</div>`);
    await page.evaluate(bundle);
    const captured = await page.evaluate(() => window.HtmlSnapshot.collectCurrentState());
    await restoreAndCheck(preview, captured);
    expect(await preview.$eval('#center', el => el.style.marginLeft)).toBe('auto');
  } finally { await page.close(); await preview.close(); }
});
