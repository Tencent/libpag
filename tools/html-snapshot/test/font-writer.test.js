// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const fs = require('fs'), os = require('os'), path = require('path');
const { DOMParser } = require('@xmldom/xmldom');
const { rewritePagxFonts, finalizePagxFonts } = require('../dist/lib/pagx-font-writer');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'font-rule-writer-')); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });
const rule = { from: { 'font-family': '"PingFang SC", sans-serif', 'font-weight': '600', 'font-style': 'normal' },
  to: { 'font-family': 'Target & Name', 'font-weight': '600', 'font-style': 'normal', 'font-weight-style': 'Bold' } };
function request(weight) {
  return JSON.stringify({ families: ['PingFang SC', 'sans-serif'], weight, style: 'normal' }).replace(/"/g, '&quot;');
}
function fixture() {
  return `<pagx width="200" height="100"><Layer data-note="keep &amp; preserve">
<Text text="A &amp; B" fontFamily="Linux Fallback" fontStyle="SemiBold" data-html-font-request="${request(600)}"><GlyphRun font="@f" glyphs="1" positions="1,2"/></Text>
<Text id="existing-id" text="C" fontFamily="Linux Fallback" fontStyle="Bold" data-html-font-request="${request(700)}"/>
<Text text="icon" fontFamily="Icon Font" fontStyle="Regular"/>
</Layer><Resources><Font id="f"><Glyph path="M0,0L1,1"/></Font></Resources></pagx>`;
}
function write(xml) { const file = path.join(dir, 'test.pagx'); fs.writeFileSync(file, xml); return file; }
test('rewrites anonymous Text directly, preserves unmatched fonts and existing ids, and removes only the temporary requests', () => {
  const file = write(fixture());
  expect(rewritePagxFonts({ inputPath: file, outputPath: file, fontMap: [rule] })).toEqual({ replaced: 1, unmatched: 1, missingRequests: 1 });
  const out = fs.readFileSync(file, 'utf8');
  const doc = new DOMParser().parseFromString(out, 'application/xml');
  const texts = doc.getElementsByTagName('Text');
  expect(texts[0].getAttribute('fontFamily')).toBe('Target & Name');
  expect(texts[0].getAttribute('fontStyle')).toBe('Bold');
  expect(texts[0].hasAttribute('id')).toBe(false);
  expect(texts[0].getAttribute('text')).toBe('A & B');
  expect(texts[1].getAttribute('fontFamily')).toBe('Linux Fallback');
  expect(texts[1].getAttribute('id')).toBe('existing-id');
  expect(texts[2].getAttribute('fontFamily')).toBe('Icon Font');
  expect(out).not.toContain('data-html-font-request');
  expect(out).not.toContain('data-font-document-id');
  expect(out).toContain('<GlyphRun font="@f" glyphs="1" positions="1,2"/>');
  expect(out).toContain('<Resources><Font id="f"><Glyph path="M0,0L1,1"/></Font></Resources>');
});
test('leaves the file untouched on malformed requests or conflicting rules', () => {
  for (const xml of [fixture().replace(request(600), '{oops}'), fixture().replace(request(600), '{}')]) {
    const file = write(xml);
    expect(() => rewritePagxFonts({ inputPath: file, outputPath: file, fontMap: [rule] })).toThrow(/FONT_METADATA_MISMATCH/);
    expect(fs.readFileSync(file, 'utf8')).toBe(xml);
  }
  const file = write(fixture());
  const conflict = { ...rule, to: { ...rule.to, 'font-weight-style': 'Other' } };
  expect(() => rewritePagxFonts({ inputPath: file, outputPath: file, fontMap: [rule, conflict] })).toThrow(/INVALID_FONT_MAP/);
  expect(fs.readFileSync(file, 'utf8')).toBe(fixture());
});

test.each([undefined, [rule]])('exports source fonts with and without replacements: %j', fontMap => {
  const file = write(fixture());
  const metadataPath = path.join(dir, 'test.fonts.json');
  const result = finalizePagxFonts({ inputPath: file, outputPath: file, metadataPath, fontMap });
  const metadata = JSON.parse(fs.readFileSync(metadataPath, 'utf8'));
  expect(metadata).toEqual({ version: 1, fonts: [
    { families: ['PingFang SC', 'sans-serif'], weight: 600, weightName: 'SemiBold', style: 'normal' },
    { families: ['PingFang SC', 'sans-serif'], weight: 700, weightName: 'Bold', style: 'normal' },
  ], missingRequests: 1 });
  expect(result.metadata).toEqual(metadata);
  expect(result.fontMapping).toEqual(fontMap
    ? { replaced: 1, unmatched: 1, missingRequests: 1 } : undefined);
  const out = fs.readFileSync(file, 'utf8');
  expect(out).not.toContain('data-html-font-request');
  expect(out).toContain('fontFamily="Linux Fallback"');
  expect(out).toContain('<GlyphRun font="@f" glyphs="1" positions="1,2"/>');
  expect(out).toContain('<Resources><Font id="f"><Glyph path="M0,0L1,1"/></Font></Resources>');
  expect(out.includes('fontFamily="Target &amp; Name"')).toBe(!!fontMap);
});

test('a failed metadata write leaves source requests in the PAGX', () => {
  const file = write(fixture());
  expect(() => finalizePagxFonts({ inputPath: file, outputPath: file,
    metadataPath: path.join(dir, 'missing-parent', 'fonts.json') })).toThrow();
  expect(fs.readFileSync(file, 'utf8')).toBe(fixture());
  expect(fs.readdirSync(dir)).toEqual(['test.pagx']);
});

test('invalid requests cannot overwrite an existing inventory or change the PAGX', () => {
  const file = write(fixture().replace(request(600), '{}'));
  const original = fs.readFileSync(file, 'utf8');
  const metadataPath = path.join(dir, 'test.fonts.json');
  fs.writeFileSync(metadataPath, 'previous inventory');
  expect(() => finalizePagxFonts({ inputPath: file, outputPath: file, metadataPath })).toThrow(/FONT_METADATA_MISMATCH/);
  expect(fs.readFileSync(file, 'utf8')).toBe(original);
  expect(fs.readFileSync(metadataPath, 'utf8')).toBe('previous inventory');
});

test('inventory cannot overwrite the input or output PAGX', () => {
  const file = write(fixture());
  expect(() => finalizePagxFonts({ inputPath: file, outputPath: file, metadataPath: file })).toThrow(/separate output path/);
  expect(fs.readFileSync(file, 'utf8')).toBe(fixture());
});
