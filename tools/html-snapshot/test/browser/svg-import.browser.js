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

const binary = process.env.PAGX_SVG_TEST_BIN ||
  path.resolve(__dirname, '../../../../cmake-build-debug/pagx');

describe('SVG rendering agrees with Chromium', { skip: !fs.existsSync(binary) }, () => {
  let browser;
  let page;
  before(async () => {
    ({ browser } = await launchBrowser({ engine: 'puppeteer' }));
    page = await browser.newPage();
    await page.setViewport({ width: 240, height: 180, deviceScaleFactor: 1 });
  });
  after(async () => { if (browser) await browser.close(); });

  async function compare(name, content, { meanLimit = 1, flatTolerance = 3 } = {}) {
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="240" height="180">${content}</svg>`;
    await page.setContent(`<style>html,body{margin:0;padding:0;width:240px;height:180px}
      svg{display:block}</style>${svg}`);
    await page.evaluate(() => document.fonts.ready);
    const reference = PNG.sync.read(Buffer.from(await page.screenshot({ omitBackground: true })));
    const snapshot = await page.evaluate(takeSnapshot);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), `pagx-svg-${name}-`));
    for (const [format, source] of [['svg', svg], ['html', snapshot.html]]) {
      const input = path.join(dir, `source.${format}`);
      const output = path.join(dir, `${format}.pagx`);
      const png = path.join(dir, `${format}.png`);
      fs.writeFileSync(input, source);
      for (const args of [
        ['import', '--input', input, '--output', output, '--format', format],
        ['render', output, '--output', png],
      ]) {
        const result = spawnSync(binary, args, {
          env: { ...process.env, PAGX_HTML_SNAPSHOT: '0' }, encoding: 'utf8', timeout: 30000,
        });
        assert.equal(result.status, 0, `${name}/${format}: ${result.stderr}; ${dir}`);
      }
      const actual = PNG.sync.read(fs.readFileSync(png));
      assert.equal(actual.width, reference.width);
      assert.equal(actual.height, reference.height);
      let sum = 0;
      for (let y = 0; y < actual.height; y++) {
        for (let x = 0; x < actual.width; x++) {
          const at = (y * actual.width + x) * 4;
          // Compare premultiplied colours so RGB in fully transparent pixels is irrelevant.
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
              assert.ok(Math.abs(reference.data[at + c] - actual.data[at + c]) <= flatTolerance,
                `${name}/${format}: (${x},${y}) channel ${c}: ` +
                `${reference.data[at + c]} != ${actual.data[at + c]}; ${dir}`);
            }
          }
        }
      }
      assert.ok(sum / actual.data.length < meanLimit,
        `${name}/${format}: mean error ${sum / actual.data.length}; ${dir}`);
    }
    fs.rmSync(dir, { recursive: true });
  }

  it('instances a shared mask in each transformed owner coordinate system', async () => {
    await compare('local-masks', `<defs>
      <mask id="m" maskUnits="userSpaceOnUse" x="0" y="0" width="80" height="60">
        <rect width="40" height="60" fill="white"/>
      </mask></defs>
      <g fill="red" transform="translate(20 20)" mask="url(#m)">
        <rect width="80" height="60"/></g>
      <g fill="blue" transform="translate(220 160) scale(-1 -1)" mask="url(#m)">
        <rect width="80" height="60"/></g>`);
  });

  it('keeps mask region, content units and clip geometry independent', async () => {
    await compare('mask-units', `<defs>
      <mask id="m" x=".25" y="0" width=".5" height="1" maskContentUnits="objectBoundingBox">
        <rect width="1" height="1" fill="white"/>
      </mask>
      <clipPath id="c" clipPathUnits="objectBoundingBox">
        <path d="M0 0H1V1H0Z M.4 .4H.6V.6H.4Z" clip-rule="evenodd"
              fill="none" opacity="0" stroke="black" stroke-width="2"/>
      </clipPath></defs>
      <g transform="translate(30 25)" mask="url(#m)" clip-path="url(#c)">
        <rect x="10" y="10" width="160" height="100" fill="#e02020"/>
      </g>`);
  });

  it('does not paint a reflection whose local mask does not overlap its geometry', async () => {
    await compare('reflection', `<defs><mask id="m">
      <rect x="0" y="90" width="240" height="90" fill="white"/>
      </mask></defs>
      <rect width="240" height="180" fill="#102030"/>
      <g transform="translate(0 180) scale(1 -1)" mask="url(#m)">
        <rect x="20" y="10" width="180" height="80" fill="red"/>
      </g>`);
  });

  it('uses curve extrema for object bounding box gradients', async () => {
    await compare('curve-bounds', `<defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop stop-color="red"/><stop offset="1" stop-color="blue"/>
      </linearGradient></defs>
      <path d="M20 120 Q120 -80 220 120 L220 160H20Z" fill="url(#g)"/>`,
    { meanLimit: 1.5 });
  });

  it('inherits textPath typography and anchors the shaped text chunk', async () => {
    await compare('text-path', `<defs><path id="p" d="M10 90H210" transform="translate(10 0)"
      pathLength="100"/></defs>
      <text font-family="Arial" font-size="12" fill="#2040d0">
        <textPath href="#p" startOffset="50" text-anchor="middle" font-size="22">Path text</textPath>
      </text>`, { meanLimit: 1.5, flatTolerance: 8 });
  });

  it('preserves nested styled spans and separate positioned text chunks', async () => {
    await compare('text-spans', `<text x="120" y="60" font-family="Arial" font-size="20"
      text-anchor="middle" fill="#2040d0">A<tspan font-size="28" fill="#e02020">B<tspan>C</tspan></tspan>D<tspan
      x="120" y="130" font-size="24">Second</tspan></text>`,
    { meanLimit: 1.5, flatTolerance: 8 });
  });

  it('preserves CSS overrides of presentation attributes', async () => {
    await compare('css-paint', `<style>.paint{fill:#e02020;stroke:#2040d0;stroke-width:6}</style>
      <rect class="paint" x="30" y="30" width="180" height="120" fill="green" stroke="black"/>`);
  });

  it('applies textLength spacing across styled path text as one chunk', async () => {
    await compare('path-spacing', `<defs><path id="p" d="M10 90H230"/></defs>
      <text font-family="Arial" font-size="20" textLength="180" fill="#2040d0">
        <textPath href="#p" startOffset="50%" text-anchor="middle">A<tspan
          fill="#e02020">BC</tspan>D</textPath></text>`,
    { meanLimit: 1.5, flatTolerance: 8 });
  });

  it('keeps relative styles in symbols until each use is instantiated', async () => {
    await compare('symbol-style', `<defs>
      <symbol id="s" viewBox="0 0 80 60" width="80" height="60" overflow="visible">
        <rect width="35" height="20" fill="inherit"/>
        <text x="0" y="50" fill="currentColor" font-family="Arial" font-size="1em">Ab</text>
      </symbol></defs>
      <use href="#s" x="20" y="20" fill="#e02020" color="#2040d0" font-size="20"/>
      <use href="#s" x="130" y="100" fill="#2040d0" color="#e02020" font-size="28"/>`,
    { meanLimit: 1.5, flatTolerance: 8 });
  });
});
