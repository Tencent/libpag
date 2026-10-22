'use strict';

// `loadFontCodec` is a lazy loader: the first call resolves opentype.js and
// wawoff2 (the latter spins up a multi-MB WebAssembly module on import), and
// subsequent calls reuse the cached module references. We exercise the
// caching contract end-to-end so a future refactor cannot silently turn the
// helper into a per-call require.

describe('loadFontCodec', () => {
  let loadFontCodec;

  beforeEach(() => {
    // Drop the require cache for the module under test so each test sees a
    // fresh module-level closure (the lazy globals are initialised to null
    // again). The underlying packages stay cached by Node, so we are not
    // paying the wasm cost more than once.
    jest.resetModules();
    ({ loadFontCodec } = require('../dist/lib/font-codec'));
  });

  test('returns both libraries with the expected shape', () => {
    const { opentype, wawoff2 } = loadFontCodec();
    expect(opentype).toBeDefined();
    expect(typeof opentype.parse).toBe('function');
    expect(wawoff2).toBeDefined();
    // wawoff2's public surface is a `decompress` async function.
    expect(typeof wawoff2.decompress).toBe('function');
  });

  test('returns the same module references on every call', () => {
    const first = loadFontCodec();
    const second = loadFontCodec();
    expect(second.opentype).toBe(first.opentype);
    expect(second.wawoff2).toBe(first.wawoff2);
  });

  test('matches a direct require of the underlying packages', () => {
    const { opentype, wawoff2 } = loadFontCodec();
    expect(opentype).toBe(require('opentype.js'));
    expect(wawoff2).toBe(require('wawoff2'));
  });
});

// `wawoff2.decompress` resolves with a `Uint8Array` that aliases the decoder's
// module-owned WASM heap: the next decode overwrites those bytes in place. A
// caller that yields between the resolve and its copy therefore receives the
// *last* call's output, and every earlier font silently corrupts. Pages
// routinely ship several web fonts (seven in the CJK showcase corpus; Font
// Awesome Free has three files), so a `Promise.all` over decodes is the norm
// and the failure mode is "one font loads, the rest are garbage".
describe('decompressWoff2', () => {
  let decompressWoff2;

  beforeEach(() => {
    jest.resetModules();
    ({ decompressWoff2 } = require('../dist/lib/font-codec'));
  });

  test('decodes every buffer correctly when several run concurrently', async () => {
    const { woff2Fixture } = require('./font-fixtures');
    const families = ['Ma Shan Zheng', 'ZCOOL XiaoWei', 'Long Cang', 'Liu Jian Mao Cao'];
    const buffers = [];
    for (const family of families) {
      buffers.push(await woff2Fixture(family));
    }

    const sfnts = await Promise.all(buffers.map((buffer) => decompressWoff2(buffer)));

    const { readFontMeta } = require('../dist/lib/font-download');
    expect(sfnts.map((sfnt) => readFontMeta(sfnt).family)).toEqual(families);
  });

  test('returns an independent Buffer copy a later decode cannot overwrite', async () => {
    const { woff2Fixture } = require('./font-fixtures');
    const first = await woff2Fixture('First Family');
    const second = await woff2Fixture('Second Family');
    const { readFontMeta } = require('../dist/lib/font-download');

    const decoded = await decompressWoff2(first);
    const snapshot = Buffer.from(decoded);
    expect(Buffer.isBuffer(decoded)).toBe(true);

    await decompressWoff2(second);

    expect(decoded.equals(snapshot)).toBe(true);
    expect(readFontMeta(decoded).family).toBe('First Family');
  });

  test('keeps the queue usable after a failed decode', async () => {
    const { woff2Fixture } = require('./font-fixtures');
    const good = await woff2Fixture('Recovered Family');
    // Signature intact, body truncated: the decoder throws instead of resolving.
    const truncated = Buffer.from([0x77, 0x4f, 0x46, 0x32, 0x00]);

    await expect(decompressWoff2(truncated)).rejects.toThrow();
    const sfnt = await decompressWoff2(good);
    const { readFontMeta } = require('../dist/lib/font-download');
    expect(readFontMeta(sfnt).family).toBe('Recovered Family');
  });
});
