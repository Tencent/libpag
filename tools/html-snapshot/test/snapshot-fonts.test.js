// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { collectSnapshotFontMetadata, prepareSnapshotFontReplacements } = require('../dist/lib/snapshot-fonts');
const source = { families: ['Source & Co', 'sans-serif'], weight: 650, style: 'normal', lang: 'zh' };
const style = "font-family:'Source &amp; Co',sans-serif;font-weight:650";
const fonts = [{ fontName: { family: 'Source & Co', style: 'Medium Italic' } }];

test('collects standard inherited CSS, exact numeric weights and language without request attributes', () => {
  const html = `<html lang="zh"><head><style>ignored</style></head><body>
    <span style="${style}">A<b style="font-weight:700">B</b></span>
    <span style="${style}">C</span><svg><text>missing</text></svg></body></html>`;
  expect(collectSnapshotFontMetadata(html)).toEqual({ version: 1, fonts: [
    { ...source, weightName: '650' }, { ...source, weight: 700, weightName: 'Bold' },
  ], missingRequests: 1 });
});

test('reads variation axes from computed HTML styles', () => {
  const result = collectSnapshotFontMetadata(`<div lang="ar"><span
    style="font-family:Arabic;font-weight:500;font-style:oblique 14deg;font-variation-settings:'wght' 550,'wdth' 90">Hi</span></div>`);
  expect(result.fonts).toEqual([{ families: ['Arabic'], weight: 500, weightName: 'Medium',
    style: 'oblique', variations: { wght: 550, wdth: 90 }, lang: 'ar' }]);
});

test('does not guess SVG computed requests from retained authored markup', () => {
  const html = `<div style="font-family:Source"><svg><text font-family="Arabic">Hi</text></svg></div>`;
  expect(collectSnapshotFontMetadata(html)).toEqual({ version: 1, fonts: [], missingRequests: 1 });
  expect(prepareSnapshotFontReplacements(html, fonts).mapping)
    .toEqual({ replaced: 0, unmatched: 0, missingRequests: 1 });
});

test('prepares exact target faces without altering source CSS or inheriting a mapped parent', () => {
  const html = `<div lang="zh" style="${style};left:12px;color:red">A<span>B</span>
    <span style="font-weight:700">C</span></div>`;
  const mapped = prepareSnapshotFontReplacements(html, fonts);
  expect(mapped.mapping).toEqual({ replaced: 3, unmatched: 0, missingRequests: 0 });
  expect(mapped.replacements).toEqual([{
    families: "'Source & Co',sans-serif", weight: 650, style: 'normal',
    targetFamily: 'Source & Co', targetStyle: 'Medium Italic',
  }, {
    families: "'Source & Co',sans-serif", weight: 700, style: 'normal',
    targetFamily: 'Source & Co', targetStyle: 'Medium Italic',
  }]);
  expect(collectSnapshotFontMetadata(html).fonts[0].families).toEqual(source.families);
  expect(prepareSnapshotFontReplacements(html, []).replacements).toEqual([]);
});

test('parses semicolons in quoted family names and ignores unrelated URL styles', () => {
  const html = `<span style="font-family:'Source; Co';font-weight:650;background-image:url('a;b.png')">A</span>`;
  const custom = { fontName: { family: 'Source; Co', style: 'Regular' } };
  const result = prepareSnapshotFontReplacements(html, [custom]);
  expect(result.mapping.replaced).toBe(1);
  expect(result.replacements[0].families).toBe("'Source; Co'");
});
