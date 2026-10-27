'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  toSfnt,
  readFontMeta,
  makeFontCaptureListener,
  saveDownloadedFonts,
} = require('../dist/lib/font-download');

// A throwaway buffer that is neither WOFF nor WOFF2 (so toSfnt returns it
// unchanged) and is not a parseable SFNT (so readFontMeta falls back to its
// defaults). Lets us exercise the save pipeline without shipping a real font.
function fakeSfnt(tag = 'TEST') {
  return Buffer.concat([Buffer.from('\x00\x01\x00\x00'), Buffer.from(tag)]);
}

describe('toSfnt', () => {
  test('returns a non-WOFF buffer unchanged', async () => {
    const buf = fakeSfnt();
    const out = await toSfnt(buf);
    expect(out).toBe(buf);
  });

  test('decompresses a real WOFF2 web font to SFNT', async () => {
    // Generate a valid, tiny font so this test also runs on a clean checkout.
    const opentype = require('opentype.js');
    const font = new opentype.Font({
      familyName: 'Coverage Sans', styleName: 'Regular', unitsPerEm: 1000,
      ascender: 800, descender: -200,
      glyphs: [new opentype.Glyph({ name: '.notdef', advanceWidth: 500, path: new opentype.Path() })],
    });
    const woff2 = await require('wawoff2').compress(Buffer.from(font.toArrayBuffer()));
    const out = await toSfnt(Buffer.from(woff2));
    expect(Buffer.isBuffer(out)).toBe(true);
    expect(out.length).toBeGreaterThan(0);
    // Decompressed output must be a raw SFNT, never a WOFF2 container.
    const sig = out.slice(0, 4).toString('binary');
    expect(sig).not.toBe('wOF2');
    expect(readFontMeta(out).family).toBe('Coverage Sans');
  });
});

describe('readFontMeta', () => {
  test('falls back to defaults for an unparseable buffer', () => {
    const meta = readFontMeta(fakeSfnt());
    expect(meta).toEqual({ family: 'font', style: 'Regular', ext: 'ttf' });
  });

  test('reports otf for an OTTO (CFF) signature', () => {
    const meta = readFontMeta(Buffer.from('OTTO and some junk'));
    expect(meta.ext).toBe('otf');
  });

  test('handles a sub-4-byte buffer without throwing', () => {
    expect(() => readFontMeta(Buffer.from('ab'))).not.toThrow();
  });
});

describe('makeFontCaptureListener', () => {
  const makeResp = ({ type = 'font', url = 'https://x/font.woff2', ok = true, body = 'BYTES' }) => ({
    request: () => ({ resourceType: () => type, url: () => url }),
    ok: () => ok,
    buffer: async () => Buffer.from(body),
  });

  test('caches a successful font response by URL', async () => {
    const cache = new Map();
    const listener = makeFontCaptureListener('puppeteer', cache);
    await listener(makeResp({ url: 'https://x/a.woff2' }));
    expect(cache.get('https://x/a.woff2').toString()).toBe('BYTES');
  });

  test('ignores non-font resource types', async () => {
    const cache = new Map();
    const listener = makeFontCaptureListener('puppeteer', cache);
    await listener(makeResp({ type: 'image' }));
    expect(cache.size).toBe(0);
  });

  test('ignores data: URLs and non-OK responses', async () => {
    const cache = new Map();
    const listener = makeFontCaptureListener('puppeteer', cache);
    await listener(makeResp({ url: 'data:font/woff2;base64,AAAA' }));
    await listener(makeResp({ url: 'https://x/b.woff2', ok: false }));
    expect(cache.size).toBe(0);
  });

  test('does not re-fetch a URL already in the cache', async () => {
    const cache = new Map();
    cache.set('https://x/c.woff2', Buffer.from('OLD'));
    let bufferCalls = 0;
    const listener = makeFontCaptureListener('puppeteer', cache);
    await listener({
      request: () => ({ resourceType: () => 'font', url: () => 'https://x/c.woff2' }),
      ok: () => true,
      buffer: async () => { bufferCalls++; return Buffer.from('NEW'); },
    });
    expect(bufferCalls).toBe(0);
    expect(cache.get('https://x/c.woff2').toString()).toBe('OLD');
  });

  test('logs and swallows a body-read error', async () => {
    const logs = [];
    const cache = new Map();
    const listener = makeFontCaptureListener('puppeteer', cache, (m) => logs.push(m));
    await listener({
      request: () => ({ resourceType: () => 'font', url: () => 'https://x/d.woff2' }),
      ok: () => true,
      buffer: async () => { throw new Error('detached'); },
    });
    expect(cache.size).toBe(0);
    expect(logs[0]).toMatch(/font capture skipped: detached/);
  });
});

describe('saveDownloadedFonts', () => {
  let outDir;
  beforeEach(() => {
    outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-font-test-'));
  });
  afterEach(() => {
    fs.rmSync(outDir, { recursive: true, force: true });
  });

  test('returns an empty array for an empty / missing map', async () => {
    expect(await saveDownloadedFonts(new Map(), outDir)).toEqual([]);
    expect(await saveDownloadedFonts(null, outDir)).toEqual([]);
  });

  test('writes a content-addressed file and reports it', async () => {
    const map = new Map([['https://x/a.ttf', fakeSfnt('AAAA')]]);
    const saved = await saveDownloadedFonts(map, outDir);
    expect(saved).toHaveLength(1);
    expect(saved[0].url).toBe('https://x/a.ttf');
    expect(fs.existsSync(saved[0].path)).toBe(true);
    expect(path.dirname(saved[0].path)).toBe(outDir);
  });

  test('de-dupes identical bytes served from different URLs', async () => {
    const bytes = fakeSfnt('SAME');
    const map = new Map([
      ['https://x/a.ttf', bytes],
      ['https://y/b.ttf', Buffer.from(bytes)],
    ]);
    const saved = await saveDownloadedFonts(map, outDir);
    expect(saved).toHaveLength(1);
    expect(fs.readdirSync(outDir)).toHaveLength(1);
  });

  test('logs and skips an entry that fails to decode', async () => {
    const logs = [];
    // A buffer whose WOFF2 signature forces a decompress attempt that throws
    // on the truncated body.
    const badWoff2 = Buffer.from([0x77, 0x4f, 0x46, 0x32, 0x00]);
    const map = new Map([['https://x/bad.woff2', badWoff2]]);
    const saved = await saveDownloadedFonts(map, outDir, (m) => logs.push(m));
    expect(saved).toEqual([]);
    expect(logs.join('\n')).toMatch(/failed to save font/);
  });

  test('reuses an existing on-disk file with the same content-addressed name', async () => {
    // Pre-stage a file at the exact content-addressed name we expect, then
    // ensure saveDownloadedFonts skips the rewrite (and still reports it).
    const bytes = fakeSfnt('REUSE');
    const map = new Map([['https://x/a.ttf', bytes]]);
    const first = await saveDownloadedFonts(map, outDir);
    expect(first).toHaveLength(1);
    const stagedPath = first[0].path;
    fs.writeFileSync(stagedPath, 'REPLACED');

    const second = await saveDownloadedFonts(map, outDir);
    expect(second).toHaveLength(1);
    expect(second[0].path).toBe(stagedPath);
    // Existing bytes are preserved — the writeFileAtomic call short-circuits.
    expect(fs.readFileSync(stagedPath, 'utf8')).toBe('REPLACED');
  });

  test('logs and drops faces whose atomic write fails', async () => {
    const logs = [];
    // Make the destination directory read-only so writeFileAtomic's mkdir-
    // sibling-temp + rename sequence fails. The decode pass already created
    // the dir (it succeeds first), so the failure surfaces only on the write.
    fs.mkdirSync(outDir, { recursive: true });
    const bytes = fakeSfnt('LOCKED');
    fs.chmodSync(outDir, 0o500);
    try {
      const map = new Map([['https://x/locked.ttf', bytes]]);
      const saved = await saveDownloadedFonts(map, outDir, (m) => logs.push(m));
      expect(saved).toEqual([]);
      expect(logs.join('\n')).toMatch(/failed to save font https:\/\/x\/locked\.ttf/);
    } finally {
      // Restore permissions so afterEach's rm doesn't leave the tmp behind.
      fs.chmodSync(outDir, 0o700);
    }
  });
});

describe('toSfnt — short / non-magic buffers', () => {
  test('returns a sub-4-byte buffer unchanged (no magic to match)', async () => {
    const tiny = Buffer.from([0x01, 0x02]);
    const out = await toSfnt(tiny);
    expect(out).toBe(tiny);
  });
});

describe('readFontMeta — name-table fallbacks', () => {
  // Drive readFontMeta through a stubbed font-codec so we can hit the platform
  // table → flat-layout → "" name resolution paths without a real font fixture.
  function loadWithCodecStub(parseImpl) {
    jest.resetModules();
    jest.doMock('../dist/lib/font-codec', () => ({
      loadFontCodec: () => ({ opentype: { parse: parseImpl }, wawoff2: { decompress: async (b) => b } }),
    }));
    return require('../dist/lib/font-download');
  }

  afterEach(() => { jest.resetModules(); });

  test('reads names from the windows platform table', () => {
    const { readFontMeta: readFM } = loadWithCodecStub(() => ({
      names: {
        windows: {
          fontFamily: { en: 'Stub Sans' },
          fontSubfamily: { en: 'Bold' },
        },
      },
    }));
    const meta = readFM(Buffer.from([0x01, 0x02, 0x03, 0x04]));
    expect(meta.family).toBe('Stub Sans');
    expect(meta.style).toBe('Bold');
    expect(meta.ext).toBe('ttf');
  });

  test('falls back to the flat-layout names when no platform table has the key', () => {
    const { readFontMeta: readFM } = loadWithCodecStub(() => ({
      names: {
        // No windows/macintosh/unicode tables; the v1 flat layout exposes
        // the records directly under `names`.
        fontFamily: { en: 'Flat Family' },
        fontSubfamily: { en: 'Italic' },
      },
    }));
    const meta = readFM(Buffer.from([0x01, 0x02, 0x03, 0x04]));
    expect(meta.family).toBe('Flat Family');
    expect(meta.style).toBe('Italic');
  });

  test('picks any non-empty locale when "en" is missing', () => {
    const { readFontMeta: readFM } = loadWithCodecStub(() => ({
      names: {
        windows: {
          fontFamily: { fr: 'Famille' },
          // No subfamily anywhere → readFontMeta defaults to 'Regular'.
        },
      },
    }));
    const meta = readFM(Buffer.from([0x01, 0x02, 0x03, 0x04]));
    expect(meta.family).toBe('Famille');
    expect(meta.style).toBe('Regular');
  });

  test('marks CFF (OTF) fonts via the tables flag', () => {
    const { readFontMeta: readFM } = loadWithCodecStub(() => ({
      names: { windows: { fontFamily: { en: 'CFF Sans' } } },
      tables: { cff: {} },
    }));
    const meta = readFM(Buffer.from([0x01, 0x02, 0x03, 0x04]));
    expect(meta.ext).toBe('otf');
  });
});
