'use strict';

const assert = require('node:assert/strict');
const { describe, it, before, after, beforeEach, afterEach } = require('node:test');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { captureTextLayout, installTextLayout } = require('../../dist/lib/text-layout');
const { HELPERS_SRC, PAYLOAD_CONSTANTS_SRC, takeSnapshot } =
  require('../../dist/lib/browser-snapshot');

describe('browser text fragments', () => {
  let browser, page;
  before(async () => { ({ browser } = await launchBrowser({ engine: 'puppeteer' })); });
  after(async () => { if (browser) await browser.close(); });
  beforeEach(async () => {
    page = await browser.newPage();
    await page.setViewport({ width: 800, height: 600, deviceScaleFactor: 1 });
  });
  afterEach(async () => { await page.close(); });

  async function load(body, css = '', protocol = true) {
    await page.setContent(`<style>
      body { margin:0; } p { margin:0; width:240px; font:18px/30px Arial; }
      ${css}</style>${body}`);
    await page.evaluate(() => document.fonts.ready);
    if (protocol) await page.evaluate(installTextLayout, await captureTextLayout(page, 'puppeteer'));
  }

  async function fragments(protocol = true) {
    return page.evaluate(`(() => {
      ${PAYLOAD_CONSTANTS_SRC}
      ${HELPERS_SRC}
      const host = document.querySelector('#host');
      const walker = document.createTreeWalker(host, NodeFilter.SHOW_TEXT);
      const result = [];
      let node;
      while (node = walker.nextNode()) {
        if (!node.nodeValue.trim()) continue;
        const cs = getComputedStyle(node.parentElement);
        const axis = cs.writingMode.startsWith('vertical') ? 'x-rl' : 'y';
        const items = ${protocol ? 'snapshotTextFragments(node, cs)' :
          'splitTextNodeIntoLines(node, cs.whiteSpace, axis)'};
        if (!items) { result.push({ missing: node.nodeValue }); continue; }
        for (const item of items) {
          const style = item.computed || cs;
          result.push({
            text:item.text, start:item.start, end:item.end, initial:!!item.initial,
            source:node.nodeValue.slice(item.start,item.end),
            left:item.rect.left, top:item.rect.top, width:item.rect.width, height:item.rect.height,
            size:style.getPropertyValue('font-size'), color:style.getPropertyValue('color'),
          });
        }
      }
      return result;
    })()`);
  }

  const prose = '当我们在屏幕上读到一个汉字时，很少会去想它从何而来。' +
    '它看起来理所当然：横平竖直，大小一致，排列整齐。'.repeat(2);
  const cap = 'p::first-letter{float:left;font:72px/.9 serif;color:rgb(181,69,42);margin:8px 12px 0 0}';
  for (const [name, content, first] of [
    ['drop cap', prose, '当'],
    ['surrounding punctuation', '“当”' + prose.slice(1), '“当”'],
    ['combining character', 'A\u0301 quick brown fox jumps over the lazy dog.', 'A\u0301'],
    ['nested inline', '<em>当</em>' + prose.slice(1), '当'],
  ]) {
    it(`keeps the effective style and exact source range of ${name}`, async () => {
      await load(`<p id="host">${content}</p>`, cap);
      const parts = await fragments();
      assert.ok(parts.every((p) => !p.missing), JSON.stringify(parts));
      const initials = parts.filter((p) => p.initial);
      assert.equal(initials.map((p) => p.text).join(''), first);
      assert.ok(initials.every((p) => p.size === '72px' && p.color === 'rgb(181, 69, 42)'));
      assert.equal(parts.map((p) => p.source).join('').replace(/\s/g, ''),
        content.replace(/<[^>]*>/g, '').replace(/\s/g, ''));
      assert.ok(parts.filter((p) => !p.initial).every((p) => p.height < 40));
      const snapshot = await page.evaluate(takeSnapshot);
      assert.match(snapshot.html, /font-size: 72px/);
      assert.equal((snapshot.html.match(/font-size: 72px/g) || []).length, 1);
    });
  }

  it('keeps an inline first letter on the same visual line without merging its style', async () => {
    await load(`<p id="host">${prose}</p>`, cap + 'p::first-letter{float:none}');
    const parts = await fragments();
    assert.equal(parts[0].text, '当');
    assert.equal(parts[0].size, '72px');
    assert.equal(parts[1].size, '18px');
  });

  it('diagnoses a generated first letter without a DOM source range', async () => {
    await load(`<p id="host">${prose}</p>`, cap + 'p::before{content:"“"}');
    const snapshot = await page.evaluate(takeSnapshot);
    assert.ok(snapshot.warnings.some((w) => /first-letter/.test(w)), snapshot.warnings.join('\n'));
  });

  it('diagnoses first-letter style in the standalone fallback', async () => {
    await load(`<p id="host">${prose}</p>`, cap, false);
    const snapshot = await page.evaluate(takeSnapshot);
    assert.ok(snapshot.warnings.some((w) => /first-letter.*metadata/.test(w)));
  });

  for (const protocol of [true, false]) {
    for (const [name, css] of [
      ['columns', '#host{columns:2;column-gap:32px;width:400px}'],
      ['overlapping line boxes', '#host{line-height:8px}'],
      ['vertical columns', '#host{writing-mode:vertical-rl;height:180px;width:200px}'],
    ]) {
      it(`${name} preserves text once and in source order (${protocol ? 'protocol' : 'DOM'})`, async () => {
        await load(`<p id="host">${prose}</p>`, css, protocol);
        const parts = await fragments(protocol);
        assert.ok(parts.length > 1);
        assert.equal(parts.map((p) => p.source).join(''), prose);
        if (name === 'columns') assert.ok(parts.some((p) => p.left > 200));
      });
    }
  }

  it('preserves justified line fragments without character-position metadata', async () => {
    await load(`<p id="host">${prose}</p>`, '#host{text-align:justify}');
    const parts = await fragments();
    assert.ok(parts.length > 1);
    assert.equal(parts.map((p) => p.text).join(''), prose);
    assert.ok(parts.at(-1).width < 240);
    const snapshot = await page.evaluate(takeSnapshot);
    assert.doesNotMatch(snapshot.html, /data-pagx-inline-positions=/);
  });

  it('keeps RTL fragments in logical text order', async () => {
    await load('<p id="host">שלום עולם שלום עולם שלום עולם</p>', '#host{direction:rtl;text-align:justify}');
    const parts = await fragments();
    assert.ok(parts.every((p) => !p.missing));
    assert.equal(parts.map((p) => p.text).join('').replace(/\s/g, ''),
      'שלום עולם שלום עולם שלום עולם'.replace(/\s/g, ''));
  });

  it('maps text-transform expansions back to the source range', async () => {
    await load('<p id="host">ßtraße words words words words</p>', '#host{text-transform:uppercase}');
    const parts = await fragments();
    assert.ok(parts.every((p) => !p.missing));
    assert.match(parts.map((p) => p.text).join(''), /^SSTRASSE/);
  });

  it('preserves whitespace at inline style seams exactly once', async () => {
    await load('<p id="host">one <em>two</em> three</p>', '#host{text-align:justify}');
    const snapshot = await page.evaluate(takeSnapshot);
    await page.setContent(snapshot.html);
    const text = await page.$eval('body', (body) => body.textContent.replace(/\u00a0/g, ' ').trim());
    assert.equal(text, 'one two three');
  });

  it('keeps explicit breaks and visible soft hyphens on the DOM fallback', async () => {
    await load('<p id="host">Compre\u00adhensive overview\nNext line</p>',
      '#host{width:95px;white-space:pre-wrap}', false);
    const parts = await fragments(false);
    assert.equal(parts.map((p) => p.text).join('').replace(/[-\s]/g, ''),
      'ComprehensiveoverviewNextline');
    assert.ok(parts.some((p) => p.text.endsWith('-')));
    assert.ok(parts.every((p) => !p.text.includes('\n') && !p.text.includes('\u00ad')));
  });
});
