// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { parseFontFamilyStack, collectFontMetadata } = require('../dist/lib/font-metadata');
const source = '"PingFang SC", "Microsoft YaHei", sans-serif';
test('parses quoted commas, escaped names and the ordered fallback stack', () => {
  expect(parseFontFamilyStack('"Font, Name", serif')).toEqual(['Font, Name', 'serif']);
  expect(parseFontFamilyStack('"A\\"B", Noto\\20 Sans')).toEqual(['A"B', 'Noto Sans']);
  expect(parseFontFamilyStack(source)).toEqual(['PingFang SC', 'Microsoft YaHei', 'sans-serif']);
});
test('deduplicates normalized requests while retaining stack order and unbucketed weights', () => {
  const base = { families: ['Font, Name', 'sans-serif'], weight: 650, style: 'normal' };
  expect(collectFontMetadata([
    base,
    { ...base, families: ['font, name', 'SANS-SERIF'] },
    { ...base, weight: 700 },
    { ...base, families: ['sans-serif', 'Font, Name'] },
  ])).toEqual({ version: 1, fonts: [{ ...base, weightName: '650' },
    { ...base, weight: 700, weightName: 'Bold' },
    { ...base, families: ['sans-serif', 'Font, Name'], weightName: '650' }], missingRequests: 0 });
});

test('retains distinct axes and languages, independent of axis property order', () => {
  const base = { families: ['Variable'], weight: 400, style: 'normal' };
  const metadata = collectFontMetadata([
    { ...base, variations: { wght: 650, wdth: 90 }, lang: 'zh-Hans' },
    { ...base, variations: { wdth: 90, wght: 650 }, lang: 'zh-Hans' },
    { ...base, variations: { wdth: 90, wght: 700 }, lang: 'zh-Hans' },
    { ...base, variations: { wdth: 90, wght: 650 }, lang: 'zh-Hant' },
  ], 2);
  expect(metadata.fonts).toHaveLength(3);
  expect(metadata.missingRequests).toBe(2);
  expect(metadata.fonts[0].variations).toEqual({ wdth: 90, wght: 650 });
  expect(metadata.fonts[0].weightName).toBe('Regular');
});

test.each([
  [100, 'Thin'], [200, 'ExtraLight'], [300, 'Light'], [400, 'Regular'], [500, 'Medium'],
  [600, 'SemiBold'], [700, 'Bold'], [800, 'ExtraBold'], [900, 'Black'],
  [1, '1'], [450.5, '450.5'], [650, '650'], [1000, '1000'],
])('exports weight %s as %s without rounding or changing the source', (weight, weightName) => {
  const request = { families: ['A'], weight, style: 'normal' };
  expect(collectFontMetadata([request]).fonts).toEqual([{ ...request, weightName }]);
  expect(request).toEqual({ families: ['A'], weight, style: 'normal' });
});

test('derives the display label instead of trusting a supplied font face name', () => {
  const request = { families: ['A'], weight: 600, style: 'normal' };
  expect(collectFontMetadata([{ ...request, weightName: 'Bold' }, request]).fonts)
    .toEqual([{ ...request, weightName: 'SemiBold' }]);
});

test('does not export malformed optional font metadata', () => {
  const base = { families: ['A'], weight: 400, style: 'normal' };
  for (const extra of [{ lang: 123 }, { variations: [] }, { variations: { wght: null } },
    { variations: { wght: Infinity } }, { variations: { invalid: 100 } }]) {
    expect(() => collectFontMetadata([{ ...base, ...extra }])).toThrow(/FONT_METADATA_MISMATCH/);
  }
  expect(collectFontMetadata([])).toEqual({ version: 1, fonts: [], missingRequests: 0 });
});
