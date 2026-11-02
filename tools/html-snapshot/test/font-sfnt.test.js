'use strict';

const { readSfntNames, readFontMeta } = require('../dist/lib/font-download');

// A small SFNT with only a name table. No system fonts or downloaded fixtures
// are needed to validate record ordering, encodings and truncated buffers.
function sfnt(records) {
  const payloads = records.map(record => record.bytes || (record.platform === 1
    ? Buffer.from(record.text, 'latin1') : Buffer.from(record.text, 'utf16le').swap16()));
  const strings = 6 + records.length * 12;
  const data = Buffer.alloc(strings + payloads.reduce((sum, bytes) => sum + bytes.length, 0));
  data.writeUInt16BE(records.length, 2);
  data.writeUInt16BE(strings, 4);
  let offset = 0;
  records.forEach((record, i) => {
    const p = 6 + i * 12;
    data.writeUInt16BE(record.platform ?? 3, p);
    data.writeUInt16BE(record.language ?? 0x409, p + 4);
    data.writeUInt16BE(record.id ?? 1, p + 6);
    data.writeUInt16BE(payloads[i].length, p + 8);
    data.writeUInt16BE(offset, p + 10);
    payloads[i].copy(data, strings + offset);
    offset += payloads[i].length;
  });
  const header = Buffer.alloc(44);
  header.writeUInt32BE(0x00010000);
  header.writeUInt16BE(2, 4);
  header.write('head', 12); // Unrelated tables must be skipped.
  header.write('name', 28);
  header.writeUInt32BE(44, 36);
  header.writeUInt32BE(data.length, 40);
  return Buffer.concat([header, data]);
}

test('prefers Windows English names over Unicode/Mac names and keeps the first equally-ranked record', () => {
  const font = sfnt([
    { platform: 1, text: 'Café' }, { platform: 0, language: 0, text: 'Unicode' },
    { language: 0x804, text: '中文' }, { text: 'Windows' }, { text: 'Duplicate' },
    { platform: 0, text: 'Lower ranked' },
    { id: 16, text: 'Preferred Family' }, { id: 17, text: 'Preferred Style' },
    { id: 2, text: 'Regular' }, { id: 6, text: 'Ignored PostScript' },
    { platform: 2, text: 'Unsupported platform' }, { text: '' },
  ]);
  expect(Object.fromEntries(readSfntNames(font))).toEqual({
    1: 'Windows', 2: 'Regular', 16: 'Preferred Family', 17: 'Preferred Style',
  });
  expect(readFontMeta(font)).toEqual({ family: 'Preferred Family', style: 'Preferred Style', ext: 'ttf' });
});

test('decodes Mac Latin-1 and Unicode UTF-16BE names without glyph tables', () => {
  expect(readSfntNames(sfnt([{ platform: 1, text: 'Café' }])).get(1)).toBe('Café');
  expect(readSfntNames(sfnt([{ platform: 0, text: '测试😀' }])).get(1)).toBe('测试😀');
});

test('rejects malformed table and string lengths without reading beyond the buffer', () => {
  const font = sfnt([{ text: 'Font' }]);
  const corruptions = [
    buffer => buffer.subarray(0, 5),
    buffer => buffer.subarray(0, 20),
    buffer => { buffer.writeUInt32BE(0xffffffff, 36); return buffer; },
    buffer => { buffer.writeUInt32BE(2, 40); return buffer; },
    buffer => { buffer.writeUInt16BE(999, 46); return buffer; },
    buffer => { buffer.writeUInt16BE(999, 48); return buffer; },
    buffer => { buffer.writeUInt16BE(999, 58); return buffer; },
  ];
  for (const corrupt of corruptions) expect(readSfntNames(corrupt(Buffer.from(font))).size).toBe(0);
  expect(readSfntNames(sfnt([{ bytes: Buffer.from([1, 2, 3]) }])).size).toBe(0);
  expect(readSfntNames(sfnt([{ text: '' }])).size).toBe(0);
});

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
