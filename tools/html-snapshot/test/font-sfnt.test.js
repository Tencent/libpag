'use strict';

describe('font codec adapters', () => {
  afterEach(() => { jest.dontMock('../dist/lib/font-codec'); jest.resetModules(); });
  function loadCodec(codec) {
    jest.resetModules();
    jest.doMock('../dist/lib/font-codec', () => ({ loadFontCodec: () => codec }));
    return require('../dist/lib/font-download');
  }

  test.each([Buffer.from('sfnt'), new Uint8Array([1, 2, 3])])('converts WOFF2 decoder output to a Buffer', async decoded => {
    const decompress = jest.fn().mockResolvedValue(decoded);
    const { toSfnt } = loadCodec({ opentype: {}, wawoff2: { decompress } });
    const input = Buffer.from('wOF2encoded');
    expect(await toSfnt(input)).toEqual(Buffer.from(decoded));
    expect(decompress).toHaveBeenCalledWith(input);
  });

  test('passes only the WOFF view bytes to the parser and serializes the decoded font', async () => {
    const input = Buffer.from('prefixwOFFdatasuffix').subarray(6, 14);
    const parse = jest.fn(() => ({ toArrayBuffer: () => Uint8Array.from([0, 1, 2]).buffer }));
    const { toSfnt } = loadCodec({ opentype: { parse }, wawoff2: {} });
    expect(await toSfnt(input)).toEqual(Buffer.from([0, 1, 2]));
    expect(Buffer.from(parse.mock.calls[0][0])).toEqual(Buffer.from('wOFFdata'));
  });

  test('uses preferred names when primary name records are malformed or empty', () => {
    const { readFontMeta: read } = loadCodec({ opentype: { parse: () => ({ names: {
      windows: { fontFamily: { en: '', value: 42 }, fontSubfamily: 'invalid' },
      unicode: { preferredFamily: { ja: 'Family' }, preferredSubfamily: { en: 'Bold' } },
    } }) }, wawoff2: {} });
    expect(read(Buffer.from('data'))).toEqual({ family: 'Family', style: 'Bold', ext: 'ttf' });
  });
});
