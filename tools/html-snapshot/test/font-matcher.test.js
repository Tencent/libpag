// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { FontMatcher, validateAvailableFonts } = require('../dist/lib/font-matcher');
const font = (family, style = 'Regular') => ({ fontName: { family, style } });
const request = (families, weight = 400, style = 'normal') => ({ families, weight, style });

test.each([[100, 'Light'], [400, 'Regular'], [450, 'Regular'], [500, 'Medium'], [650, 'Bold'], [900, 'Bold']])(
  'selects the nearest known weight for %i', (weight, style) => {
    const matcher = new FontMatcher(['Light', 'Regular', 'Medium', 'SemiBold', 'Bold'].map(s => font('Noto Sans', s)));
    expect(matcher.match(request(['Noto Sans'], weight))).toEqual({ family: 'Noto Sans', style });
  });
test('normalizes names but preserves the exact catalog family and face', () => {
  expect(new FontMatcher([font('Long Cang', 'regular')]).match(request(['LONG-CANG'])))
    .toEqual({ family: 'Long Cang', style: 'regular' });
});
test.each([['QingKeHuangYou', 'ZCOOL QingKe HuangYou'], ['XiaoWei', 'ZCOOL XiaoWei'], ['Noto Sanz', 'Noto Sans']])(
  'matches a unique high confidence name %s', (source, target) => {
    expect(new FontMatcher([font(target)]).match(request([source]))).toEqual({ family: target, style: 'Regular' });
  });
test('respects family order, skips generic names and continues after missing names', () => {
  const matcher = new FontMatcher([font('Long Cang'), font('Noto Sans'), font('system-ui')]);
  expect(matcher.match(request(['system-ui', 'Missing', 'LongCang', 'Noto Sans'])).family).toBe('Long Cang');
  expect(matcher.match(request(['Noto Sans', 'LongCang'])).family).toBe('Noto Sans');
  expect(matcher.match(request(['system-ui', 'serif']))).toBeNull();
});
test('does not resolve ambiguous normalized or fuzzy names by input order', () => {
  const fonts = [font('Noto Sans'), font('Noto-Sans'), font('ABC XiaoWei'), font('XYZ XiaoWei')];
  for (const list of [fonts, [...fonts].reverse()]) {
    const matcher = new FontMatcher(list);
    expect(matcher.match(request(['NOTOSANS']))).toBeNull();
    expect(matcher.match(request(['XiaoWei']))).toBeNull();
    expect(matcher.match(request(['Noto Sans'])).family).toBe('Noto Sans');
    expect(matcher.match(request(['Noto-Sans'])).family).toBe('Noto-Sans');
  }
});
test('does not let normalized cache keys change exact family identity', () => {
  const matcher = new FontMatcher([font('Face'), font('FACE')]);
  expect(matcher.match(request(['Face'])).family).toBe('Face');
  expect(matcher.match(request(['FACE'])).family).toBe('FACE');
  expect(matcher.match(request(['face']))).toBeNull();
});
test('prioritizes slant and selects actual face names without synthesizing them', () => {
  const matcher = new FontMatcher([font('Sample', 'Regular'), font('Sample', 'Bold Italic'), font('Sample', 'Oblique')]);
  expect(matcher.match(request(['Sample'], 400, 'italic')).style).toBe('Bold Italic');
  expect(matcher.match(request(['Sample'], 700, 'oblique')).style).toBe('Oblique');
});
test('unknown weights are not assumed to be regular and ties are deterministic', () => {
  const fonts = [font('Sample', 'Mystery'), font('Sample', 'Bold')];
  expect(new FontMatcher(fonts).match(request(['Sample'])).style).toBe('Bold');
  expect(new FontMatcher([font('Sample', 'Zulu'), font('Sample', 'Alpha')]).match(request(['Sample'])).style).toBe('Alpha');
});
test('leaves weak, short and unrelated names unmatched', () => {
  const matcher = new FontMatcher([font('Arial'), font('Noto Sans'), font('Long Cang')]);
  for (const family of ['Aria', 'Other', 'Icons', 'Noto Serif', '']) expect(matcher.match(request([family]))).toBeNull();
});
test('deduplicates catalog entries and prevents returned objects from mutating cached results', () => {
  const matcher = new FontMatcher([font('Sample'), font('Sample')]);
  matcher.match(request(['Sample'])).style = 'Changed';
  expect(matcher.match(request(['Sample'])).style).toBe('Regular');
});
test.each([null, {}, [{ from: {}, to: {} }], [{ fontName: { family: 'A' } }],
  [font('')], [font('A', ' ')], [font('A\0B')], [font('A'.repeat(257))]])('rejects malformed catalog %j', value => {
  expect(validateAvailableFonts(value).length).toBeGreaterThan(0);
  expect(() => new FontMatcher(value)).toThrow(/INVALID_AVAILABLE_FONTS/);
});
test('accepts an empty catalog', () => {
  expect(validateAvailableFonts([])).toEqual([]);
  expect(new FontMatcher([]).match(request(['A']))).toBeNull();
});
