'use strict';

// Exercise the node-side font decode + glyph-resolution paths that need the
// `font-codec` collaborator. We mock `loadFontCodec` so the WOFF2 branch and
// the end-to-end resolver flow run without shipping a real binary font: the
// stub `opentype.parse` returns a hand-built glyph-bearing font and the stub
// `wawoff2.decompress` records that it was asked to inflate a WOFF2 container.

const SFNT_MARKER = Buffer.from('SFNT');

// Build a minimal opentype.js-shaped font whose `charToGlyph` yields a real
// path for `0xe900` and throws for `0xe901` (to drive the resolver's per-glyph
// catch path).
function stubFont() {
  return {
    unitsPerEm: 1000,
    ascender: 800,
    charToGlyph: (ch) => {
      if (ch === String.fromCodePoint(0xe901)) throw new Error('bad outline table');
      return { index: 1, getPath: () => ({ toPathData: () => 'M0 0 L10 10 Z' }) };
    },
  };
}

function loadIconFontWithCodec({ parse, decompress } = {}) {
  jest.resetModules();
  jest.doMock('../dist/lib/font-codec', () => ({
    loadFontCodec: () => ({
      opentype: { parse: parse || (() => stubFont()) },
      wawoff2: { decompress: decompress || (async (b) => Buffer.concat([SFNT_MARKER, b.subarray(4)])) },
    }),
  }));
  return require('../dist/lib/icon-font');
}

afterEach(() => {
  jest.dontMock('../dist/lib/font-codec');
  jest.resetModules();
});

describe('parseFontBuffer — codec routing', () => {
  test('decompresses a WOFF2 container before parsing', async () => {
    let parsedBytes = null;
    const { parseFontBuffer } = loadIconFontWithCodec({
      parse: (ab) => { parsedBytes = Buffer.from(ab); return stubFont(); },
    });
    // wOF2 magic = 0x77 0x4F 0x46 0x32.
    const woff2 = Buffer.from([0x77, 0x4f, 0x46, 0x32, 0xde, 0xad, 0xbe, 0xef]);
    const font = await parseFontBuffer(woff2);
    expect(font.unitsPerEm).toBe(1000);
    // The bytes handed to opentype.parse were the decompressor's SFNT output,
    // never the raw WOFF2 container.
    expect(parsedBytes.subarray(0, 4).toString()).toBe('SFNT');
  });

  test('feeds a non-WOFF2 buffer straight to opentype.parse', async () => {
    let decompressCalls = 0;
    const { parseFontBuffer } = loadIconFontWithCodec({
      decompress: async (b) => { decompressCalls++; return b; },
    });
    const ttf = Buffer.from([0x00, 0x01, 0x00, 0x00, 1, 2, 3, 4]);
    await parseFontBuffer(ttf);
    expect(decompressCalls).toBe(0);
  });

  test('handles a Uint8Array (non-Buffer) decompressor result', async () => {
    const { parseFontBuffer } = loadIconFontWithCodec({
      decompress: async () => new Uint8Array([0x00, 0x01, 0x00, 0x00]),
    });
    const woff2 = Buffer.from([0x77, 0x4f, 0x46, 0x32, 9, 9, 9, 9]);
    const font = await parseFontBuffer(woff2);
    expect(font.unitsPerEm).toBe(1000);
  });
});

describe('resolveIconFontSvgs — full success path', () => {
  let realFetch;
  beforeEach(() => { realFetch = globalThis.fetch; });
  afterEach(() => { globalThis.fetch = realFetch; });

  test('fetches, parses, and emits one SVG per resolvable target', async () => {
    const { resolveIconFontSvgs } = loadIconFontWithCodec();
    globalThis.fetch = async () => ({
      ok: true,
      status: 200,
      arrayBuffer: async () => new Uint8Array([0x00, 0x01, 0x00, 0x00]).buffer,
    });
    const out = await resolveIconFontSvgs(
      [{ id: 'a', codepoint: 0xe900, fontUrl: 'https://cdn/x.ttf', fontFamily: 'X', format: 'ttf', pseudo: '::before' }],
      () => {},
    );
    expect(out).toHaveLength(1);
    expect(out[0].id).toBe('a');
    expect(out[0].svg).toContain('<svg');
    expect(out[0].svg).toContain('fill="currentColor"');
  });

  test('fetches each unique URL once and reuses the cached font', async () => {
    const { resolveIconFontSvgs } = loadIconFontWithCodec();
    let fetchCalls = 0;
    globalThis.fetch = async () => {
      fetchCalls++;
      return { ok: true, status: 200, arrayBuffer: async () => new Uint8Array([0, 1, 0, 0]).buffer };
    };
    const url = 'https://cdn/shared.ttf';
    const out = await resolveIconFontSvgs(
      [
        { id: 'a', codepoint: 0xe900, fontUrl: url, fontFamily: 'X', format: 'ttf', pseudo: '::before' },
        { id: 'b', codepoint: 0xe900, fontUrl: url, fontFamily: 'X', format: 'ttf', pseudo: '::before' },
      ],
      () => {},
    );
    expect(out).toHaveLength(2);
    expect(fetchCalls).toBe(1);
  });

  test('logs and skips a target whose glyph extraction throws, keeping the rest', async () => {
    const { resolveIconFontSvgs } = loadIconFontWithCodec();
    const logs = [];
    globalThis.fetch = async () => ({
      ok: true,
      status: 200,
      arrayBuffer: async () => new Uint8Array([0, 1, 0, 0]).buffer,
    });
    const url = 'https://cdn/x.ttf';
    const out = await resolveIconFontSvgs(
      [
        { id: 'ok', codepoint: 0xe900, fontUrl: url, fontFamily: 'X', format: 'ttf', pseudo: '::before' },
        { id: 'boom', codepoint: 0xe901, fontUrl: url, fontFamily: 'X', format: 'ttf', pseudo: '::before' },
      ],
      (m) => logs.push(m),
    );
    expect(out).toEqual([{ id: 'ok', svg: expect.stringContaining('<svg') }]);
    expect(logs.join('\n')).toMatch(/failed to extract glyph U\+E901/);
  });

  test('skips a target whose glyph resolves to .notdef (null SVG)', async () => {
    const { resolveIconFontSvgs } = loadIconFontWithCodec({
      parse: () => ({ unitsPerEm: 1000, ascender: 800, charToGlyph: () => ({ index: 0 }) }),
    });
    globalThis.fetch = async () => ({
      ok: true,
      status: 200,
      arrayBuffer: async () => new Uint8Array([0, 1, 0, 0]).buffer,
    });
    const out = await resolveIconFontSvgs(
      [{ id: 'a', codepoint: 0xe900, fontUrl: 'https://cdn/x.ttf', fontFamily: 'X', format: 'ttf', pseudo: '::before' }],
      () => {},
    );
    expect(out).toEqual([]);
  });
});
