// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { FontMatcher } = require('../dist/lib/font-matcher');
const families = ['PingFang SC', 'Microsoft YaHei', 'sans-serif'];
const rules = require('./fixtures/font-map.json');
test.each([[400, 'Regular'], [500, 'Medium'], [600, 'Bold'], [700, 'Bold']])('maps weight %i to the explicitly supplied %s face', (weight, style) => {
  expect(new FontMatcher(rules).match({ families, weight, style: 'normal' })).toEqual({ family: 'Source Han Sans CN', style });
});
test('normalizes quoting, comma whitespace and family case without changing the target names', () => {
  expect(new FontMatcher(rules).match({ families: ['pingfang sc', 'Microsoft YaHei', 'SANS-SERIF'], weight: 400, style: 'normal' })).toEqual({ family: 'Source Han Sans CN', style: 'Regular' });
});
test.each([
  { families, weight: 650, style: 'normal' },
  { families, weight: 400, style: 'italic' },
  { families: ['PingFang SC'], weight: 400, style: 'normal' },
  { families: ['Microsoft YaHei', 'PingFang SC', 'sans-serif'], weight: 400, style: 'normal' },
  { families: ['Icons'], weight: 400, style: 'normal' },
])('leaves an unmatched request alone: %j', request => {
  expect(new FontMatcher(rules).match(request)).toBeNull();
});
test('does not inspect source font files or pin a variable font to its default Regular face', () => {
  expect(new FontMatcher(rules).match({ families, weight: 700, style: 'normal', variations: { wght: 700 } })).toEqual({ family: 'Source Han Sans CN', style: 'Bold' });
});
test('cannot mutate subsequent matches through a returned target', () => {
  const matcher = new FontMatcher(rules);
  matcher.match({ families, weight: 600, style: 'normal' }).style = 'Changed';
  expect(matcher.match({ families, weight: 600, style: 'normal' }).style).toBe('Bold');
});
