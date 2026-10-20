'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { describe, it, before, after } = require('node:test');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { takeSnapshot, inlineExternalImages } = require('../../dist/lib/browser-snapshot');

const binary = process.env.PAGX_HTML_TEST_BIN ||
  path.resolve(__dirname, '../../../../cmake-build-debug/pagx');
const checker = 'repeating-conic-gradient(#e5e5e5 0 25%,white 0 50%)';
const png = new PNG({ width: 120, height: 80 });
for (let y = 0; y < 80; y++) for (let x = 0; x < 120; x++) {
  const at = (y * 120 + x) * 4;
  png.data.set(x < 60 ? [255, 0, 0, 255] : [0, 0, 255, 100], at);
}
const image = `data:image/png;base64,${PNG.sync.write(png).toString('base64')}`;

describe('CSS background layout agrees with Chromium', { skip: !fs.existsSync(binary) }, () => {
  let browser, page;
  before(async () => {
    ({ browser } = await launchBrowser());
    page = await browser.newPage();
  });
  after(async () => { if (browser) await browser.close(); });

  async function compare(name, style, {
    scale = 1, tag = 'div', contents = '', mean = 2, direct = true,
  } = {}) {
    await page.setViewport({ width: 240, height: 200, deviceScaleFactor: scale });
    const html = `<html><body style="margin:0;width:240px;height:200px;background:#00ff00">
      <${tag} ${tag === 'img' ? `src="${image}"` : ''}
      style="box-sizing:border-box;position:absolute;left:10px;top:10px;width:220px;height:180px;${style.replaceAll('"', '&quot;')}"${tag === 'img' ? '/' : ''}>
      ${contents}${tag === 'img' ? '' : `</${tag}>`}</body></html>`;
    await page.setContent(html);
    await page.evaluate(async () => { await Promise.all([...document.images].map(i => i.decode())); });
    const reference = PNG.sync.read(Buffer.from(await page.screenshot()));
    await page.evaluate(inlineExternalImages, {});
    const snapshot = await page.evaluate(takeSnapshot);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), `pagx-background-${name}-`));
    fs.writeFileSync(path.join(dir, 'reference.png'), PNG.sync.write(reference));
    const sources = direct ? [['direct', html], ['snapshot', snapshot.html]] : [['snapshot', snapshot.html]];
    for (const [kind, source] of sources) {
      const input = path.join(dir, `${kind}.html`), output = path.join(dir, `${kind}.pagx`);
      fs.writeFileSync(input, source);
      for (const args of [
        ['import', '--input', input, '--output', output, '--format', 'html'],
        ['render', output, '-o', path.join(dir, `${kind}.png`), '--scale', String(scale)],
      ]) {
        const result = spawnSync(binary, args, {
          env: { ...process.env, PAGX_HTML_SNAPSHOT: '0' }, encoding: 'utf8',
        });
        assert.equal(result.status, 0, `${name}/${kind}: ${result.stderr}; ${dir}`);
      }
      const actual = PNG.sync.read(fs.readFileSync(path.join(dir, `${kind}.png`)));
      assert.equal(actual.width, reference.width);
      assert.equal(actual.height, reference.height);
      let sum = 0, flatError = 0;
      for (let y = 0; y < actual.height; y++) for (let x = 0; x < actual.width; x++) {
        const at = (y * actual.width + x) * 4;
        for (let c = 0; c < 4; c++) sum += Math.abs(actual.data[at + c] - reference.data[at + c]);
        if (x < 2 || y < 2 || x >= actual.width - 2 || y >= actual.height - 2) continue;
        let flat = true;
        for (let dy = -2; dy <= 2 && flat; dy++) for (let dx = -2; dx <= 2 && flat; dx++) {
          const near = ((y + dy) * actual.width + x + dx) * 4;
          for (let c = 0; c < 4; c++) if (reference.data[at + c] !== reference.data[near + c]) flat = false;
        }
        if (flat) for (let c = 0; c < 4; c++) {
          flatError = Math.max(flatError, Math.abs(actual.data[at + c] - reference.data[at + c]));
        }
      }
      assert.ok(flatError <= 4, `${name}/${kind}: flat error ${flatError}; ${dir}`);
      assert.ok(sum / actual.data.length < mean,
        `${name}/${kind}: mean ${sum / actual.data.length}; ${dir}`);
    }
    fs.rmSync(dir, { recursive: true });
  }

  it('tiles the gallery checker behind a contained image', async () => {
    await compare('gallery', `object-fit:contain;border-radius:6px;background:${checker} 0 0/20px 20px`,
      { tag: 'img' });
  });
  it('rotates a full turn in pixel space around an off-center point', async () => {
    await compare('conic', 'background:conic-gradient(from 37deg at 30% 70%,red,blue) 7px -3px/30px 20px');
  });
  it('preserves negative repeating phase and angular units', async () => {
    await compare('phase', 'background:repeating-conic-gradient(from .1turn,red -.25turn -.1turn,blue -.1turn .25turn) 0 0/40px 40px',
      { mean: 3 });
  });
  it('interpolates stops outside the visible turn before clipping', async () => {
    await compare('outside', 'background:conic-gradient(red -180deg,blue 540deg) 0 0/50px 30px');
  });
  it('resolves gradient auto axes without an intrinsic aspect ratio', async () => {
    await compare('auto', 'background:linear-gradient(135deg,red,blue) right bottom/40px auto no-repeat');
    await compare('auto-y', 'background:radial-gradient(circle at 30% 70%,red,blue) left top/auto 40px no-repeat');
  });
  it('supports independently spaced and rounded axes', async () => {
    await compare('space-round', `background:${checker} right bottom/37px 31px space round`, { mean: 3 });
    await compare('space-one', `background:${checker} right 7px bottom 9px/130px 120px space no-repeat`);
  });
  it('cycles lists without dropping none and preserves mixed image order', async () => {
    await compare('layers', `background-color:white;background-image:linear-gradient(90deg,rgba(0,255,0,.5),transparent),none,url("${image}");
      background-size:100% 100%,10px 10px,24px 16px;background-repeat:no-repeat,repeat;
      background-position:0 0,5px 6px,3px 4px`);
    await compare('two-urls', `background-image:none,url("${image}"),url("${image}");
      background-size:auto,40px 30px,80px 60px;background-repeat:no-repeat;
      background-position:0 0,left top,right bottom`);
  });
  it('distinguishes origin, clip and the color layer under translucent borders', async () => {
    await compare('boxes', `border:8px solid rgba(255,0,0,.3);padding:12px;border-radius:20px;
      background:${checker} 50% 50%/32px 24px content-box border-box,linear-gradient(blue,blue) padding-box content-box;
      background-color:white`);
  });
  it('preserves image fitting, position and repeat for URL images', async () => {
    await compare('url', `background:white url("${image}") right bottom/contain repeat-x`);
  });
  for (const scale of [1, 1.5, 2]) {
    it(`keeps fractional shared edges opaque at scale ${scale}`, async () => {
      await compare(`fractional-${scale}`, `border-radius:13px;background:${checker} .3px .3px/20.5px 19.5px`,
        { scale, mean: 3 });
    });
  }
  it('retains real no-repeat edges and transparency without overlapping tiles', async () => {
    await compare('transparent', 'background:linear-gradient(90deg,rgba(255,0,0,.5) 0 50%,transparent 50%) .3px .3px/20.5px 19.5px repeat no-repeat',
      { mean: 2 });
  });
  it('keeps SVG background tiles vector through import and resolve', async () => {
    const svg = Buffer.from('<svg xmlns="http://www.w3.org/2000/svg" width="20" height="16">' +
      '<rect width="10" height="16" fill="red"/><circle cx="15" cy="8" r="4" fill="blue"/></svg>').toString('base64');
    await compare('svg', `background: url(data:image/svg+xml;base64,${svg}) 3px 2px/20px 16px repeat`);
  });
  it('isolates per-layer background blending from the page backdrop', async () => {
    await compare('blend', 'background-color:rgba(255,255,255,.5);' +
      'background-image:linear-gradient(rgba(255,0,0,.5),rgba(255,0,0,.5)),linear-gradient(blue,blue);' +
      'background-blend-mode:multiply,screen;background-size:60% 60%,80% 80%;background-repeat:no-repeat');
  });
  it('uses the same tile layout for a repeated conic alpha mask', async () => {
    await compare('mask', 'background:red;mask-image:repeating-conic-gradient(white 0 25%,transparent 0 50%);' +
      'mask-size:20px 20px;mask-position:3px 4px;mask-repeat:repeat');
  });
  it('tiles linear and radial repeating gradients independently from their internal periods', async () => {
    await compare('repeat-linear', 'background:repeating-linear-gradient(90deg,red -5px,blue 5px) 3px 2px/40px 20px');
    await compare('repeat-fixup', 'background:repeating-linear-gradient(90deg,red 0%,green 40%,blue 20%,white,black 100%) 3px 2px/40px 20px');
    await compare('repeat-radial', 'background:repeating-radial-gradient(circle,red 0%,blue 50%) 3px 2px/40px 20px',
      { mean: 3 });
  });
  it('sizes multiline highlighter backgrounds once per measured fragment', async () => {
    await compare('highlighter', 'font:16px Arial;line-height:24px;color:transparent', {
      direct: false,
      contents: '<span style="color:transparent;background:linear-gradient(#ffcc00,#ffcc00) ' +
        '0 90%/100% 8px no-repeat;box-decoration-break:clone;-webkit-box-decoration-break:clone">' +
        'Measured inline fragments keep their full border boxes before the background is sized.</span>',
    });
  });
  it('inlines every cached URL without concurrent writes erasing sibling layers', async () => {
    await page.setRequestInterception(true);
    const respond = request => request.respond({
      status: 200, contentType: 'image/png', body: PNG.sync.write(png),
    });
    page.on('request', respond);
    try {
      await page.setContent('<div id="layers" style="background-image:none,' +
        'url(https://background.test/a.png),linear-gradient(red,blue),' +
        'url(https://background.test/b.png);width:100px;height:100px"></div>');
      await page.evaluate(inlineExternalImages, {
        'https://background.test/a.png': image,
        'https://background.test/b.png': image,
      });
      const actual = await page.$eval('#layers', el => el.style.backgroundImage);
      assert.equal((actual.match(/data:image\/png/g) || []).length, 2);
      assert.match(actual, /^none,/);
      assert.match(actual, /linear-gradient/);
      assert.doesNotMatch(actual, /background\.test/);
    } finally {
      page.off('request', respond);
      await page.setRequestInterception(false);
    }
  });
  // Zero-length repeating periods use the CSS Images average-color rule, asserted in the
  // parser tests. Chromium currently paints the last conic stop instead, so it is not an
  // independent reference for that degenerate case.
});
