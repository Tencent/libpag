'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  glyphToSvg,
  formatRank,
  parseSrcList,
  parseFontFaceFromText,
  isPuaCodepoint,
  roundTo,
  fetchFontBytes,
  parseFontBuffer,
  resolveIconFontSvgs,
  inlineIconFontsOnPage,
} = require('../dist/lib/icon-font');

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

describe('fetchFontBytes', () => {
  test('decodes a base64 data: URL', async () => {
    const payload = Buffer.from('hello-font').toString('base64');
    const bytes = await fetchFontBytes('data:font/ttf;base64,' + payload);
    expect(bytes.toString()).toBe('hello-font');
  });

  test('decodes a non-base64 (percent-encoded) data: URL', async () => {
    const bytes = await fetchFontBytes('data:font/ttf,' + encodeURIComponent('abc'));
    expect(bytes.toString()).toBe('abc');
  });

  test('rejects a data: URL without a comma', async () => {
    await expect(fetchFontBytes('data:font/ttf')).rejects.toThrow(/malformed data:/);
  });

  test('reads bytes from a file:// URL', async () => {
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'icon-font-fetch-'));
    try {
      const file = path.join(tmp, 'tiny.ttf');
      fs.writeFileSync(file, Buffer.from([1, 2, 3, 4]));
      const bytes = await fetchFontBytes('file://' + file);
      expect(Array.from(bytes)).toEqual([1, 2, 3, 4]);
    } finally {
      fs.rmSync(tmp, { recursive: true, force: true });
    }
  });

  test('rejects unsupported schemes', async () => {
    await expect(fetchFontBytes('ftp://example.com/x.ttf'))
      .rejects.toThrow(/unsupported scheme/);
    await expect(fetchFontBytes('relative.ttf'))
      .rejects.toThrow(/unsupported scheme/);
  });

  test('reports an HTTP non-2xx as "HTTP <status>"', async () => {
    const realFetch = globalThis.fetch;
    globalThis.fetch = async () => ({ ok: false, status: 404 });
    try {
      await expect(fetchFontBytes('https://example.com/x.ttf'))
        .rejects.toThrow(/HTTP 404/);
    } finally {
      globalThis.fetch = realFetch;
    }
  });

  test('returns the body bytes on a 2xx HTTP response', async () => {
    const realFetch = globalThis.fetch;
    const body = new Uint8Array([0xAA, 0xBB, 0xCC]).buffer;
    globalThis.fetch = async () => ({
      ok: true,
      status: 200,
      arrayBuffer: async () => body,
    });
    try {
      const bytes = await fetchFontBytes('https://cdn.example/x.woff2');
      expect(Array.from(bytes)).toEqual([0xAA, 0xBB, 0xCC]);
    } finally {
      globalThis.fetch = realFetch;
    }
  });
});

describe('parseFontBuffer', () => {
  test('rejects an obviously truncated buffer', async () => {
    // Bytes that don't match WOFF2 magic and aren't a valid SFNT — opentype.parse
    // surfaces a parse error which propagates out of parseFontBuffer.
    await expect(parseFontBuffer(Buffer.from('not-a-real-font'))).rejects.toThrow();
  });
});

describe('resolveIconFontSvgs', () => {
  test('returns [] for a falsy / empty target list', async () => {
    expect(await resolveIconFontSvgs(null, () => {})).toEqual([]);
    expect(await resolveIconFontSvgs([], () => {})).toEqual([]);
  });

  test('logs and skips targets whose font URL fails to load', async () => {
    const logs = [];
    // Unsupported scheme triggers a synchronous-style rejection inside
    // fetchFontBytes; the resolver catches it and logs.
    const out = await resolveIconFontSvgs(
      [{ id: 'a', codepoint: 0xe900, fontUrl: 'gopher://nope', fontFamily: 'X', format: '', pseudo: '::before' }],
      (m) => logs.push(m),
    );
    expect(out).toEqual([]);
    expect(logs.join('\n')).toMatch(/failed to load icon font gopher:/);
  });

  test('skips entries with missing fontUrl without invoking fetch', async () => {
    const realFetch = globalThis.fetch;
    let fetchCalls = 0;
    globalThis.fetch = async () => { fetchCalls++; return { ok: false, status: 0 }; };
    try {
      const out = await resolveIconFontSvgs([
        null,
        { id: 'a', codepoint: 0xe900 }, // no fontUrl
      ], () => {});
      expect(out).toEqual([]);
      expect(fetchCalls).toBe(0);
    } finally {
      globalThis.fetch = realFetch;
    }
  });
});

describe('inlineIconFontsOnPage', () => {
  // Build a stub `page` exposing only the surface inlineIconFontsOnPage uses:
  // page.evaluate(arg) returning the value (or throwing) the test wants.
  function pageStub(returns) {
    const calls = [];
    const stub = async (arg) => {
      calls.push(arg);
      const next = returns.shift();
      if (typeof next === 'function') return next(arg);
      if (next && next.throw) throw next.throw;
      return next;
    };
    return { page: { evaluate: stub }, calls };
  }

  test('returns 0/0 when the collector evaluate throws', async () => {
    const logs = [];
    const { page } = pageStub([
      { throw: new Error('cssRules denied') },
      undefined, // applicator (cleanup)
    ]);
    const out = await inlineIconFontsOnPage(page, { logger: (m) => logs.push(m) });
    expect(out).toEqual({ inlined: 0, total: 0 });
    expect(logs.join('\n')).toMatch(/target collection failed: cssRules denied/);
  });

  test('returns 0/0 silently when no targets are collected', async () => {
    const { page } = pageStub([[], undefined]);
    const out = await inlineIconFontsOnPage(page);
    expect(out).toEqual({ inlined: 0, total: 0 });
  });

  test('logs SVG-application failures without rejecting', async () => {
    const logs = [];
    // 1st evaluate: returns one target whose fontUrl cannot be fetched, so
    // resolveIconFontSvgs returns []. 2nd evaluate (the applicator cleanup)
    // throws, which the function swallows + logs.
    const { page, calls } = pageStub([
      [{ id: 'x', codepoint: 0xe900, fontUrl: 'gopher://no', fontFamily: 'X', format: '', pseudo: '::before' }],
      { throw: new Error('apply boom') },
    ]);
    const out = await inlineIconFontsOnPage(page, { logger: (m) => logs.push(m) });
    expect(out).toEqual({ inlined: 0, total: 1 });
    // The applicator was still invoked — its failure is non-fatal.
    expect(calls).toHaveLength(2);
    expect(logs.some((m) => /SVG application failed: apply boom/.test(m))).toBe(true);
  });

  test('uses a no-op logger when none is provided', async () => {
    const { page } = pageStub([{ throw: new Error('x') }, undefined]);
    // No logger -> inlineIconFontsOnPage must not throw on the rejected evaluate.
    await expect(inlineIconFontsOnPage(page)).resolves.toEqual({ inlined: 0, total: 0 });
  });
});
