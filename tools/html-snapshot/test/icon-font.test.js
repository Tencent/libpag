'use strict';

const {
  glyphToSvg,
  formatRank,
  parseSrcList,
  parseFontFaceFromText,
  isPuaCodepoint,
  roundTo,
} = require('../lib/icon-font');

// Minimal opentype.js-shaped font stub. `glyphToSvg` only touches
// charToGlyph(), unitsPerEm, ascender, and the returned glyph's
// index / getPath().toPathData().
function fontStub({ glyph, unitsPerEm = 1000, ascender = 800 } = {}) {
  return {
    unitsPerEm,
    ascender,
    charToGlyph: () => glyph,
  };
}

function glyphStub({ index = 1, pathData = 'M0 0 L10 10 Z' } = {}) {
  return {
    index,
    getPath: () => ({ toPathData: () => pathData }),
  };
}

describe('glyphToSvg', () => {
  test('emits an SVG with a viewBox and currentColor fill for a mapped glyph', () => {
    const svg = glyphToSvg(fontStub({ glyph: glyphStub() }), 0xe900);
    expect(svg).toContain('viewBox="0 0 1000 1000"');
    expect(svg).toContain('fill="currentColor"');
    expect(svg).toContain('d="M0 0 L10 10 Z"');
    expect(svg.startsWith('<svg')).toBe(true);
  });

  test('returns null when the font has no glyph for the codepoint', () => {
    expect(glyphToSvg(fontStub({ glyph: null }), 0xe900)).toBeNull();
  });

  test('returns null for the .notdef glyph (index 0)', () => {
    expect(glyphToSvg(fontStub({ glyph: glyphStub({ index: 0 }) }), 0xe900)).toBeNull();
  });

  test('returns null when getPath throws', () => {
    const glyph = { index: 1, getPath: () => { throw new Error('bad outline'); } };
    expect(glyphToSvg(fontStub({ glyph }), 0xe900)).toBeNull();
  });

  test('returns null when the path data is empty', () => {
    expect(glyphToSvg(fontStub({ glyph: glyphStub({ pathData: '   ' }) }), 0xe900))
      .toBeNull();
  });

  test('falls back to unitsPerEm for the baseline when ascender is zero', () => {
    const svg = glyphToSvg(fontStub({ glyph: glyphStub(), ascender: 0, unitsPerEm: 2048 }), 0xe900);
    expect(svg).toContain('viewBox="0 0 2048 2048"');
  });

  test('defaults unitsPerEm to 1000 when missing', () => {
    const font = { charToGlyph: () => glyphStub(), ascender: 800 };
    const svg = glyphToSvg(font, 0xe900);
    expect(svg).toContain('viewBox="0 0 1000 1000"');
  });
});

describe('formatRank', () => {
  test('prefers woff2 over woff over sfnt containers', () => {
    expect(formatRank('woff2')).toBeLessThan(formatRank('woff'));
    expect(formatRank('woff')).toBeLessThan(formatRank('ttf'));
    expect(formatRank('ttf')).toBeLessThan(formatRank('otf'));
  });

  test('treats truetype/opentype as aliases', () => {
    expect(formatRank('truetype')).toBe(formatRank('ttf'));
    expect(formatRank('opentype')).toBe(formatRank('otf'));
  });

  test('ranks unknown formats last', () => {
    expect(formatRank('svg')).toBe(5);
    expect(formatRank('')).toBe(5);
  });
});

describe('parseSrcList', () => {
  test('returns null when no url() is present', () => {
    expect(parseSrcList('local("Foo")', 'https://cdn/sheet.css')).toBeNull();
  });

  test('picks the woff2 source and resolves it against the sheet base', () => {
    const src = "url('icons.woff') format('woff'), url('icons.woff2') format('woff2')";
    const out = parseSrcList(src, 'https://cdn.example/css/sheet.css');
    expect(out.format).toBe('woff2');
    expect(out.url).toBe('https://cdn.example/css/icons.woff2');
  });

  test('handles bare (unquoted) urls', () => {
    const out = parseSrcList('url(icons.ttf) format(truetype)', 'https://cdn.example/sheet.css');
    expect(out.format).toBe('truetype');
    expect(out.url).toBe('https://cdn.example/icons.ttf');
  });

  test('keeps an absolute url untouched', () => {
    const out = parseSrcList('url("https://other.cdn/icons.woff2")', 'https://cdn/sheet.css');
    expect(out.url).toBe('https://other.cdn/icons.woff2');
  });
});

describe('parseFontFaceFromText', () => {
  test('returns [] for empty input', () => {
    expect(parseFontFaceFromText('', 'https://cdn/sheet.css')).toEqual([]);
  });

  test('extracts family + best source from a @font-face block', () => {
    const css = `
      @font-face {
        font-family: "Phosphor";
        src: url(phosphor.woff2) format("woff2");
      }`;
    const out = parseFontFaceFromText(css, 'https://cdn.example/sheet.css');
    expect(out).toHaveLength(1);
    expect(out[0].family).toBe('Phosphor');
    expect(out[0].url).toBe('https://cdn.example/phosphor.woff2');
    expect(out[0].format).toBe('woff2');
  });

  test('ignores @font-face inside CSS comments', () => {
    const css = `/* @font-face { font-family: "Decoy"; src: url(x.woff2); } */`;
    expect(parseFontFaceFromText(css, 'https://cdn/sheet.css')).toEqual([]);
  });

  test('skips blocks missing font-family or src', () => {
    const css = `@font-face { font-family: "NoSrc"; }`;
    expect(parseFontFaceFromText(css, 'https://cdn/sheet.css')).toEqual([]);
  });
});

describe('isPuaCodepoint', () => {
  test('accepts the BMP Private Use Area range', () => {
    expect(isPuaCodepoint(0xe000)).toBe(true);
    expect(isPuaCodepoint(0xf8ff)).toBe(true);
    expect(isPuaCodepoint(0xe900)).toBe(true);
  });

  test('rejects codepoints outside the PUA', () => {
    expect(isPuaCodepoint(0x0041)).toBe(false); // 'A'
    expect(isPuaCodepoint(0xdfff)).toBe(false);
    expect(isPuaCodepoint(0xf900)).toBe(false);
  });
});

describe('roundTo', () => {
  test('rounds to the requested precision', () => {
    expect(roundTo(1.23456, 2)).toBe(1.23);
    expect(roundTo(1000.5, 0)).toBe(1001);
  });

  test('normalises -0 to 0', () => {
    expect(Object.is(roundTo(-0.0001, 2), 0)).toBe(true);
  });

  test('returns 0 for non-finite input', () => {
    expect(roundTo(NaN, 2)).toBe(0);
    expect(roundTo(Infinity, 2)).toBe(0);
  });
});
