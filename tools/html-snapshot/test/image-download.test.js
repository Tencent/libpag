'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  saveDownloadedImages,
  pickExt,
  sniffExt,
  baseNameFromUrl,
} = require('../lib/image-download');

// Minimal valid magic-byte headers for the formats the sniffer recognises.
const PNG = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
const JPG = Buffer.from([0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10]);
const GIF = Buffer.from('GIF89a');
const WEBP = Buffer.concat([
  Buffer.from('RIFF'),
  Buffer.from([0x00, 0x00, 0x00, 0x00]),
  Buffer.from('WEBP'),
]);

describe('sniffExt', () => {
  test.each([
    [PNG, 'png'],
    [JPG, 'jpg'],
    [GIF, 'gif'],
    [WEBP, 'webp'],
    [Buffer.from([0x42, 0x4d, 0x00, 0x00]), 'bmp'],
    [Buffer.from('<svg xmlns="...">'), 'svg'],
    [Buffer.from('<?xml version="1.0"?><svg/>'), 'svg'],
  ])('detects the container from magic bytes', (buf, expected) => {
    expect(sniffExt(buf)).toBe(expected);
  });

  test('falls back to img for unknown / tiny buffers', () => {
    expect(sniffExt(Buffer.from('ab'))).toBe('img');
    expect(sniffExt(Buffer.from([0x01, 0x02, 0x03, 0x04]))).toBe('img');
  });
});

describe('pickExt', () => {
  test('prefers the content-type and strips parameters', () => {
    expect(pickExt('image/png', JPG)).toBe('png');
    expect(pickExt('image/jpeg; charset=binary', PNG)).toBe('jpg');
    expect(pickExt('IMAGE/WEBP', PNG)).toBe('webp');
  });

  test('falls back to a byte sniff for generic / missing content-types', () => {
    expect(pickExt('application/octet-stream', PNG)).toBe('png');
    expect(pickExt('', GIF)).toBe('gif');
    expect(pickExt(undefined, JPG)).toBe('jpg');
  });
});

describe('baseNameFromUrl', () => {
  test('derives a slug from the URL basename without the extension', () => {
    expect(baseNameFromUrl('https://cdn.example.com/path/photo.png')).toBe('photo');
    expect(baseNameFromUrl('https://x/a/b/Hero_01.JPG?w=200')).toBe('Hero_01');
  });

  test('falls back to a stable default for empty / odd names', () => {
    expect(baseNameFromUrl('https://example.com/')).toBe('image');
    expect(baseNameFromUrl('https://example.com/?q=1')).toBe('image');
  });
});

describe('saveDownloadedImages', () => {
  let outDir;
  beforeEach(() => {
    outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-image-test-'));
  });
  afterEach(() => {
    fs.rmSync(outDir, { recursive: true, force: true });
  });

  test('returns an empty array for an empty / missing map', async () => {
    expect(await saveDownloadedImages(new Map(), outDir)).toEqual([]);
    expect(await saveDownloadedImages(null, outDir)).toEqual([]);
  });

  test('writes a content-addressed file with the right extension', async () => {
    const map = new Map([
      ['https://x/a.png', { buffer: PNG, contentType: 'image/png' }],
    ]);
    const saved = await saveDownloadedImages(map, outDir);
    expect(saved).toHaveLength(1);
    expect(saved[0].url).toBe('https://x/a.png');
    expect(fs.existsSync(saved[0].path)).toBe(true);
    expect(path.dirname(saved[0].path)).toBe(outDir);
    expect(saved[0].path.endsWith('.png')).toBe(true);
  });

  test('accepts a bare Buffer entry (no content-type wrapper)', async () => {
    const map = new Map([['https://x/b', JPG]]);
    const saved = await saveDownloadedImages(map, outDir);
    expect(saved).toHaveLength(1);
    expect(saved[0].path.endsWith('.jpg')).toBe(true);
  });

  test('de-dupes identical bytes served from different URLs', async () => {
    const map = new Map([
      ['https://x/a.png', { buffer: PNG, contentType: 'image/png' }],
      ['https://y/b.png', { buffer: Buffer.from(PNG), contentType: 'image/png' }],
    ]);
    const saved = await saveDownloadedImages(map, outDir);
    expect(saved).toHaveLength(1);
    expect(fs.readdirSync(outDir)).toHaveLength(1);
  });

  test('skips empty / non-buffer entries', async () => {
    const map = new Map([
      ['https://x/empty', { buffer: Buffer.alloc(0), contentType: 'image/png' }],
      ['https://x/null', null],
    ]);
    const saved = await saveDownloadedImages(map, outDir);
    expect(saved).toEqual([]);
  });
});
