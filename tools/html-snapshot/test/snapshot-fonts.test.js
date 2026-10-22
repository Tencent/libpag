// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { collectSnapshotFontMetadata, prepareSnapshotFontReplacements, validateSnapshotFontMap } = require('../dist/lib/snapshot-fonts');
const source = { families: ['Source & Co', 'sans-serif'], weight: 650, style: 'normal', lang: 'zh' };
const style = "font-family:'Source &amp; Co',sans-serif;font-weight:650";
const rule = { from: { 'font-family': '"SOURCE & CO", sans-serif', 'font-weight': 650, 'font-style': 'normal' },
  to: { 'font-family': 'Target', 'font-weight': 500, 'font-style': 'italic', 'font-weight-style': 'Medium Italic' } };

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
  expect(prepareSnapshotFontReplacements(html, [rule]).mapping)
    .toEqual({ replaced: 0, unmatched: 0, missingRequests: 1 });
});

test('prepares exact target faces without altering source CSS or inheriting a mapped parent', () => {
  const html = `<div lang="zh" style="${style};left:12px;color:red">A<span>B</span>
    <span style="font-weight:700">C</span></div>`;
  const mapped = prepareSnapshotFontReplacements(html, [rule]);
  expect(mapped.mapping).toEqual({ replaced: 2, unmatched: 1, missingRequests: 0 });
  expect(mapped.replacements).toEqual([{
    families: "'Source & Co',sans-serif", weight: 650, style: 'normal',
    targetFamily: 'Target', targetStyle: 'Medium Italic',
  }]);
  expect(collectSnapshotFontMetadata(html).fonts[0].families).toEqual(source.families);
  expect(prepareSnapshotFontReplacements(html, []).replacements).toEqual([]);
});

test('parses semicolons in quoted family names and ignores unrelated URL styles', () => {
  const html = `<span style="font-family:'Source; Co';font-weight:650;background-image:url('a;b.png')">A</span>`;
  const custom = { ...rule, from: { ...rule.from, 'font-family': '"Source; Co"' } };
  const result = prepareSnapshotFontReplacements(html, [custom]);
  expect(result.mapping.replaced).toBe(1);
  expect(result.replacements[0].families).toBe("'Source; Co'");
});

test('validates partial and empty rules against the prepared inventory', () => {
  const metadata = collectSnapshotFontMetadata(`<span lang="zh" style="${style}">A</span>`);
  expect(validateSnapshotFontMap([rule], metadata)).toEqual([]);
  expect(validateSnapshotFontMap([], metadata)).toEqual([]);
  expect(validateSnapshotFontMap([{ ...rule, from: { ...rule.from, 'font-weight': 700 } }], metadata))
    .toEqual(['font map[0].from is not in the prepared snapshot']);
});
