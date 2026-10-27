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

describe('fractional layout at the canvas boundary', { skip: !fs.existsSync(binary) }, () => {
  let browser;
  before(async () => { ({ browser } = await launchBrowser()); });
  after(async () => { if (browser) await browser.close(); });

  for (const scale of [1, 2]) {
    it(`preserves a bottom border through snapshot, import and render at ${scale}x`, async () => {
      const page = await browser.newPage();
      await page.setViewport({ width: 180, height: 100, deviceScaleFactor: scale });
      // The border ends at 39.984375. Rounding each offset and the 100% background size
      // independently moves it to 41, outside the 40-pixel canvas.
      await page.setContent(`<!doctype html><style>
        html,body{margin:0;padding:0}body{width:180px;height:39.984375px;background:white}
        *{box-sizing:border-box}div{position:absolute}
        </style><div style="left:40.25px;top:.796875px;width:100px;height:39.1875px">
          <div style="left:0;top:7.84375px;width:100px;height:31.34375px">
            <div style="left:0;top:0;width:85.8125px;height:31.34375px;border:1px solid black;border-radius:999px"></div>
          </div>
        </div>`);
      const reference = PNG.sync.read(Buffer.from(await page.screenshot({
        clip: { x: 0, y: 0, width: 180, height: 40 }, omitBackground: true,
      })));
      const snapshot = await page.evaluate(takeSnapshot);
      await page.close();
      const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-fractional-'));
      const input = path.join(dir, 'source.html');
      const output = path.join(dir, 'output.pagx');
      fs.writeFileSync(input, snapshot.html);
      fs.writeFileSync(path.join(dir, 'reference.png'), PNG.sync.write(reference));
      for (const args of [
        ['import', '--html-source', 'snapshot', '--input', input, '--output', output],
        ['render', output, '--scale', String(scale), '--output', path.join(dir, 'actual.png')],
      ]) {
        const result = spawnSync(binary, args, {
          encoding: 'utf8', timeout: 30000,
        });
        assert.equal(result.status, 0, `${result.stderr}; ${dir}`);
      }
      const actual = PNG.sync.read(fs.readFileSync(path.join(dir, 'actual.png')));
      assert.equal(actual.width, reference.width);
      assert.equal(actual.height, reference.height);
      const bottomCenter = ((actual.height - 1) * actual.width + 80 * scale) * 4;
      assert.ok(reference.data[bottomCenter] < 12, `reference bottom border; ${dir}`);
      assert.ok(actual.data[bottomCenter] < 12, `exported bottom border is clipped; ${dir}`);
      let error = 0;
      for (let i = 0; i < actual.data.length; i++) {
        error += Math.abs(actual.data[i] - reference.data[i]);
      }
      assert.ok(error / actual.data.length < 1, `mean error ${error / actual.data.length}; ${dir}`);
      fs.rmSync(dir, { recursive: true });
    });
  }
});
