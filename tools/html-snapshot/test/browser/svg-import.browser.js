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
    fs.writeFileSync(path.join(dir, 'reference.png'), PNG.sync.write(reference));
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

  it('inherits markers and honors none, marker viewBox and auto-start-reverse', async () => {
    await compare('marker-viewport', `<defs>
      <marker id="a" markerWidth="12" markerHeight="8" viewBox="0 0 6 4"
        refX="5" refY="2" orient="auto-start-reverse" markerUnits="userSpaceOnUse" fill="#e02020">
        <path d="M0 0L6 2L0 4Z"/>
      </marker></defs>
      <g fill="none" stroke="#2040d0" stroke-width="3" marker-start="url(#a)" marker-end="url(#a)">
        <path d="M30 40H210"/>
        <line x1="30" y1="90" x2="210" y2="90" marker-start="none"/>
        <path d="M30 140C30 140 150 140 210 140" marker-end="none"/>
      </g>`);
  });

  it('places markers only at authored vertices including arcs and closed subpaths', async () => {
    await compare('marker-vertices', `<defs>
      <marker id="m" markerWidth="7" markerHeight="5" refX="3" refY="2"
        orient="auto" markerUnits="userSpaceOnUse" overflow="visible">
        <path d="M0 0L6 2L0 4Z" fill="#e02020"/>
      </marker></defs>
      <path d="M30 60A45 35 0 0 1 120 60L180 60L180 60Q210 60 210 100
               M35 120H90L60 160Z" fill="none" stroke="#2040d0" stroke-width="2"
        marker-start="url(#m)" marker-mid="url(#m)" marker-end="url(#m)"/>`,
    { meanLimit: 1.5 });
  });

  it('keeps marker definition paint separate from the referencing path', async () => {
    await compare('marker-paint', `<defs>
      <marker id="a" markerWidth="12" markerHeight="12" refX="8" refY="6"
        orient="auto" markerUnits="userSpaceOnUse">
        <path d="M0 0L12 6L0 12Z"/>
      </marker>
      <marker id="b" markerWidth="12" markerHeight="12" refX="8" refY="6"
        orient="auto" markerUnits="userSpaceOnUse">
        <path d="M0 0L12 6L0 12Z" fill="context-stroke"/>
      </marker></defs>
      <path d="M30 50H200" fill="none" stroke="#e02020" stroke-width="2" marker-end="url(#a)"/>
      <path d="M30 120H200" fill="none" stroke="#2040d0" stroke-width="2" marker-end="url(#b)"/>`);
  });

  it('keeps context gradients continuous across transformed marker contents', async () => {
    await compare('marker-context-gradient', `<defs>
      <linearGradient id="g" gradientUnits="userSpaceOnUse" x1="0%" x2="100%">
        <stop stop-color="#e02020"/><stop offset="1" stop-color="#2040d0"/>
      </linearGradient>
      <marker id="m" markerWidth="40" markerHeight="30" viewBox="0 0 20 15"
        refX="15" refY="7.5" orient="auto" markerUnits="userSpaceOnUse">
        <g transform="translate(2 1)"><path d="M0 0L18 6.5L0 13Z" fill="context-stroke"/></g>
      </marker></defs>
      <path d="M30 35L200 150" fill="none" stroke="url(#g)" stroke-width="6"
        marker-start="url(#m)" marker-end="url(#m)"/>`);
  });

  it('maps gradients through non-square bounding boxes and gradient transforms', async () => {
    await compare('gradient-mapping', `<defs>
      <linearGradient id="l" x2="1" y2="1" gradientTransform="rotate(15 .5 .5)">
        <stop stop-color="#e02020"/><stop offset="1" stop-color="#2040d0"/>
      </linearGradient>
      <radialGradient id="r"><stop stop-color="white"/><stop offset="1" stop-color="#2040d0"/></radialGradient>
      </defs><rect x="20" y="20" width="200" height="50" fill="url(#l)"/>
      <rect x="20" y="100" width="200" height="60" fill="url(#r)"/>`);
  });

  it('tiles vector paths with negative origins and rotated pattern transforms', async () => {
    await compare('pattern-wave', `<defs>
      <pattern id="p" patternUnits="userSpaceOnUse" x="-7" y="5" width="24" height="12"
        patternTransform="rotate(12)">
        <path d="M0 6Q6 0 12 6T24 6" fill="none" stroke="white" stroke-opacity=".4" stroke-width="2"/>
      </pattern></defs><rect width="240" height="180" fill="#204070"/>
      <path d="M20 30H220V150H20Z M90 60V120H150V60Z" fill-rule="evenodd"
        fill="url(#p)" fill-opacity=".6"/>`, { meanLimit: 1.5 });
  });

  it('keeps pattern units and content units independent for non-square shapes', async () => {
    await compare('pattern-units', `<defs>
      <pattern id="a" width=".25" height=".5"><rect width="12" height="10" fill="#e02020"/></pattern>
      <pattern id="b" x=".1" y=".2" width=".25" height=".5" patternContentUnits="objectBoundingBox">
        <rect width=".1" height=".25" fill="#2040d0"/></pattern>
      <pattern id="c" patternUnits="userSpaceOnUse" width="40" height="15" patternContentUnits="objectBoundingBox">
        <rect width=".1" height=".2" fill="#008040"/></pattern>
      </defs>
      <rect x="20" y="10" width="200" height="40" fill="url(#a)"/>
      <rect x="20" y="65" width="160" height="50" fill="url(#b)"/>
      <rect x="20" y="130" width="200" height="40" fill="url(#c)"/>`);
  });

  it('transforms bounding-box pattern tiles in the referring user space', async () => {
    await compare('pattern-box-transform', `<defs>
      <pattern id="p" x=".1" y="-.2" width=".25" height=".5"
        patternContentUnits="objectBoundingBox" patternTransform="rotate(12)">
        <rect width=".15" height=".3" fill="#2040d0"/>
      </pattern></defs><rect x="30" y="25" width="180" height="110" fill="url(#p)"/>`,
    { meanLimit: 1.5 });
  });

  it('resolves pattern template chains and fits a viewBox into each tile', async () => {
    await compare('pattern-template', `<defs>
      <pattern id="base" width=".25" height=".5" viewBox="0 0 10 10" fill="#e02020">
        <rect x="1" y="1" width="8" height="8"/></pattern>
      <pattern id="middle" href="#base" preserveAspectRatio="xMaxYMid meet"/>
      <pattern id="p" href="#middle" fill="#2040d0"/>
      </defs><rect x="20" y="20" width="200" height="120" fill="url(#p)"/>`);
  });

  it('uses the stroke outline and applies paint opacity after tile compositing', async () => {
    await compare('pattern-stroke', `<defs>
      <pattern id="p" width="20" height="20" patternUnits="userSpaceOnUse">
        <rect width="14" height="20" fill="#e02020"/><rect x="8" width="12" height="20" fill="#2040d0"/>
      </pattern></defs><path d="M30 40L200 40L100 140" fill="none" stroke="url(#p)"
        stroke-width="16" stroke-linecap="round" stroke-linejoin="miter"
        stroke-dasharray="24 8" stroke-opacity=".5"/>`, { meanLimit: 1.5 });
  });

  it('instances masks independently inside repeated pattern tiles', async () => {
    await compare('pattern-mask', `<defs>
      <mask id="m" maskContentUnits="objectBoundingBox"><rect width=".5" height="1" fill="white"/></mask>
      <pattern id="p" width="40" height="30" patternUnits="userSpaceOnUse">
        <rect x="4" y="4" width="32" height="22" fill="#e02020" mask="url(#m)"/>
      </pattern></defs><rect x="20" y="20" width="200" height="140" fill="url(#p)"/>`);
  });

  it('keeps referenced shape styles and effects inside pattern content', async () => {
    await compare('pattern-use', `<defs>
      <rect id="r" x="2" y="2" width="12" height="8" fill="#e02020" transform="translate(1 1)"/>
      <g id="g" opacity=".5"><circle cx="8" cy="8" r="6" fill="#2040d0"/></g>
      <pattern id="p" width="40" height="30" patternUnits="userSpaceOnUse" fill="none">
        <use href="#r" x="3" y="2"/><use href="#g" x="20" y="10"/>
      </pattern></defs><rect x="20" y="20" width="200" height="140" fill="url(#p)"/>`);
  });

  it('continues a context pattern into marker contents', async () => {
    await compare('pattern-context', `<defs>
      <pattern id="p" width="20" height="10" patternUnits="userSpaceOnUse">
        <rect width="10" height="10" fill="#e02020"/><rect x="10" width="10" height="10" fill="#2040d0"/>
      </pattern>
      <marker id="m" markerUnits="userSpaceOnUse" markerWidth="40" markerHeight="30"
        refX="30" refY="15" orient="auto"><path d="M0 0L40 15L0 30Z" fill="context-stroke"/></marker>
      </defs><path d="M30 40L200 140" fill="none" stroke="url(#p)" stroke-width="8"
        marker-end="url(#m)"/>`, { meanLimit: 1.5 });
  });

  it('honors the image geometry and spacing inside pattern tiles', async () => {
    const bitmap = new PNG({ width: 2, height: 2 });
    bitmap.data.fill(255);
    for (let i = 0; i < 4; i++) bitmap.data[i * 4 + 1] = 32;
    const data = PNG.sync.write(bitmap).toString('base64');
    await compare('pattern-image', `<defs>
      <image id="image" width="12" height="10" href="data:image/png;base64,${data}"/>
      <pattern id="p" x="3" y="7" width="30" height="24" patternUnits="userSpaceOnUse">
        <use href="#image" x="5" y="4"/>
      </pattern></defs><rect x="20" y="20" width="200" height="140" fill="url(#p)"/>`);
  });
});
