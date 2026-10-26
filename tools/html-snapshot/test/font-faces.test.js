'use strict';

const { collectWebFontFaces, makeFontCaptureListener } = require('../dist/lib/font-download');

const saved = {};
beforeEach(() => {
  for (const key of ['document', 'CSSFontFaceRule', 'CSSImportRule']) saved[key] = global[key];
  global.CSSFontFaceRule = class {
    constructor(props) { this.style = { getPropertyValue: name => props[name] || '' }; }
  };
  global.CSSImportRule = class { constructor(sheet) { this.styleSheet = sheet; } };
});
afterEach(() => {
  for (const [key, value] of Object.entries(saved)) {
    if (value === undefined) delete global[key];
    else global[key] = value;
  }
});

test('collects nested/imported/adopted font faces once, preserving base URLs and descriptors', () => {
  const face = new CSSFontFaceRule({
    'font-family': 'Body', 'font-weight': '100 900', 'font-style': 'italic',
    'unicode-range': 'U+4E00-9FFF', src: 'url(body.woff2)',
    'font-stretch': 'condensed', 'font-feature-settings': '"kern"',
    'font-variation-settings': '"wght" 500',
  });
  const imported = { href: 'https://example.test/fonts/style.css', cssRules: [face] };
  const root = { href: null, cssRules: [
    { cssRules: [new CSSImportRule(imported), { cssRules: [new CSSFontFaceRule({ 'font-family': 'Inline' })] }] },
    new CSSImportRule(null), {},
  ] };
  imported.cssRules.push(new CSSImportRule(root)); // Cyclic imports must terminate.
  const opaque = { get cssRules() { throw new Error('SecurityError'); } };
  global.document = {
    baseURI: 'https://example.test/page.html',
    styleSheets: [root, opaque], adoptedStyleSheets: [imported, root],
  };
  const result = collectWebFontFaces();
  expect(result).toHaveLength(2);
  expect(result[0]).toEqual({
    'font-family': 'Body', 'font-weight': '100 900', 'font-style': 'italic',
    'unicode-range': 'U+4E00-9FFF', src: 'url(body.woff2)',
    'font-stretch': 'condensed', 'font-feature-settings': '"kern"',
    'font-variation-settings': '"wght" 500', base: imported.href,
  });
  expect(result[1]).toMatchObject({ 'font-family': 'Inline', src: '', base: document.baseURI });
});

test('invalid inline-font encodings are logged and skipped without aborting capture', async () => {
  const cache = new Map();
  const log = jest.fn();
  const response = { request: () => ({ resourceType: () => 'font', url: () => 'data:font/ttf;base64,%ZZ' }) };
  await makeFontCaptureListener('puppeteer', cache, log)(response);
  await expect(makeFontCaptureListener('puppeteer', cache)(response)).resolves.toBeUndefined();
  expect(cache.size).toBe(0);
  expect(log).toHaveBeenCalledWith(expect.stringContaining('font capture skipped:'));
});
