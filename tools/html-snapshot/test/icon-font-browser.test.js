'use strict';

// Browser-side icon-font helpers (collectFontFaceMap, browserCollectIconFontTargets,
// browserApplyIconFontSvgs) normally run inside `page.evaluate` against a real
// DOM. They are plain module-scope functions, though, so we can drive them in
// node by standing up minimal `document` / `getComputedStyle` / `window`
// stubs. `isIconScanSkippedTag` resolves the free `DROP_TAGS_UPPER` global the
// IIFE wrapper would normally inject, so we mirror that here.

const { DROP_TAG_NAMES } = require('../dist/lib/dom-tags');
const {
  browserCollectFontFaceMap,
  browserCollectIconFontTargets,
  browserApplyIconFontSvgs,
} = require('../dist/lib/icon-font');

// ---- Fake-DOM builders -----------------------------------------------------

// A CSSStyleSheet-shaped stub. `cssRules` is a getter so a test can make it
// throw (the cross-origin SecurityError path) without a real browser.
function makeSheet({ href = null, rules = null, throwOnRules = false } = {}) {
  return {
    href,
    get cssRules() {
      if (throwOnRules) throw new Error('SecurityError: cssRules denied');
      return rules;
    },
  };
}

function fontFaceRule(family, src) {
  return {
    type: 5,
    style: {
      getPropertyValue: (k) =>
        k === 'font-family' ? family : k === 'src' ? src : '',
    },
  };
}

function nonFontRule(type = 1) {
  return { type, style: { getPropertyValue: () => '' } };
}

// Build an element stub mirroring the slice of the DOM API the walker touches.
// `before` / `after` are `{ content, family }` specs for the matching pseudo.
function makeEl({ tag = 'I', childCount = 0, text = [], before = null, after = null, attrs = {} } = {}) {
  const a = { ...attrs };
  const childNodes = text.map((t) => ({ nodeType: 3, nodeValue: t }));
  const csFor = (spec) => ({
    getPropertyValue: (k) =>
      k === 'content' ? (spec && spec.content) || '' : k === 'font-family' ? (spec && spec.family) || '' : '',
  });
  return {
    tagName: tag,
    children: { length: childCount },
    childNodes,
    setAttribute: (k, v) => { a[k] = v; },
    removeAttribute: (k) => { delete a[k]; },
    getAttribute: (k) => (k in a ? a[k] : null),
    _attrs: a,
    __cs: { '::before': csFor(before), '::after': csFor(after) },
  };
}

let savedGlobals;

beforeEach(() => {
  savedGlobals = {
    document: global.document,
    window: global.window,
    getComputedStyle: global.getComputedStyle,
    fetch: global.fetch,
    DROP_TAGS_UPPER: global.DROP_TAGS_UPPER,
  };
  global.DROP_TAGS_UPPER = new Set(DROP_TAG_NAMES.map((s) => s.toUpperCase()));
  global.getComputedStyle = (el, pseudo) =>
    (el && el.__cs && el.__cs[pseudo]) || { getPropertyValue: () => '' };
});

afterEach(() => {
  global.document = savedGlobals.document;
  global.window = savedGlobals.window;
  global.getComputedStyle = savedGlobals.getComputedStyle;
  global.fetch = savedGlobals.fetch;
  global.DROP_TAGS_UPPER = savedGlobals.DROP_TAGS_UPPER;
});

describe('browserCollectFontFaceMap', () => {
  test('returns {} when the document exposes no stylesheets', async () => {
    global.document = { baseURI: 'https://x/', styleSheets: null };
    expect(await browserCollectFontFaceMap()).toEqual({});
  });

  test('reads @font-face rules from an accessible sheet, first-write wins', async () => {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [
        makeSheet({
          href: 'https://cdn.example/css/sheet.css',
          rules: [
            nonFontRule(1), // CSSStyleRule — ignored
            fontFaceRule('"Phosphor"', "url('p.woff2') format('woff2')"),
            fontFaceRule('NoSrc', ''), // missing src — skipped
            // Duplicate family: the first source must win.
            fontFaceRule('Phosphor', "url('p-bold.woff2') format('woff2')"),
          ],
        }),
      ],
    };
    const map = await browserCollectFontFaceMap();
    expect(Object.keys(map)).toEqual(['Phosphor']);
    expect(map.Phosphor.url).toBe('https://cdn.example/css/p.woff2');
    expect(map.Phosphor.format).toBe('woff2');
  });

  test('skips a sheet whose cssRules is null', async () => {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [makeSheet({ href: 'https://x/empty.css', rules: null })],
    };
    expect(await browserCollectFontFaceMap()).toEqual({});
  });

  test('falls back to fetching cross-origin sheets that deny cssRules', async () => {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [
        makeSheet({ href: 'https://cdn.example/cors.css', throwOnRules: true }),
        // A blocked sheet without an href contributes nothing (no URL to fetch).
        makeSheet({ href: null, throwOnRules: true }),
      ],
    };
    global.fetch = async (href) => ({
      ok: true,
      text: async () =>
        `@font-face { font-family: "Remix"; src: url(remix.woff2) format("woff2"); }`,
    });
    const map = await browserCollectFontFaceMap();
    expect(map.Remix.url).toBe('https://cdn.example/remix.woff2');
  });

  test('ignores a cross-origin fallback that returns non-OK', async () => {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [makeSheet({ href: 'https://cdn.example/cors.css', throwOnRules: true })],
    };
    global.fetch = async () => ({ ok: false, status: 403, text: async () => '' });
    expect(await browserCollectFontFaceMap()).toEqual({});
  });

  test('swallows a fetch rejection on the cross-origin fallback', async () => {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [makeSheet({ href: 'https://cdn.example/cors.css', throwOnRules: true })],
    };
    global.fetch = async () => { throw new Error('network down'); };
    expect(await browserCollectFontFaceMap()).toEqual({});
  });
});

describe('browserCollectIconFontTargets', () => {
  function withFontMap(elements) {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [
        makeSheet({
          href: 'https://cdn.example/icons.css',
          rules: [fontFaceRule('"Phosphor"', "url('p.woff2') format('woff2')")],
        }),
      ],
      body: { querySelectorAll: () => elements },
    };
  }

  test('returns [] when there is no document body', async () => {
    global.document = {
      baseURI: 'https://x/',
      styleSheets: [],
      body: null,
    };
    expect(await browserCollectIconFontTargets()).toEqual([]);
  });

  test('tags a leaf whose ::before is a single PUA glyph backed by a webfont', async () => {
    const hit = makeEl({ tag: 'I', before: { content: '"\ue900"', family: '"Phosphor", sans-serif' } });
    withFontMap([hit]);
    const targets = await browserCollectIconFontTargets();
    expect(targets).toHaveLength(1);
    expect(targets[0]).toMatchObject({
      fontFamily: 'Phosphor',
      codepoint: 0xe900,
      fontUrl: 'https://cdn.example/p.woff2',
      format: 'woff2',
      pseudo: '::before',
    });
    // The host was tagged so the node-side resolver can route the SVG back.
    expect(hit.getAttribute('data-icon-target-id')).toBe(targets[0].id);
  });

  test('falls through ::before none to a matching ::after glyph', async () => {
    const hit = makeEl({
      tag: 'SPAN',
      before: { content: 'none' },
      after: { content: '"\ue901"', family: 'Phosphor' },
    });
    withFontMap([hit]);
    const targets = await browserCollectIconFontTargets();
    expect(targets).toHaveLength(1);
    expect(targets[0].pseudo).toBe('::after');
    expect(targets[0].codepoint).toBe(0xe901);
  });

  test('rejects non-candidates: children, dropped tags, text, multi-char, non-PUA, unknown family', async () => {
    const elements = [
      makeEl({ tag: 'DIV', childCount: 2, before: { content: '"\ue900"', family: 'Phosphor' } }),
      makeEl({ tag: 'SCRIPT', before: { content: '"\ue900"', family: 'Phosphor' } }),
      makeEl({ tag: 'I', text: ['real text'], before: { content: '"\ue900"', family: 'Phosphor' } }),
      makeEl({ tag: 'I', before: { content: 'normal' } }),
      makeEl({ tag: 'I', before: { content: '"ab"', family: 'Phosphor' } }), // 2 chars
      makeEl({ tag: 'I', before: { content: '"A"', family: 'Phosphor' } }), // non-PUA
      makeEl({ tag: 'I', before: { content: '"\ue900"', family: 'UnknownFont' } }), // family not in map
    ];
    withFontMap(elements);
    expect(await browserCollectIconFontTargets()).toEqual([]);
  });

  test('assigns unique ids across multiple matched hosts', async () => {
    const a = makeEl({ tag: 'I', before: { content: '"\ue900"', family: 'Phosphor' } });
    const b = makeEl({ tag: 'I', before: { content: '"\ue901"', family: 'Phosphor' } });
    withFontMap([a, b]);
    const targets = await browserCollectIconFontTargets();
    expect(targets).toHaveLength(2);
    expect(targets[0].id).not.toBe(targets[1].id);
  });
});

describe('browserApplyIconFontSvgs', () => {
  test('stores resolved SVGs on window and rewrites the host attributes', () => {
    const host = makeEl({ tag: 'I', attrs: { 'data-icon-target-id': 'id-1' } });
    const straggler = makeEl({ tag: 'I', attrs: { 'data-icon-target-id': 'id-orphan' } });
    const win = {};
    global.window = win;
    global.document = {
      querySelector: (sel) => (sel.includes('id-1') ? host : null),
      querySelectorAll: () => [straggler],
    };
    browserApplyIconFontSvgs([
      null, // skipped
      { id: 'id-1', svg: '<svg/>' },
      { id: 'missing', svg: '<svg/>' }, // querySelector returns null → skipped
      { id: 'no-svg' }, // no svg → skipped
    ]);
    expect(win.__pagxIconSvgs['id-1']).toBe('<svg/>');
    expect(host.getAttribute('data-snapshot-icon-svg-id')).toBe('id-1');
    expect(host.getAttribute('data-icon-target-id')).toBeNull();
    // The orphaned tracking attribute is stripped from leftover hosts.
    expect(straggler.getAttribute('data-icon-target-id')).toBeNull();
  });

  test('tolerates a null results list', () => {
    global.window = {};
    global.document = { querySelector: () => null, querySelectorAll: () => [] };
    expect(() => browserApplyIconFontSvgs(null)).not.toThrow();
  });

  test('reuses an existing window.__pagxIconSvgs dictionary', () => {
    const win = { __pagxIconSvgs: { existing: '<old/>' } };
    const host = makeEl({ tag: 'I', attrs: { 'data-icon-target-id': 'id-2' } });
    global.window = win;
    global.document = {
      querySelector: () => host,
      querySelectorAll: () => [],
    };
    browserApplyIconFontSvgs([{ id: 'id-2', svg: '<svg2/>' }]);
    expect(win.__pagxIconSvgs.existing).toBe('<old/>');
    expect(win.__pagxIconSvgs['id-2']).toBe('<svg2/>');
  });
});
