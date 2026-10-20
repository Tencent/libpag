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

  test('retains the parser and compression API while wrapping decompression', () => {
    const { opentype, wawoff2 } = loadFontCodec();
    expect(opentype).toBe(require('opentype.js'));
    expect(wawoff2.compress).toBe(require('wawoff2').compress);
  });

  test('copies shared WASM memory before another decode can overwrite it', async () => {
    jest.resetModules();
    const shared = Buffer.alloc(8);
    jest.doMock('wawoff2', () => ({
      decompress: async (input) => {
        shared.fill(input[0]);
        return shared.subarray(0, 4);
      },
    }));
    try {
      const codec = require('../dist/lib/font-codec').loadFontCodec().wawoff2;
      const outputs = await Promise.all([1, 2, 3].map((n) => codec.decompress(Buffer.from([n]))));
      expect(outputs.map((out) => [...out])).toEqual([[1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3]]);
      shared.fill(9);
      expect(outputs[0][0]).toBe(1);
    } finally {
      jest.dontMock('wawoff2');
      jest.resetModules();
    }
  });
});
