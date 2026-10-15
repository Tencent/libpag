// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { parseFontFamilyStack, validateFontMap } = require('../dist/lib/font-metadata');
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
