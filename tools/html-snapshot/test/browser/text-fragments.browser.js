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
            positions:measureTextInlinePositions(node,item,style,item.text,axis !== 'y'),
          });
        }
      }
      return result;
    })()`);
  }

  const prose = '当我们在屏幕上读到一个汉字时，很少会去想它从何而来。' +
    '它看起来理所当然：横平竖直，大小一致，排列整齐。'.repeat(2);
  const cap = 'p::first-letter{float:left;font:72px/.9 serif;color:rgb(181,69,42);margin:8px 12px 0 0}';

  async function capturedParagraph() {
    const snapshot = await page.evaluate(takeSnapshot);
    return page.evaluate(html => {
      const doc = new DOMParser().parseFromString(html, 'text/html');
      const host = doc.querySelector('[data-pagx-paragraph]');
      if (!host) return null;
      return [...host.children].map(run => ({
        text: run.getAttribute('data-pagx-run-text'),
        fragments: [...run.children].map(span => ({
          text: span.textContent,
          start: Number(span.getAttribute('data-pagx-start')),
          baseline: Number(span.getAttribute('data-pagx-baseline')),
          positions: span.getAttribute('data-pagx-inline-positions').split(',').map(Number),
        })),
      }));
    }, snapshot.html);
  }

  it('retains one logical justified paragraph and scalar ranges for all wrapped lines', async () => {
    await load(`<p id="host">${prose}</p>`, '#host{text-align:justify}');
    const runs = await capturedParagraph();
    assert.ok(runs);
    assert.equal(runs.length, 1);
    assert.equal(runs[0].text, prose);
    assert.ok(runs[0].fragments.length > 2);
    for (const part of runs[0].fragments) {
      assert.equal(Array.from(prose).slice(part.start, part.start + part.positions.length).join(''), part.text);
      assert.ok(Number.isFinite(part.baseline));
    }
  });

  for (const [name, content, css, expected] of [
    ['collapsed spaces', '  one   two three four five six seven eight nine ten eleven twelve  ', '',
      'one two three four five six seven eight nine ten eleven twelve'],
    ['hard breaks', 'first line<br>second line<br>third line', '', 'first line\nsecond line\nthird line'],
    ['preserved spaces', ' A  B\n C  D', '#host{white-space:pre-wrap}', ' A  B\n C  D'],
    ['Unicode scalars', 'Á 😀 中文 ❤️ office '.repeat(6), '', 'Á 😀 中文 ❤️ office '.repeat(6).trim()],
    ['rich text', 'normal <b>bold words</b> <em>italic words</em> and more words '.repeat(3), '',
      'normal bold words italic words and more words '.repeat(3).trim()],
  ]) {
    it(`keeps the semantic source for ${name}`, async () => {
      await load(`<p id="host">${content}</p>`, css);
      const runs = await capturedParagraph();
      assert.ok(runs, name);
      assert.equal(runs.map(r => r.text).join(''), expected);
      for (const run of runs) for (const part of run.fragments) {
        assert.equal(Array.from(run.text).slice(part.start, part.start + part.positions.length).join(''), part.text);
      }
    });
  }

  for (const [name, content, css] of [
    ['drop cap', prose, cap],
    ['columns', prose, '#host{columns:2;width:400px}'],
    ['RTL', 'שלום עולם '.repeat(20), '#host{direction:rtl}'],
    ['vertical', prose, '#host{writing-mode:vertical-rl;height:200px}'],
    ['inline margins', `start <span>${prose}</span>`, 'span{margin-left:10px}'],
    ['inline effects', `start <span>${prose}</span>`, 'span{filter:blur(1px)}'],
    ['first-line style', prose, '#host::first-line{color:red}'],
    ['float', `<span style="float:left;width:50px;height:60px">X</span>${prose}`, ''],
  ]) {
    it(`retains visual fallback for unsupported ${name}`, async () => {
      await load(`<p id="host">${content}</p>`, css);
      assert.equal(await capturedParagraph(), null);
      const snapshot = await page.evaluate(takeSnapshot);
      assert.ok(snapshot.html.includes('<span'));
    });
  }
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

  it('measures justification as scalar positions, including the final line', async () => {
    await load(`<p id="host">${prose}</p>`, '#host{text-align:justify}');
    const parts = await fragments();
    for (const part of parts) {
      assert.equal(part.positions.length, Array.from(part.text).length);
      assert.equal(part.positions[0], 0);
    }
    assert.ok(parts[0].positions[1] > 18);
    assert.ok(parts.at(-1).width < 240);
    assert.equal(parts.at(-1).positions[1], 18);
    const snapshot = await page.evaluate(takeSnapshot);
    assert.match(snapshot.html, /data-pagx-inline-positions=/);
  });

  for (const display of ['block', 'flex', 'grid'])
  it(`measures wrapped baselines in ${display} without changing text or geometry`, async () => {
    await load('<p id="host">😀 😂 🥹 😍 🤔 🫠 😎 🥳 😭 🤯 👍 🙏 👏 💪 🔥 ✨ 🎉 ❤️ 💜 🌈 ☀️ 🌙 ⭐</p>',
      `#host{position:absolute;left:15.953125px;top:15.953125px;display:${display};font:44px/52.8px system-ui;width:330px}`);
    const result = await page.evaluate(`(() => {
      ${PAYLOAD_CONSTANTS_SRC}
      ${HELPERS_SRC}
      const host = document.querySelector('#host'), node = host.firstChild;
      const source = node.nodeValue, style = getComputedStyle(host);
      const lines = measuredTextFragments(node, style, 'y');
      const baselines = lines.map(line => measureTextBaseline(node, line));
      const after = measuredTextFragments(node, style, 'y');
      return { baselines, sameNode:host.firstChild === node, text:node.nodeValue, source,
        before:lines.map(x => x.rect.toJSON()), after:after.map(x => x.rect.toJSON()) };
    })()`);
    assert.ok(result.baselines.length > 2);
    assert.equal(result.sameNode, true);
    assert.equal(result.text, result.source);
    assert.deepEqual(result.before, result.after);
    for (let i = 0; i < result.baselines.length; ++i) {
      assert.notEqual(result.baselines[i], null);
      if (i) assert.ok(Math.abs(result.baselines[i] - result.baselines[i - 1] - 52.8) < 0.1);
    }
    const snapshot = await page.evaluate(takeSnapshot);
    assert.match(snapshot.html, /data-pagx-baseline=/);
    assert.match(snapshot.html, /top: 15\.953125px/);
  });

  it('retains the origin of children when asymmetric borders become overlays', async () => {
    await load('<div id="host"><span>竖排</span></div>',
      '#host{position:absolute;left:20px;top:30px;border-top:7px solid black;padding:10px;writing-mode:vertical-rl}');
    const expected = await page.$eval('#host span', node => node.getBoundingClientRect().top);
    const snapshot = await page.evaluate(takeSnapshot);
    await page.setContent(snapshot.html);
    await page.addStyleTag({ content: 'html{padding:0!important;display:block!important}' });
    const actual = await page.$$eval('span', nodes => {
      const node = nodes.find(n => n.textContent === '竖排');
      return node.getBoundingClientRect().top;
    });
    assert.ok(Math.abs(actual - expected) < 0.1, `${actual} vs ${expected}`);
  });

  for (const orientation of ['mixed', 'upright', 'sideways']) {
    it(`captures vertical ${orientation} with measured anchors and column alignment`, async () => {
      await load('<p id="host">竖排English 2026「引号」、句号。</p>',
        `#host{writing-mode:vertical-rl;text-orientation:${orientation};height:190px;text-align:center}`);
      const snapshot = await page.evaluate(takeSnapshot);
      await page.setContent(snapshot.html);
      const spans = await page.$$eval('span[data-pagx-inline-positions]', nodes => nodes.map(node => ({
        text: node.textContent, positions: node.getAttribute('data-pagx-inline-positions').split(',').map(Number),
        orientation: getComputedStyle(node).textOrientation, align: node.style.textAlign,
      })));
      assert.ok(spans.length > 0);
      for (const span of spans) {
        assert.equal(span.orientation, orientation);
        assert.equal(span.positions.length, Array.from(span.text).length);
        assert.equal(span.positions[0], 0);
        assert.equal(span.align, 'start');
      }
    });
  }

  it('binds CSS families to resource candidates independently of font-internal names', async () => {
    await load('<p id="host">中文 Latin</p>', '#host{font-family:Alias,Backup,sans-serif}');
    await page.evaluate(() => {
      window.__pagxFontAliases = [
        {from:'Alias', path:'/fonts/cjk.ttf', weight:'400', style:'normal', unicodeRange:'U+4E00-9FFF'},
        {from:'Alias', path:'/fonts/latin.ttf', weight:'400', style:'normal', unicodeRange:'U+0-7F'},
        {from:'Backup', path:'/fonts/backup.ttf', weight:'400', style:'normal'},
      ];
    });
    const snapshot = await page.evaluate(takeSnapshot);
    const bindings = await page.evaluate(() => window.__pagxFontBindings);
    const binding = bindings.find(b => b.faces.some(f => f.path === '/fonts/cjk.ttf'));
    assert.ok(binding);
    assert.deepEqual(binding.faces.map(f => f.path || f.systemFamily),
      ['/fonts/latin.ttf', '/fonts/cjk.ttf', '/fonts/backup.ttf', 'sans-serif']);
    assert.ok(snapshot.html.includes(binding.family));
  });

  it('keeps RTL scalar positions in logical text order', async () => {
    await load('<p id="host">שלום עולם שלום עולם שלום עולם</p>', '#host{direction:rtl;text-align:justify}');
    const parts = await fragments();
    assert.ok(parts.every((p) => !p.missing));
    assert.ok(parts[0].positions[0] > parts[0].positions[1]);
    assert.equal(parts.map((p) => p.text).join('').replace(/\s/g, ''),
      'שלום עולם שלום עולם שלום עולם'.replace(/\s/g, ''));
  });

  it('maps text-transform expansions back to the source range', async () => {
    await load('<p id="host">ßtraße words words words words</p>', '#host{text-transform:uppercase}');
    const parts = await fragments();
    assert.ok(parts.every((p) => !p.missing));
    assert.match(parts.map((p) => p.text).join(''), /^SSTRASSE/);
    assert.deepEqual(parts[0].positions, []);
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
