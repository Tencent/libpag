// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { PNG } = require('pngjs');
const pipeline = require('../dist/lib/pipeline');
const { layoutContactSheet, generateContactSheet } = require('../dist/lib/contact-sheet');

let dir, render;
function png(file, width, height) {
  // Blank fixtures need no adaptive filter search, which is costly in Jest's VM.
  fs.writeFileSync(file, PNG.sync.write(new PNG({ width, height }), { filterType: 0 }));
  return file;
}
function image(name, width, height) { return { name, png: `${name}.png`, width, height }; }
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'contact sheet 中文 & \' '));
  render = jest.spyOn(pipeline, 'runPagxRender').mockImplementation(async args => {
    const xml = fs.readFileSync(args.pagxFile, 'utf8');
    const [, width, height] = xml.match(/^<pagx width="(\d+)" height="(\d+)">/);
    png(args.output, Number(width), Number(height));
    return { code: 0, durationMs: 0, stdout: '', stderr: '' };
  });
});
afterEach(() => { jest.restoreAllMocks(); fs.rmSync(dir, { recursive: true, force: true }); });

test('mixed sizes stay whole, keep their aspect ratios, and never upscale', () => {
  const images = [image('phone', 780, 1688), image('wide', 1920, 600), image('tall', 100, 12000), image('tiny', 20, 10)];
  const pages = layoutContactSheet(images);
  expect(pages).toEqual(layoutContactSheet(images));
  expect(pages.flatMap(page => page.items.map(item => item.name))).toEqual(images.map(item => item.name));
  for (const page of pages) {
    expect(page.width).toBeLessThanOrEqual(4096);
    expect(page.height).toBeLessThanOrEqual(4096);
    expect(page.width * page.height).toBeLessThanOrEqual(16000000);
    for (const item of page.items) {
      expect(item.thumbnailWidth / item.thumbnailHeight).toBeCloseTo(item.width / item.height, 8);
      expect(item.thumbnailWidth).toBeLessThanOrEqual(Math.min(item.width, 320));
      expect(item.thumbnailHeight).toBeLessThanOrEqual(Math.min(item.height, 480));
      expect(item.left + page.cellWidth).toBeLessThanOrEqual(page.width);
      expect(item.top + page.cellHeight).toBeLessThanOrEqual(page.height);
    }
  }
});

test('source pixels, output pixels, and output edges each trigger ordered pagination', () => {
  const images = Array.from({ length: 12 }, (_, i) => image(String(i), 100, 100));
  for (const limits of [{ maxSourcePixels: 20000 }, { maxPixels: 150000 }, { maxEdge: 500 }]) {
    const pages = layoutContactSheet(images, limits);
    expect(pages.length).toBeGreaterThan(1);
    expect(pages.flatMap(page => page.items.map(item => item.index))).toEqual(images.map((_, i) => i));
    for (const page of pages) {
      if (limits.maxSourcePixels) expect(page.items.reduce((sum, item) => sum + item.width * item.height, 0)).toBeLessThanOrEqual(limits.maxSourcePixels);
      if (limits.maxPixels) expect(page.width * page.height).toBeLessThanOrEqual(limits.maxPixels);
      if (limits.maxEdge) expect(Math.max(page.width, page.height)).toBeLessThanOrEqual(limits.maxEdge);
    }
  }
});

test('one image and requested columns are supported; impossible inputs fail clearly', () => {
  expect(layoutContactSheet([image('one', 10, 10)], { columns: 4 })[0].columns).toBe(1);
  expect(layoutContactSheet(Array.from({ length: 12 }, () => image('a', 390, 844)), { columns: 4 })[0].columns).toBe(4);
  expect(() => layoutContactSheet([])).toThrow('at least one PNG');
  for (const columns of [0, -1, 1.5, Infinity, NaN, 33]) {
    expect(() => layoutContactSheet([image('one', 1, 1)], { columns })).toThrow('columns');
  }
  expect(() => layoutContactSheet([image('large', 4000, 9000)])).toThrow('source pixel budget');
  expect(() => layoutContactSheet([image('one', 1, 1)], { maxEdge: 10 })).toThrow('output budget');
});

test('uses supplied files and real PNG dimensions, escapes labels, and renders at scale 1', async () => {
  const input = png(path.join(dir, '图片 & \' " < >.png'), 780, 1688);
  const before = fs.readFileSync(input);
  fs.writeFileSync(path.join(dir, 'unrelated.png'), 'not a PNG');
  const originalRender = render.getMockImplementation();
  let document;
  render.mockImplementation(async args => { document = fs.readFileSync(args.pagxFile, 'utf8'); return originalRender(args); });
  const result = await generateContactSheet([{ name: '首页 & < > " \'\u0001', png: input }], { outputDir: dir });
  expect(result.pages).toHaveLength(1);
  expect(result.pages[0].items).toEqual(['首页 & < > " \'\u0001']);
  expect(document).toContain('780 × 1688 px');
  expect(document).toContain('首页 &amp; &lt; &gt; &quot; &apos;�');
  expect(document).toContain('image="input-0.png"');
  expect(document).not.toContain(input);
  expect(render.mock.calls[0][0].scale).toBe(1);
  expect(fs.readFileSync(input)).toEqual(before);
  expect(fs.readdirSync(dir).some(file => file.startsWith('.tmp-'))).toBe(false);
});

test.each(['missing', 'signature', 'crc', 'truncated', 'huge'])('rejects %s PNG before calling the renderer', async kind => {
  const input = path.join(dir, 'bad.png');
  if (kind !== 'missing') {
    png(input, 20, 20);
    let bytes = fs.readFileSync(input);
    if (kind === 'signature') bytes[0] = 0;
    if (kind === 'crc') bytes[bytes.length - 1] ^= 255;
    if (kind === 'truncated') bytes = bytes.subarray(0, 40);
    if (kind === 'huge') bytes.writeUInt32BE(4000000000, 16);
    fs.writeFileSync(input, bytes);
  }
  await expect(generateContactSheet([{ name: kind, png: input }], { outputDir: path.join(dir, 'out') })).rejects.toThrow('invalid preview input');
  expect(render).not.toHaveBeenCalled();
  expect(fs.readdirSync(path.join(dir, 'out'))).toEqual([]);
});

test('a later render failure preserves published sheets and cleans all staging files', async () => {
  const input = png(path.join(dir, 'input.png'), 100, 100);
  const outputDir = path.join(dir, 'out'); fs.mkdirSync(outputDir);
  const previous = path.join(outputDir, 'sheet-001.png'); fs.writeFileSync(previous, 'previous run');
  render.mockImplementationOnce(render.getMockImplementation()).mockResolvedValueOnce({ code: 1, stdout: '', stderr: 'device failed' });
  await expect(generateContactSheet([{ name: 'a', png: input }, { name: 'b', png: input }],
    { outputDir, maxSourcePixels: 10000 })).rejects.toThrow('device failed');
  expect(fs.readFileSync(previous, 'utf8')).toBe('previous run');
  expect(fs.readdirSync(outputDir)).toEqual(['sheet-001.png']);
});

test('success without a valid rendered file is a failure', async () => {
  const input = png(path.join(dir, 'input.png'), 10, 10);
  render.mockImplementation(async args => { fs.writeFileSync(args.output, 'bad render'); return { code: 0 }; });
  await expect(generateContactSheet([{ name: 'a', png: input }], { outputDir: path.join(dir, 'out') })).rejects.toThrow('PNG file size');
  expect(fs.readdirSync(path.join(dir, 'out'))).toEqual([]);
});

test('a shorter repeated run returns only its own pages and leaves unrelated files alone', async () => {
  const input = png(path.join(dir, 'input.png'), 100, 100);
  const options = { outputDir: path.join(dir, 'out'), maxSourcePixels: 10000 };
  const first = await generateContactSheet([{ name: 'a', png: input }, { name: 'b', png: input }], options);
  expect(first.pages).toHaveLength(2);
  const second = await generateContactSheet([{ name: 'selected', png: input }], options);
  expect(second.pages).toHaveLength(1);
  expect(second.pages[0].items).toEqual(['selected']);
  expect(fs.existsSync(first.pages[1].file)).toBe(true);
});

test('generic use cannot overwrite a source PNG with the same output name', async () => {
  const input = png(path.join(dir, 'sheet-001.png'), 50, 50);
  const before = fs.readFileSync(input);
  await expect(generateContactSheet([{ name: 'source', png: input }], { outputDir: dir })).rejects.toThrow('would overwrite an input PNG');
  expect(fs.readFileSync(input)).toEqual(before);
  expect(render).not.toHaveBeenCalled();
});

test('publication errors propagate and clean staging without deleting existing destinations', async () => {
  const input = png(path.join(dir, 'input.png'), 50, 50);
  const destination = path.join(dir, 'sheet-001.png');
  fs.mkdirSync(destination);
  fs.writeFileSync(path.join(destination, 'keep.txt'), 'existing directory');
  await expect(generateContactSheet([{ name: 'source', png: input }], { outputDir: dir })).rejects.toThrow();
  expect(fs.readFileSync(path.join(destination, 'keep.txt'), 'utf8')).toBe('existing directory');
  expect(fs.readdirSync(dir).some(file => file.startsWith('.tmp-'))).toBe(false);
});
