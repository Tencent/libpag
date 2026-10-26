// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { PNG } = require('pngjs');
const { generateContactSheet, layoutContactSheet } = require('../dist/lib/contact-sheet');
const pagxBin = path.resolve(__dirname, '../../../cmake-build-debug/pagx');
const nativeTest = fs.existsSync(pagxBin) ? test : test.skip;

nativeTest('native sheets preserve full images, transparency, order, and pixels across pages', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'preview 原图 & \' '));
  try {
    const descriptions = [
      { name: '长图 & < " 测试', width: 100, height: 2000, pixel: (_x, y) => y < 1000 ? [255, 0, 0, 255] : [0, 0, 255, 255] },
      { name: 'wide', width: 600, height: 100, pixel: x => x < 300 ? [0, 255, 0, 255] : [255, 255, 0, 255] },
      { name: 'transparent', width: 80, height: 80, pixel: x => x < 40 ? [0, 0, 0, 0] : [255, 0, 0, 128] },
    ];
    const inputs = descriptions.map((item, index) => {
      const png = new PNG(item);
      for (let y = 0; y < item.height; y++) for (let x = 0; x < item.width; x++) {
        png.data.set(item.pixel(x, y), (y * item.width + x) * 4);
      }
      const file = path.join(dir, `图 ${index} '&.png`);
      fs.writeFileSync(file, PNG.sync.write(png));
      return { name: item.name, png: file, width: item.width, height: item.height };
    });
    const options = { outputDir: path.join(dir, 'output'), pagxBin, columns: 2, maxSourcePixels: 220000 };
    const layouts = layoutContactSheet(inputs, options);
    const result = await generateContactSheet(inputs, options);
    expect(result.pages).toHaveLength(2);
    expect(result.pages.flatMap(page => page.items)).toEqual(descriptions.map(item => item.name));
    for (let i = 0; i < result.pages.length; i++) {
      const page = result.pages[i], layout = layouts[i];
      const rendered = PNG.sync.read(fs.readFileSync(page.file));
      expect([rendered.width, rendered.height]).toEqual([page.width, page.height]);
      const pixel = (x, y) => Array.from(rendered.data.subarray((Math.floor(y) * rendered.width + Math.floor(x)) * 4,
        (Math.floor(y) * rendered.width + Math.floor(x)) * 4 + 4));
      for (const item of layout.items) {
        const x = item.left + (layout.cellWidth - item.thumbnailWidth) / 2;
        const y = item.top + 64;
        if (item.name === descriptions[0].name) {
          expect(pixel(x + item.thumbnailWidth / 2, y + 5)).toEqual([255, 0, 0, 255]);
          expect(pixel(x + item.thumbnailWidth / 2, y + item.thumbnailHeight - 5)).toEqual([0, 0, 255, 255]);
        } else if (item.name === 'wide') {
          expect(pixel(x + 5, y + item.thumbnailHeight / 2)).toEqual([0, 255, 0, 255]);
          expect(pixel(x + item.thumbnailWidth - 5, y + item.thumbnailHeight / 2)).toEqual([255, 255, 0, 255]);
        } else {
          const a = pixel(x + 3, y + 3), b = pixel(x + 11, y + 3);
          expect(new Set([a[0], b[0]])).toEqual(new Set([214, 240]));
          expect(a.slice(0, 3)).toEqual([a[0], a[0], a[0]]);
          const mixed = pixel(x + 60, y + 30);
          expect(mixed[0]).toBeGreaterThan(230);
          expect(mixed[1]).toBeLessThan(125);
          expect(mixed[3]).toBe(255);
        }
      }
    }
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
}, 60000);
