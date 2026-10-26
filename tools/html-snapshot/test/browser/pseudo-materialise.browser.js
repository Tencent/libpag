'use strict';

// Run separately from the browser-free Jest suite: npm run test:browser.
// Real CSS matching, live computed styles and animation precedence are the regressions here;
// stubbing getComputedStyle cannot exercise them.
const assert = require('node:assert/strict');
const { describe, it, before, after, beforeEach, afterEach } = require('node:test');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const {
  materializeDecorativePseudoElements,
  takeSnapshot,
  SNAPSHOT_INIT_SCRIPT,
  TAKE_SNAPSHOT_EXPR,
} = require('../../dist/lib/browser-snapshot');

describe('pseudo materialisation in Chromium', () => {
  let browser;
  let page;

  before(async () => {
    ({ browser } = await launchBrowser({ engine: 'puppeteer' }));
  });
  after(async () => { if (browser) await browser.close(); });
  beforeEach(async () => {
    page = await browser.newPage();
    await page.setViewport({ width: 240, height: 160, deviceScaleFactor: 1 });
  });
  afterEach(async () => { if (page) await page.close(); });

  async function load(css, body) {
    await page.setContent(`<style>html, body { margin: 0; padding: 0; } ${css}</style>${body}`);
  }

  async function pixel(x, y) {
    const png = PNG.sync.read(Buffer.from(await page.screenshot()));
    const offset = (y * png.width + x) * 4;
    return [...png.data.subarray(offset, offset + 4)];
  }

  async function previewSnapshot() {
    const result = await page.evaluate(takeSnapshot);
    assert.doesNotMatch(result.html, /__pagxSnapshotPseudo|data-snapshot-pseudo/);
    await page.setContent(result.html);
    await page.evaluate(() => {
      Object.assign(document.documentElement.style, { margin: '0', padding: '0', display: 'block' });
    });
    return result;
  }

  for (const pseudo of ['before', 'after']) {
    for (const zIndex of ['auto', '-2', '0', '1', '3']) {
      for (const important of [false, true]) {
        it(`preserves ::${pseudo} z-index:${zIndex}, child rule${important ? ' !important' : ''}`, async () => {
          await load(`
            .host { position: relative; isolation: isolate; width: 120px; height: 80px; background: white; }
            .host > div { position: absolute; inset: 0; background: rgb(0, 200, 0); z-index: 1 ${important ? '!important' : ''}; }
            .host::${pseudo} { content: ''; position: absolute; inset: 0; background: rgb(200, 0, 0); z-index: ${zIndex}; }
          `, '<div class="host"><div></div></div>');
          const baseline = await pixel(20, 20);
          await page.evaluate(materializeDecorativePseudoElements);
          await previewSnapshot();
          assert.deepEqual(await pixel(20, 20), baseline);
        });
      }
    }
  }

  it('uses the source position for a static pseudo with an otherwise inactive z-index', async () => {
    await load(`
      .host { position: relative; isolation: isolate; width: 120px; height: 80px; }
      .host::before { content: ''; display: block; width: 120px; height: 80px; background: red; z-index: 5; }
      .host > span { position: absolute; inset: 0; background: green; z-index: 1; }
      .host > div { position: relative !important; }
    `, '<div class="host"><span></span></div>');
    const baseline = await pixel(20, 20);
    await page.evaluate(materializeDecorativePseudoElements);
    await previewSnapshot();
    assert.deepEqual(await pixel(20, 20), baseline);
  });

  for (const display of ['flex', 'grid']) {
    it(`keeps a static ${display} pseudo's active z-index`, async () => {
      await load(`
        .host { display: ${display}; position: relative; isolation: isolate; width: 120px; height: 80px; }
        .host::before { content: ''; display: block; width: 120px; height: 80px; background: red; z-index: 3; }
        .host > span { position: absolute; inset: 0; background: green; z-index: 1; }
        .host > div { z-index: -1 !important; }
      `, '<div class="host"><span></span></div>');
      const baseline = await pixel(20, 20);
      await page.evaluate(materializeDecorativePseudoElements);
      await previewSnapshot();
      assert.deepEqual(await pixel(20, 20), baseline);
    });
  }

  for (const selector of ['.host > div', '.host > *']) {
    it(`retains default computed values against ${selector}`, async () => {
      await load(`
        .host { position: relative; width: 120px; height: 80px; }
        .host::after { content: ''; position: absolute; inset: 0; }
        ${selector} { z-index: 9; opacity: .2; transform: translateX(17px); margin: 9px;
          padding: 3px; border: 4px solid red; background: black; box-shadow: 2px 2px 2px black;
          filter: blur(2px); overflow: hidden; white-space: pre; animation: unrelated 1s infinite; }
        @keyframes unrelated { to { opacity: .4; } }
      `, '<div class="host"></div>');
      const properties = ['z-index', 'opacity', 'transform', 'margin-top', 'padding-top',
        'border-top-width', 'background-color', 'box-shadow', 'filter', 'overflow',
        'white-space', 'animation-name'];
      const original = await page.evaluate((props) => {
        const style = getComputedStyle(document.querySelector('.host'), '::after');
        return props.map(prop => style.getPropertyValue(prop));
      }, properties);
      await page.evaluate(materializeDecorativePseudoElements);
      const actual = await page.evaluate((props) => {
        const style = getComputedStyle(document.querySelector('[data-snapshot-pseudo]'));
        return props.map(prop => style.getPropertyValue(prop));
      }, properties);
      assert.deepEqual(actual, original);
    });
  }

  it('collects both pseudos and later hosts before structural selectors can change their styles', async () => {
    await load(`
      section { position: relative; width: 120px; height: 40px; }
      section::before, section::after { content: ''; position: absolute; width: 10px; height: 10px; background: red; }
      #first:has(> div)::after, #first:has(> div) + #second::before { width: 90px; background: blue; z-index: 9; }
    `, '<section id="first"></section><section id="second"></section>');
    await page.evaluate(materializeDecorativePseudoElements);
    const actual = await page.evaluate(() => [...document.querySelectorAll('[data-snapshot-pseudo]')].map(el => {
      const cs = getComputedStyle(el);
      return { width: cs.width, color: cs.backgroundColor, zIndex: cs.zIndex };
    }));
    assert.deepEqual(actual, Array.from({ length: 4 }, () => ({ width: '10px', color: 'rgb(255, 0, 0)', zIndex: 'auto' })));
  });

  for (const display of ['flex', 'grid']) {
    it(`preserves in-flow ${display} geometry and remains idempotent`, async () => {
      await load(`
        .host { display: ${display}; width: 120px; gap: 8px; grid-template-columns: 10px 30px 10px; align-items: center; }
        .host::before, .host::after { content: ''; width: 10px; height: 10px; background: red; }
        .host > span { display: block; width: 30px; height: 20px; background: green; }
        .host > div { position: relative; margin: 15px; padding: 6px; flex-grow: 1; min-width: 40px; }
      `, '<section class="host"><span></span></section>');
      const bounds = () => page.evaluate(() => ['.host', '.host > span'].map(selector =>
        document.querySelector(selector).getBoundingClientRect().toJSON()));
      const baseline = await bounds();
      await page.evaluate(materializeDecorativePseudoElements);
      assert.deepEqual(await bounds(), baseline);
      await page.evaluate(materializeDecorativePseudoElements);
      assert.deepEqual(await bounds(), baseline);
      assert.equal(await page.$$eval('[data-snapshot-pseudo]', els => els.length), 2);
    });
  }

  it('keeps opacity and transform keyframes active over the copied resting defaults', async () => {
    await load(`
      .host { position: relative; width: 120px; height: 80px; }
      .host::after { content: ''; position: absolute; inset: 0; background: red;
        animation: reveal 1s linear both; animation-play-state: paused; }
      @keyframes reveal { from { opacity: 0; transform: translateX(20px); }
        to { opacity: 1; transform: translateX(0px); } }
    `, '<div class="host"></div>');
    await page.evaluate(materializeDecorativePseudoElements);
    const actual = await page.evaluate(() => {
      const el = document.querySelector('[data-snapshot-pseudo]');
      const animation = el.getAnimations()[0];
      animation.pause();
      const values = [0, 500, 1000].map(time => {
        animation.currentTime = time;
        const cs = getComputedStyle(el);
        return { opacity: Number(cs.opacity), x: new DOMMatrixReadOnly(cs.transform).m41 };
      });
      return { opacity: el.style.opacity, transform: el.style.transform, values };
    });
    assert.equal(actual.opacity, '1');
    assert.equal(actual.transform, 'none');
    assert.deepEqual(actual.values, [{ opacity: 0, x: 20 }, { opacity: .5, x: 10 }, { opacity: 1, x: 0 }]);
  });

  it('shares private paint metadata with the separately installed snapshot payload', async () => {
    await load(`
      .host { position: relative; width: 120px; height: 80px; }
      .host > div { position: absolute; inset: 0; background: green; z-index: 1 !important; }
      .host::after { content: ''; position: absolute; inset: 0; background: red; }
    `, '<div class="host"><div></div></div>');
    const baseline = await pixel(20, 20);
    await page.evaluate(SNAPSHOT_INIT_SCRIPT);
    await page.evaluate(materializeDecorativePseudoElements);
    const result = await page.evaluate(TAKE_SNAPSHOT_EXPR);
    await page.setContent(result.html);
    await page.evaluate(() => {
      Object.assign(document.documentElement.style, { margin: '0', padding: '0', display: 'block' });
    });
    assert.deepEqual(await pixel(20, 20), baseline);
  });
});
