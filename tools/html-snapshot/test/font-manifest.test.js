const fs = require('fs');
const os = require('os');
const path = require('path');
const { fontManifestArgs, readFontManifestData, encodeUnicodeRanges } = require('../dist/lib/font-manifest');

let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'font-manifest-')); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });

test('structured bindings preserve aliases, face indices, source order and unicode ranges', () => {
  const file = path.join(dir, 'faces.json');
  fs.writeFileSync(file, JSON.stringify({ version: 1, files: ['one.ttc'], bindings: [
    { family: 'CSSAlias', faces: [
      {path:'one.ttc', faceIndex:2, unicodeRange:'U+4E00-9FFF'},
      {path:'two.ttf', unicodeRange:'U+00??'},
      {systemFamily:'Arial', style:'Italic'},
    ] },
  ] }));
  expect(fontManifestArgs(file)).toEqual([
    '--font-face', 'CSSAlias', path.join(dir,'one.ttc'), '2', 'Regular', '19968-40959',
    '--font-face', 'CSSAlias', path.join(dir,'two.ttf'), '0', 'Regular', '0-255',
    '--font-face', 'CSSAlias', 'Arial', '-1', 'Italic', '',
  ]);
});

test('legacy newline manifests remain supported', () => {
  const font = path.join(dir, 'one.ttf'); fs.writeFileSync(font, 'font');
  const file = path.join(dir, 'fonts.txt'); fs.writeFileSync(file, font + '\n');
  expect(readFontManifestData(file).files).toEqual([font]);
  expect(fontManifestArgs(file)).toEqual(['--fallback', font]);
});

test('invalid versions and ranges are not silently treated as fallbacks', () => {
  const file = path.join(dir, 'fonts.json'); fs.writeFileSync(file, '{"version":2}');
  expect(() => readFontManifestData(file)).toThrow(/Unsupported/);
  for (const range of ['U+110000','U+20-10','U+?F-FF','garbage']) {
    expect(() => encodeUnicodeRanges(range)).toThrow(/unicode-range/);
  }
});
