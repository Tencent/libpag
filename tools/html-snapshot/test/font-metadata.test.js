// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { parseFontFamilyStack, validateFontMap, collectFontMetadata } = require('../dist/lib/font-metadata');
const source = '"PingFang SC", "Microsoft YaHei", sans-serif';
function rule(weight = '600', style = 'Bold') {
  return { from: { 'font-family': source, 'font-weight': weight, 'font-style': 'normal', 'font-weight-style': 'Semi Bold' },
    to: { 'font-family': 'Source Han Sans CN', 'font-weight': weight, 'font-style': 'normal', 'font-weight-style': style } };
}
test('accepts the provided from/to shape and an explicit 600 to Bold replacement', () => {
  expect(validateFontMap([rule()])).toEqual([]);
  expect(validateFontMap([])).toEqual([]);
});
test('parses quoted commas, escaped names and the ordered fallback stack', () => {
  expect(parseFontFamilyStack('"Font, Name", serif')).toEqual(['Font, Name', 'serif']);
  expect(parseFontFamilyStack('"A\\"B", Noto\\20 Sans')).toEqual(['A"B', 'Noto Sans']);
  expect(parseFontFamilyStack(source)).toEqual(['PingFang SC', 'Microsoft YaHei', 'sans-serif']);
});
test('rejects an editor catalog, invalid weights, absent target face and malformed families', () => {
  expect(validateFontMap({ version: 1, fonts: [] }).length).toBeGreaterThan(0);
  for (const weight of ['', '600oops', 'Infinity', -1, 1001]) expect(validateFontMap([rule(weight)]).length).toBeGreaterThan(0);
  expect(validateFontMap([rule('600', '')]).length).toBeGreaterThan(0);
  const bad = rule(); bad.from['font-family'] = '"unclosed';
  expect(validateFontMap([bad]).join()).toContain('unclosed quote');
});
test('rejects conflicting normalized source rules but accepts identical duplicates', () => {
  expect(validateFontMap([rule(), rule()])).toEqual([]);
  const second = rule('600', 'SemiBold');
  second.from['font-family'] = "'pingfang sc','Microsoft YaHei',sans-serif";
  expect(validateFontMap([rule(), second]).join()).toContain('conflicts');
});

test('deduplicates normalized requests while retaining stack order and unbucketed weights', () => {
  const base = { families: ['Font, Name', 'sans-serif'], weight: 650, style: 'normal' };
  expect(collectFontMetadata([
    base,
    { ...base, families: ['font, name', 'SANS-SERIF'] },
    { ...base, weight: 700 },
    { ...base, families: ['sans-serif', 'Font, Name'] },
  ])).toEqual({ version: 1, fonts: [base, { ...base, weight: 700 },
    { ...base, families: ['sans-serif', 'Font, Name'] }], missingRequests: 0 });
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
});

test('does not export malformed optional font metadata', () => {
  const base = { families: ['A'], weight: 400, style: 'normal' };
  for (const extra of [{ lang: 123 }, { variations: [] }, { variations: { wght: null } },
    { variations: { wght: Infinity } }, { variations: { invalid: 100 } }]) {
    expect(() => collectFontMetadata([{ ...base, ...extra }])).toThrow(/FONT_METADATA_MISMATCH/);
  }
  expect(collectFontMetadata([])).toEqual({ version: 1, fonts: [], missingRequests: 0 });
});
