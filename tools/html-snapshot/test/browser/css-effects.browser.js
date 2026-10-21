'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { describe, it, before, after } = require('node:test');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { takeSnapshot } = require('../../dist/lib/browser-snapshot');

const binary = process.env.PAGX_HTML_TEST_BIN ||
  path.resolve(__dirname, '../../../../cmake-build-debug/pagx');
const stripes = new PNG({ width: 120, height: 90 });
for (let y = 0; y < stripes.height; y++) for (let x = 0; x < stripes.width; x++) {
  stripes.data.set(x >= 48 && x < 72 ? [0, 0, 255, 255] : [255, 0, 0, 255],
    (y * stripes.width + x) * 4);
}
const stripeImage = `data:image/png;base64,${PNG.sync.write(stripes).toString('base64')}`;

describe('CSS effects retain their painting semantics', { skip: !fs.existsSync(binary) }, () => {
  let browser, page;
  before(async () => {
    ({ browser } = await launchBrowser());
    page = await browser.newPage();
    await page.setViewport({ width: 240, height: 180, deviceScaleFactor: 1 });
  });
  after(async () => { if (browser) await browser.close(); });

  async function compare(name, style, contents = '', { mean = 1.5, flat = true } = {}) {
    await page.setContent(`<style>html,body{margin:0;padding:0;width:240px;height:180px;
      background:#ececec}*{box-sizing:border-box}</style>
      <div style="position:absolute;left:40px;top:35px;width:160px;height:110px;${style}">${contents}</div>`);
    await page.evaluate(() => document.fonts.ready);
    const reference = PNG.sync.read(Buffer.from(await page.screenshot()));
    const snapshot = await page.evaluate(takeSnapshot);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), `pagx-effects-${name}-`));
    const input = path.join(dir, 'source.html'), output = path.join(dir, 'output.pagx');
    fs.writeFileSync(input, snapshot.html);
    fs.writeFileSync(path.join(dir, 'reference.png'), PNG.sync.write(reference));
    for (const args of [
      ['import', '--input', input, '--output', output],
      ['render', output, '--output', path.join(dir, 'actual.png')],
    ]) {
      const result = spawnSync(binary, args, {
        env: { ...process.env, PAGX_HTML_SNAPSHOT: '0' }, encoding: 'utf8', timeout: 30000,
      });
      assert.equal(result.status, 0, `${result.stderr}; ${dir}`);
    }
    const actual = PNG.sync.read(fs.readFileSync(path.join(dir, 'actual.png')));
    assert.equal(actual.width, reference.width);
    assert.equal(actual.height, reference.height);
    let sum = 0, flatError = 0;
    for (let y = 0; y < actual.height; y++) for (let x = 0; x < actual.width; x++) {
      const at = (y * actual.width + x) * 4;
      for (let c = 0; c < 4; c++) sum += Math.abs(actual.data[at + c] - reference.data[at + c]);
      if (!flat || x < 3 || y < 3 || x >= actual.width - 3 || y >= actual.height - 3) continue;
      let uniform = true;
      for (let dy = -3; dy <= 3 && uniform; dy++) for (let dx = -3; dx <= 3 && uniform; dx++) {
        const near = ((y + dy) * actual.width + x + dx) * 4;
        for (let c = 0; c < 4; c++) if (reference.data[at + c] !== reference.data[near + c]) uniform = false;
      }
      if (uniform) for (let c = 0; c < 4; c++) {
        flatError = Math.max(flatError, Math.abs(actual.data[at + c] - reference.data[at + c]));
      }
    }
    assert.ok(flatError <= 4, `${name}: flat error ${flatError}; ${dir}`);
    assert.ok(sum / actual.data.length < mean, `${name}: mean ${sum / actual.data.length}; ${dir}`);
    fs.rmSync(dir, { recursive: true });
  }

  it('resolves both radius axes, overlap and asymmetric border widths', async () => {
    await compare('radii', 'background:yellow;border-radius:80% 35% 20% 10% / 90% 30% 20% 10%;' +
      'border-top:5px solid red;border-right:9px solid blue;border-bottom:7px solid green;border-left:3px solid black');
  });
  it('keeps double-border gaps and an outline outside overflow clipping', async () => {
    await compare('outline', 'background:yellow;border:9px double blue;outline:3px solid red;' +
      'outline-offset:5px;overflow:hidden;border-radius:20px / 12px');
  });
  it('slices a gradient border in source coordinates and preserves its center fill', async () => {
    await compare('border-image', 'border:10px solid transparent;border-radius:35px;' +
      'border-image-source:linear-gradient(90deg,red,blue);border-image-slice:20% fill;' +
      'border-image-width:1 2;border-image-outset:4px');
  });
  it('spaces and centers repeated border-image tiles', async () => {
    for (const mode of ['space', 'repeat', 'round']) {
      await compare(`border-${mode}`, 'border:12px solid transparent;' +
        `border-image-source:url('${stripeImage}');` +
        `border-image-slice:30%;border-image-width:1;border-image-repeat:${mode}`);
    }
  });
  it('paints independent text shadows even when the foreground is transparent', async () => {
    await compare('text-shadow', 'font:50px Arial;line-height:80px;color:transparent;' +
      'text-shadow:3px 2px 0 red,8px 4px 4px blue', 'TEST', { mean: 2, flat: false });
  });
  it('retains the background owner coordinates for clipped text', async () => {
    await compare('text-clip', 'font:bold 55px Arial;line-height:80px;color:transparent;' +
      'background:linear-gradient(90deg,red,blue);background-clip:text', 'HI', { mean: 2, flat: false });
  });
  it('filters a transparent empty box without filtering its foreground', async () => {
    await compare('backdrop', 'border-radius:20px;backdrop-filter:brightness(.5) invert(1);' +
      'transform:rotate(8deg)', '<div style="position:absolute;left:50px;top:30px;width:30px;height:30px;background:blue"></div>');
    await compare('empty-backdrop', 'border-radius:20px;backdrop-filter:brightness(.5) invert(1)');
  });
});
