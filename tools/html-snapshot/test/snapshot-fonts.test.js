// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { collectSnapshotFontMetadata, validateSnapshotFontMap } = require('../dist/lib/snapshot-fonts');
const source = { families: ['Source & Co', 'sans-serif'], weight: 650, style: 'normal', lang: 'zh' };
const attr = request => JSON.stringify(request).replace(/&/g, '&amp;').replace(/"/g, '&quot;');

test('collects only snapshot text requests, including inherited spans and SVG omissions', () => {
  const html = `<html><head><style>body{font-family:Other}</style><script>ignored</script></head><body>
    <span data-pagx-font-request="${attr(source)}">A<b>B</b></span>
    <span data-pagx-font-request="${attr({ ...source, weight: 700 })}">C</span>
    <span data-pagx-font-request="${attr(source)}">D</span>
    <svg><text>missing</text></svg><img src="data:image/png;base64,AAAA"><br>
  </body></html>`;
  expect(collectSnapshotFontMetadata(html)).toEqual({ version: 1, fonts: [
    { ...source, weightName: '650' }, { ...source, weight: 700, weightName: 'Bold' },
  ], missingRequests: 1 });
});

test('a nested request overrides its parent and empty text adds no fonts', () => {
  expect(collectSnapshotFontMetadata(`<body><span data-pagx-font-request="${attr(source)}">
    <b data-pagx-font-request="${attr({ ...source, weight: 700 })}">B</b>
  </span></body>`)).toEqual({ version: 1, fonts: [{ ...source, weight: 700, weightName: 'Bold' }], missingRequests: 0 });
});

test.each(['{}', '{oops}', '{&quot;families&quot;:[]}'])('rejects malformed captured requests: %s', raw => {
  expect(() => collectSnapshotFontMetadata(`<span data-pagx-font-request="${raw}">A</span>`))
    .toThrow(/FONT_METADATA_MISMATCH/);
});

const rule = { from: { 'font-family': '"SOURCE & CO", sans-serif', 'font-weight': 650, 'font-style': 'normal' },
  to: { 'font-family': 'Target', 'font-weight': 650, 'font-style': 'normal', 'font-weight-style': 'Medium' } };
test('accepts valid partial or empty maps and rejects sources outside the prepared snapshot', () => {
  const metadata = collectSnapshotFontMetadata(`<span data-pagx-font-request="${attr(source)}">A</span>`);
  expect(validateSnapshotFontMap([rule], metadata)).toEqual([]);
  expect(validateSnapshotFontMap([], metadata)).toEqual([]);
  expect(validateSnapshotFontMap([{ ...rule, from: { ...rule.from, 'font-weight': 700 } }], metadata))
    .toEqual(['font map[0].from is not in the prepared snapshot']);
  expect(validateSnapshotFontMap([{}], metadata).length).toBeGreaterThan(0);
});
