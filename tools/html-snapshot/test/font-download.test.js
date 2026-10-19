'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  dataUrlBytes,
  toSfnt,
  readFontMeta,
  makeFontCaptureListener,
  resolveFontFamilies,
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
    const woff2 = findWoff2Fixture();
    if (!woff2) {
      // No fixture available in this checkout — skip rather than fail.
      return;
    }
    const out = await toSfnt(fs.readFileSync(woff2));
    expect(Buffer.isBuffer(out)).toBe(true);
    expect(out.length).toBeGreaterThan(0);
    // Decompressed output must be a raw SFNT, never a WOFF2 container.
    const sig = out.slice(0, 4).toString('binary');
    expect(sig).not.toBe('wOF2');
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

  test('attaches the CSS faces and merges them across duplicate payloads', async () => {
    const { woff2Fixture } = require('./font-fixtures');
    const bytes = await woff2Fixture('Shared Family');
    const map = new Map([
      ['https://x/a.woff2', bytes],
      // Identical bytes served from a second URL under another family.
      ['https://x/b.woff2', Buffer.from(bytes)],
    ]);
    const facesByKey = new Map([
      ['https://x/a.woff2', [{ family: 'Family One', style: '', weight: '', url: 'https://x/a.woff2' }]],
      ['https://x/b.woff2', [{ family: 'Family Two', style: 'italic', weight: '700', url: 'https://x/b.woff2' }]],
    ]);

    const saved = await saveDownloadedFonts(map, outDir, undefined, facesByKey);

    expect(saved).toHaveLength(1);
    expect(saved[0].cssFamilies).toEqual(['Family One', 'Family Two']);
    expect(saved[0].cssFaces).toEqual([
      { family: 'Family One', style: '', weight: '', url: 'https://x/a.woff2' },
      { family: 'Family Two', style: 'italic', weight: '700', url: 'https://x/b.woff2' },
    ]);
  });

  test('writes every WOFF2 of a multi-font page intact', async () => {
    // Regression: the decode pass used to run under one `Promise.all`, and
    // wawoff2 resolves with a view into its shared WASM heap — every font but
    // the last was written as corrupted bytes (unparseable name table, garbage
    // signature) and later failed to register as a fallback face.
    const { woff2Fixture } = require('./font-fixtures');
    const families = ['Alpha Family', 'Beta Family', 'Gamma Family'];
    const map = new Map();
    for (const family of families) {
      map.set(`https://x/${family.replace(/ /g, '-')}.woff2`, await woff2Fixture(family));
    }

    const saved = await saveDownloadedFonts(map, outDir);

    expect(saved).toHaveLength(families.length);
    const onDisk = saved.map((entry) => readFontMeta(fs.readFileSync(entry.path)).family).sort();
    expect(onDisk).toEqual([...families].sort());
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

// Locate any .woff2 web font already present in the checkout so the WOFF2
// decode path can be exercised when fixtures exist, without bloating the
// repo with a dedicated test asset.
function findWoff2Fixture() {
  const roots = [path.join(__dirname, '..', 'eval', 'out'), path.join(__dirname, '..', 'bench', 'out')];
  for (const root of roots) {
    const hit = walkForExt(root, '.woff2');
    if (hit) return hit;
  }
  return null;
}

function walkForExt(dir, ext, depth = 0) {
  if (depth > 6 || !fs.existsSync(dir)) return null;
  let entries;
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return null;
  }
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isFile() && entry.name.toLowerCase().endsWith(ext)) return full;
    if (entry.isDirectory()) {
      const hit = walkForExt(full, ext, depth + 1);
      if (hit) return hit;
    }
  }
  return null;
}

describe('dataUrlBytes', () => {
  test('decodes a base64 payload', () => {
    const bytes = dataUrlBytes('data:font/woff2;base64,' + Buffer.from('hello').toString('base64'));
    expect(bytes.toString()).toBe('hello');
  });

  test('decodes a percent-encoded payload', () => {
    expect(dataUrlBytes('data:font/woff2,%77%6f%66%66').toString()).toBe('woff');
  });

  test('returns null for non-data URLs and malformed payloads', () => {
    expect(dataUrlBytes('https://x/a.woff2')).toBeNull();
    expect(dataUrlBytes('data:font/woff2;base64')).toBeNull();
    // An incomplete percent escape makes decodeURIComponent throw.
    expect(dataUrlBytes('data:font/woff2,%E4%B8')).toBeNull();
  });
});

describe('resolveFontFamilies', () => {
  // Project the returned face map down to family names. These tests pin the
  // family grouping; the face-level detail (style/weight) has its own test
  // below.
  function familyNames(map) {
    return Array.from(map.values()).map((faces) => faces.map((face) => face.family));
  }

  test('registers an inline font and keeps the CSS family it was declared under', async () => {
    const { woff2DataUrl } = require('./font-fixtures');
    const cache = new Map();

    const families = await resolveFontFamilies(
      [{ family: 'MaShanZheng', style: 'normal', weight: '400', url: await woff2DataUrl('Ma Shan Zheng') }],
      cache,
    );

    // The family is the authored one, not the one inside the file — that mismatch is exactly why
    // the caller forwards the pair to `pagx embed/render --font-alias`.
    expect(familyNames(families)).toEqual([['MaShanZheng']]);
    expect(cache.size).toBe(1);
    const cached = Array.from(cache.values())[0];
    // The cache is normalised to SFNT so the save step skips a second decode.
    expect(cached.slice(0, 4).toString('binary')).not.toBe('wOF2');
    expect(readFontMeta(cached).family).toBe('Ma Shan Zheng');
  });

  test('maps an already-captured network font by URL', async () => {
    const { woff2Fixture } = require('./font-fixtures');
    const cache = new Map([['https://x/xiaowei.woff2', await woff2Fixture('ZCOOL XiaoWei')]]);

    const families = await resolveFontFamilies(
      [{ family: 'XiaoWei', style: 'normal', weight: '400', url: 'https://x/xiaowei.woff2' }],
      cache,
    );

    expect(familyNames(families)).toEqual([['XiaoWei']]);
  });

  test('groups several CSS families serving one payload under a single entry', async () => {
    const { woff2DataUrl } = require('./font-fixtures');
    const url = await woff2DataUrl('Shared Family');

    const families = await resolveFontFamilies(
      [
        { family: 'AliasA', style: 'normal', weight: '400', url },
        { family: 'AliasB', style: 'normal', weight: '400', url },
        { family: 'AliasA', style: 'normal', weight: '400', url },
      ],
      new Map(),
    );

    expect(familyNames(families)).toEqual([['AliasA', 'AliasB']]);
  });

  test('keeps one entry per authored face so roman and italic cuts stay distinct', async () => {
    const { woff2DataUrl } = require('./font-fixtures');
    const roman = await woff2DataUrl('Roman Cut');
    const italic = await woff2DataUrl('Italic Cut');
    const family = 'Playfair';
    const faces = await resolveFontFamilies(
      [
        { family, style: 'normal', weight: '400 900', url: roman },
        { family, style: 'italic', weight: '400 900', url: italic },
        // The same face declared twice must not duplicate the entry.
        { family, style: 'italic', weight: '400 900', url: italic },
      ],
      new Map(),
    );
    const entries = Array.from(faces.values());
    expect(entries).toHaveLength(2);
    expect(entries[0]).toEqual([{ family, style: 'normal', weight: '400 900', url: roman }]);
    expect(entries[1]).toEqual([{ family, style: 'italic', weight: '400 900', url: italic }]);
  });

  test('skips a family whose file was neither inlined nor captured', async () => {
    const families = await resolveFontFamilies(
      [{ family: 'Absent', style: 'normal', weight: '400', url: 'https://x/absent.woff2' }],
      new Map(),
    );
    expect(families.size).toBe(0);
  });

  test('logs and continues when a payload cannot be decoded', async () => {
    const { woff2DataUrl } = require('./font-fixtures');
    const logs = [];

    const families = await resolveFontFamilies(
      [
        { family: 'Broken', style: 'normal', weight: '400', url: 'data:font/woff2;base64,d09GMgABAAAAAA==' },
        { family: 'Good', style: 'normal', weight: '400', url: await woff2DataUrl('Good Family') },
      ],
      new Map(),
      (message) => logs.push(message),
    );

    expect(familyNames(families)).toEqual([['Good']]);
    expect(logs.join('\n')).toMatch(/font decode skipped for 'Broken'/);
  });
});
