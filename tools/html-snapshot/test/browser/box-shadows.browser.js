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

const binary = process.env.PAGX_HTML_TEST_BIN || path.resolve(__dirname, '../../../../cmake-build-debug/pagx');

describe('CSS spread shadows agree with Chromium', { skip: !fs.existsSync(binary) }, () => {
  let browser, page;
  before(async () => {
    ({ browser } = await launchBrowser());
    page = await browser.newPage();
    await page.setViewport({ width: 240, height: 180, deviceScaleFactor: 1 });
  });
  after(async () => { if (browser) await browser.close(); });

  async function compare(name, style, contents = '') {
    await page.setContent(`<style>html,body{margin:0;padding:0;width:240px;height:180px}
      *{box-sizing:border-box}</style>
      <div style="position:absolute;left:50px;top:40px;width:120px;height:80px;${style}">${contents}</div>`);
    const reference = PNG.sync.read(Buffer.from(await page.screenshot({ omitBackground: true })));
    const snapshot = await page.evaluate(takeSnapshot);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), `pagx-shadow-${name}-`));
    const input = path.join(dir, 'source.html'), output = path.join(dir, 'output.pagx');
    fs.writeFileSync(input, snapshot.html);
    fs.writeFileSync(path.join(dir, 'reference.png'), PNG.sync.write(reference));
    for (const args of [
      ['import', '--input', input, '--output', output, '--html-no-snapshot'],
      ['render', output, '--output', path.join(dir, 'actual.png')],
    ]) {
      const result = spawnSync(binary, args, {
        encoding: 'utf8', timeout: 30000,
      });
      assert.equal(result.status, 0, `${result.stderr}; ${dir}`);
    }
    const actual = PNG.sync.read(fs.readFileSync(path.join(dir, 'actual.png')));
    assert.equal(actual.width, reference.width);
    assert.equal(actual.height, reference.height);
    let sum = 0;
    for (let y = 0; y < actual.height; y++) {
      for (let x = 0; x < actual.width; x++) {
        const at = (y * actual.width + x) * 4;
        for (let c = 0; c < 4; c++) {
          const a = c === 3 ? actual.data[at + c] : actual.data[at + c] * actual.data[at + 3] / 255;
          const b = c === 3 ? reference.data[at + c] : reference.data[at + c] * reference.data[at + 3] / 255;
          sum += Math.abs(a - b);
        }
        if (x < 2 || y < 2 || x >= actual.width - 2 || y >= actual.height - 2) continue;
        let flat = true;
        for (let dy = -2; dy <= 2 && flat; dy++) {
          for (let dx = -2; dx <= 2 && flat; dx++) {
            const near = ((y + dy) * actual.width + x + dx) * 4;
            for (let c = 0; c < 4; c++) {
              if (reference.data[at + c] !== reference.data[near + c]) flat = false;
            }
          }
        }
        if (flat) {
          for (let c = 0; c < 4; c++) {
            if (c < 3 && reference.data[at + 3] === 0 && actual.data[at + 3] === 0) continue;
            assert.ok(Math.abs(reference.data[at + c] - actual.data[at + c]) <= 4,
              `${name}: (${x},${y}) channel ${c}; ${dir}`);
          }
        }
      }
    }
    assert.ok(sum / actual.data.length < 1.5, `${name}: mean ${sum / actual.data.length}; ${dir}`);
    fs.rmSync(dir, { recursive: true });
  }

  it('uses an opaque box silhouette even for a transparent box with overflowing content', async () => {
    await compare('transparent', 'border-radius:8px;box-shadow:0 0 0 5px red',
      '<div style="position:absolute;left:130px;top:0;width:12px;height:12px;background:blue"></div>');
  });

  it('clips spread inset shadows to the rounded padding edge', async () => {
    await compare('inset', 'border:5px solid blue;border-radius:15px;' +
      'background:rgba(255,255,0,.4);box-shadow:inset 3px 2px 6px 8px rgba(0,0,0,.6)');
  });

  it('preserves multiple shadow order and excludes outer shadows from overflow clipping', async () => {
    await compare('stack', 'border:4px solid black;border-radius:10px;overflow:hidden;' +
      'box-shadow:0 0 0 3px red,0 0 0 8px blue,inset 0 0 0 6px green,0 0 0 -90px black');
  });

  it('supports blurred negative spread, offsets and transparent backgrounds', async () => {
    await compare('negative', 'border-radius:30px;background:rgba(255,0,0,.2);' +
      'box-shadow:4px 8px 12px -3px rgba(0,0,255,.6)');
  });

  it('expands both axes of an elliptical border box', async () => {
    await compare('ellipse', 'border-radius:50%;box-shadow:3px 2px 4px 6px red');
  });

  it('contracts both axes of an elliptical inset shadow', async () => {
    await compare('ellipse-inset', 'border-radius:50%;background:yellow;' +
      'box-shadow:inset 3px 2px 4px 6px red');
  });

  it('adjusts small, asymmetric corner radii when spread exceeds their radius', async () => {
    await compare('asymmetric', 'border-radius:2px 8px 12px 0;' +
      'box-shadow:1px 3px 0 16px red');
  });
});
