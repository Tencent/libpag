'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { describe, it, before, after } = require('node:test');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { takeSnapshot, materializeDecorativePseudoElements } = require('../../dist/lib/browser-snapshot');

describe('CSS painting survives the snapshot', () => {
  let browser;
  let page;
  before(async () => {
    ({ browser } = await launchBrowser({ engine: 'puppeteer' }));
    page = await browser.newPage();
    await page.setViewport({ width: 240, height: 180, deviceScaleFactor: 1 });
  });
  after(async () => { if (browser) await browser.close(); });

  function compareNative(name, html, reference, nativeTime) {
    const binary = process.env.PAGX_PAINT_TEST_BIN;
    if (!binary) return;
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-native-paint-'));
    const input = path.join(dir, 'subset.html');
    const pagx = path.join(dir, 'subset.pagx');
    const png = path.join(dir, 'subset.png');
    fs.writeFileSync(input, html);
    for (const args of [
      ['import', '--input', input, '--output', pagx, '--format', 'html', '--html-no-snapshot'],
      ['render', pagx, '--output', png, ...(nativeTime === undefined ? [] : ['--time', String(nativeTime)])],
    ]) {
      const result = spawnSync(binary, args, {
        encoding: 'utf8', timeout: 30000,
      });
      assert.equal(result.status, 0, `${name}: ${result.stderr}; ${dir}`);
    }
    const actual = PNG.sync.read(fs.readFileSync(png));
    // Compare solid interiors so backend-specific edge antialiasing is irrelevant.
    // These fixtures deliberately use flat colours; a wrong paint/clip/group changes interiors.
    for (let y = 3; y < reference.height - 3; y += 6) {
      for (let x = 3; x < reference.width - 3; x += 6) {
        const at = (y * reference.width + x) * 4;
        let flat = true;
        for (let dy = -2; dy <= 2; dy++) {
          for (let dx = -2; dx <= 2; dx++) {
            const near = ((y + dy) * reference.width + x + dx) * 4;
            for (let c = 0; c < 4; c++) {
              if (reference.data[near + c] !== reference.data[at + c]) flat = false;
            }
          }
        }
        if (!flat) continue;
        const nativeAt = (y * actual.width + x) * 4;
        for (let c = 0; c < 4; c++) {
          assert.ok(Math.abs(reference.data[at + c] - actual.data[nativeAt + c]) <= 3,
            `${name}: native pixel (${x},${y}) channel ${c}: ` +
            `${reference.data[at + c]} != ${actual.data[nativeAt + c]}; ${dir}`);
        }
      }
    }
    fs.rmSync(dir, { recursive: true });
  }

  async function compare(name, css, content, check, nativeTime) {
    const original = `<style>
      * { box-sizing:border-box } html,body { margin:0;padding:0 }
      body { width:240px;height:180px;background:white }
      .red { background:#e02020 } .green { background:#20b040 } .blue { background:#2040d0 }
      ${css}</style>${content}`;
    await page.setContent(original);
    const before = Buffer.from(await page.screenshot());
    await page.evaluate(materializeDecorativePseudoElements);
    const snapshot = await page.evaluate(takeSnapshot);
    assert.doesNotMatch(snapshot.html, /data-pagx-paint-source|data-pagx-paint-decoration/);
    if (check) check(snapshot);
    await page.setContent(snapshot.html);
    await page.evaluate(() => {
      Object.assign(document.documentElement.style, { margin: '0', padding: '0', display: 'block' });
    });
    const after = Buffer.from(await page.screenshot());
    const a = PNG.sync.read(before);
    const b = PNG.sync.read(after);
    let changed = 0;
    let maxDelta = 0;
    for (let i = 0; i < a.data.length; i += 4) {
      let delta = 0;
      for (let c = 0; c < 4; c++) delta = Math.max(delta, Math.abs(a.data[i + c] - b.data[i + c]));
      if (delta > 2) changed++;
      maxDelta = Math.max(maxDelta, delta);
    }
    if (changed) {
      const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-paint-'));
      fs.writeFileSync(path.join(dir, 'source.html'), original);
      fs.writeFileSync(path.join(dir, 'subset.html'), snapshot.html);
      fs.writeFileSync(path.join(dir, 'source.png'), before);
      fs.writeFileSync(path.join(dir, 'subset.png'), after);
      assert.fail(`${name}: ${changed} changed pixels, max delta ${maxDelta}; ${dir}`);
    }
    compareNative(name, snapshot.html, a, nativeTime);
  }

  it('compares positive descendants across non-context parents', async () => {
    await compare('cross-parent', `
      .row { display:flex;height:110px }
      .row>div { position:relative;z-index:3;width:150px;height:110px }
      .pull { position:relative;margin:-40px 20px 0;width:190px;height:80px }
    `, '<div class="row"><div class="red"></div></div><div class="pull blue"></div>');
  });

  it('paints negative descendants below a non-context parent background', async () => {
    await compare('negative-background', `
      .host { position:relative;margin:10px;width:160px;height:120px;background:#fff }
      .child { position:absolute;left:10px;top:10px;width:180px;height:140px;z-index:-1 }
    `, '<div class="host"><div class="child red"></div></div>');
  });

  for (const trigger of [
    '', 'z-index:0', 'isolation:isolate', 'opacity:.6', 'transform:translate(0px)',
    'will-change:transform', 'contain:layout', 'position:sticky;top:0', 'filter:blur(0px)',
  ]) {
    it(`respects context boundaries: ${trigger || 'relative + auto'}`, async () => {
      await compare(`boundary-${trigger}`, `
        .host { position:relative;width:160px;height:120px;${trigger} }
        .child { position:absolute;left:20px;top:20px;width:160px;height:120px;z-index:99 }
        .cover { position:absolute;left:50px;top:40px;width:150px;height:100px;z-index:2 }
      `, '<div class="host"><div class="child red"></div></div><div class="cover blue"></div>');
    });
  }

  it('keeps group opacity when children overlap', async () => {
    await compare('group-opacity', `
      .host { opacity:.5;width:220px;height:160px }
      .host>div { position:absolute;width:120px;height:100px }
      .first { left:10px;top:10px;z-index:3 }
      .second { left:80px;top:50px;z-index:2 }
    `, '<div class="host"><div class="first red"></div><div class="second blue"></div></div>');
  });

  for (const display of ['block', 'flex', 'grid']) {
    it(`uses active z-index for ${display} items`, async () => {
      await compare(`items-${display}`, `
        .host { display:${display};width:220px;height:150px;grid-template-columns:100px 100px }
        .host>div { width:120px;height:100px;flex-shrink:0 }
        .a { z-index:5 }
        .b { margin-left:-50px;margin-top:-40px }
      `, '<div class="host"><div class="a red"></div><div class="b blue"></div></div>');
    });
  }

  it('uses order-modified order to break flex z-index ties', async () => {
    await compare('flex-order', `
      .host { display:flex;width:220px;height:150px }
      .host>div { width:120px;height:100px;flex-shrink:0;z-index:0 }
      .a { order:2;margin-left:-50px }
      .b { order:1 }
    `, '<div class="host"><div class="a red"></div><div class="b blue"></div></div>');
  });

  for (const style of ['float:left', 'display:inline-block']) {
    it(`lets positioned descendants escape a ${style} paint group`, async () => {
      await compare(style, `
        .host { ${style};width:100px;height:80px }
        .child { position:absolute;width:160px;height:120px;z-index:3;left:0;top:0 }
        .cover { position:relative;width:180px;height:130px }
      `, '<div class="host"><div class="child red"></div></div><div class="cover blue"></div>');
    });
  }

  it('retains a rounded overflow clip while promoting positioned descendants', async () => {
    await compare('rounded-clip', `
      .host { position:relative;overflow:hidden;border:5px solid #111;border-radius:20px;
        width:140px;height:120px }
      .child { position:absolute;left:30px;top:30px;width:160px;height:120px;z-index:3 }
      .cover { position:absolute;left:0;top:40px;width:210px;height:90px;z-index:1 }
    `, '<div class="host"><div class="child red"></div></div><div class="cover blue"></div>');
  });

  it('does not clip absolute children whose containing block is above an overflow ancestor', async () => {
    await compare('overflow-escape', `
      .host { overflow:hidden;width:80px;height:80px;margin-left:10px;background:#ddd }
      .child { position:absolute;left:30px;top:30px;width:150px;height:120px;z-index:3 }
      .cover { position:absolute;left:0;top:50px;width:200px;height:80px;z-index:1 }
    `, '<div class="host"><div class="child red"></div></div><div class="cover blue"></div>');
  });

  it('does not create a containing block for display:contents', async () => {
    await compare('contents-containing-block', `
      .host { overflow:hidden;width:80px;height:80px;background:#ddd }
      .contents { display:contents;position:relative;overflow:hidden }
      .child { position:absolute;left:30px;top:30px;width:150px;height:120px;z-index:3 }
    `, '<div class="host"><div class="contents"><div class="child red"></div></div></div>');
  });

  for (const style of ['opacity:.5', 'isolation:isolate', 'z-index:0;display:flex']) {
    it(`preserves selective overflow inside a ${style} context`, async () => {
      await compare(`context-overflow-${style}`, `
        .outer { display:flex }
        .host { ${style};overflow:hidden;width:80px;height:80px;background:#ddd }
        .flow { width:130px;height:110px;flex-shrink:0 }
        .child { position:absolute;left:30px;top:30px;width:150px;height:120px;z-index:3 }
      `, '<div class="outer"><div class="host"><div class="flow blue"></div><div class="child red"></div></div></div>');
    });

    it(`preserves selective ancestor overflow around a ${style} context`, async () => {
      await compare(`ancestor-overflow-${style}`, `
        .outer { overflow:hidden;display:flex;width:90px;height:90px;border:3px solid black }
        .host { ${style};width:80px;height:80px;background:#ddd }
        .flow { width:130px;height:110px;flex-shrink:0 }
        .child { position:absolute;left:30px;top:30px;width:150px;height:120px;z-index:3 }
      `, '<div class="outer"><div class="host"><div class="flow blue"></div><div class="child red"></div></div></div>');
    });
  }

  it('keeps nested transforms and their coordinate origins', async () => {
    await compare('transforms', `
      .host { position:relative;transform:translate(20px,10px) rotate(12deg);
        width:160px;height:130px;border:3px solid #111;overflow:hidden }
      .child { transform:rotate(-20deg);position:absolute;left:20px;top:20px;
        width:130px;height:90px;z-index:3 }
      .cover { position:absolute;left:10px;top:50px;width:170px;height:60px;z-index:1 }
    `, '<div class="host"><div class="child red"></div><div class="cover blue"></div></div>');
  });

  it('keeps a transform-established containing block during measurement', async () => {
    await compare('transform-containing-block', `
      .host { transform:translate(15px,10px);margin-left:20px;margin-top:20px;
        width:150px;height:100px }
      .child { position:absolute;left:20px;top:20px;width:100px;height:80px;z-index:3 }
    `, '<div class="host"><div class="child red"></div></div>');
  });

  it('keeps a captured opacity animation as one compositing group', async () => {
    await compare('animated-opacity', `
      .host { animation:pagxAnim0 100000s linear -50000s both;width:220px;height:160px }
      .host>div { position:absolute;width:120px;height:100px }
      .first { left:10px;top:10px;z-index:3 }
      .second { left:80px;top:50px;z-index:2 }
    `, '<style id="__pagx_anim_keyframes">@keyframes pagxAnim0{from{opacity:0}to{opacity:1}}</style>' +
      '<div class="host"><div class="first red"></div><div class="second blue"></div></div>', undefined, 50000);
  });

  it('preserves an ordinary flex subtree', async () => {
    await compare('ordinary-flex', `
      .host { display:flex;gap:12px;padding:8px;width:220px;height:120px }
      .host>div { width:80px;height:80px }
    `, '<div class="host"><div class="red"></div><div class="blue"></div></div>',
    snapshot => assert.match(snapshot.html, /display: flex/));
  });

  it('paints a block flex background before preceding inline content', async () => {
    await compare('flex-background-phase', `
      .first { height:80px;font-size:0;line-height:0 }
      .first>i { display:inline-block;width:130px;height:100px }
      .second { display:flex;margin-top:-60px;width:200px;height:100px }
      .second>div { width:30px;height:30px;margin-left:150px }
    `, '<div class="first"><i class="red"></i></div><div class="second blue"><div class="green"></div></div>');
  });

  it('uses order-modified order through display:contents', async () => {
    await compare('flex-contents-order', `
      .host { display:flex;width:220px;height:150px }
      .contents { display:contents }
      .item { width:120px;height:100px;flex-shrink:0;z-index:0 }
      .a { order:2;margin-left:-50px }
      .b { order:1 }
    `, '<div class="host"><div class="contents"><div class="a item red"></div></div><div class="b item blue"></div></div>');
  });

  it('keeps paint containment clipping together with its context', async () => {
    await compare('paint-containment', `
      .host { contain:paint;width:130px;height:100px;border:5px solid black;border-radius:15px }
      .child { position:absolute;left:30px;top:30px;width:150px;height:130px;z-index:10 }
    `, '<div class="host"><div class="child red"></div></div>');
  });

  it('does not turn a non-transformable inline into a stacking context', async () => {
    await compare('inline-transform', `
      .host { transform:translate(0px) }
      .child { position:absolute;left:10px;top:10px;width:150px;height:120px;z-index:10 }
      .cover { position:absolute;left:30px;top:50px;width:180px;height:100px;z-index:1 }
    `, '<span class="host"><span class="child red"></span></span><div class="cover blue"></div>');
  });

  it('serializes replaced content as XML-compatible HTML', async () => {
    const src = 'data:image/svg+xml,' + encodeURIComponent(
      '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="80"><path fill="#e02020" d="M0 0h100v80H0z"/></svg>');
    await compare('image', 'img { display:block;width:100px;height:80px }',
      `<img src="${src}"/>`, snapshot => assert.match(snapshot.html, /<img\b[^>]*\/>/));
  });

  it('keeps a decorated SVG in its measured viewport', async () => {
    await compare('svg-viewport', `
      .host { width:200px;height:150px;display:flex;align-items:center;justify-content:center }
      svg { width:120px;height:80px;border-radius:12px;overflow:hidden }
    `, `<div class="host"><svg width="100%" viewBox="0 0 300 200">
      <rect width="300" height="200" fill="#e02020"/>
      <rect x="150" width="150" height="100" fill="#2040d0"/>
    </svg></div>`, snapshot => {
      assert.match(snapshot.html, /<svg[^>]*width="120"[^>]*height="80"/);
    });
  });

  it('retains rounded clipping on generated image content', async () => {
    const src = 'data:image/svg+xml,' + encodeURIComponent(
      '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><path fill="#e02020" d="M0 0h100v100H0z"/></svg>');
    await compare('rounded-image', `
      .host { display:flex }
      img { width:80px;height:80px;border:4px solid black;border-radius:50%;object-fit:cover }
      img+img { margin-left:-20px }
    `, `<div class="host"><img src="${src}"/><img src="${src}"/></div>`);
  });

  it('preserves pseudo elements across ancestor boundaries', async () => {
    await compare('pseudo', `
      .host { width:150px;height:110px }
      .host::before { content:"";position:absolute;left:10px;top:10px;
        width:150px;height:110px;background:#e02020;z-index:4 }
      .cover { position:absolute;left:30px;top:50px;width:180px;height:100px }
    `, '<div class="host"></div><div class="cover blue"></div>');
  });

  it('isolates descendant blending inside an explicit zero context', async () => {
    await compare('blend-isolation', `
      body { background:#20b040 }
      .host { position:relative;z-index:0;width:200px;height:140px }
      .child { position:absolute;left:20px;top:20px;width:150px;height:100px;
        mix-blend-mode:multiply }
    `, '<div class="host"><div class="child red"></div></div>');
  });
});
