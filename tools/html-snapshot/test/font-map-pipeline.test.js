// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { runHtmlToPagx } = require('../dist/lib/pipeline');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'font-map-pipeline-')); });
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));
const availableFonts = [{ fontName: { family: 'SOURCE', style: 'SemiBold' } }];
const html = `<html><body><span style="font-family:Source,sans-serif;font-weight:650">A</span></body></html>`;
function setup() {
  const input = path.join(dir, 'page.html'); fs.writeFileSync(input, '<html/>');
  const calls = path.join(dir, 'calls.jsonl');
  const bin = path.join(dir, 'fake-pagx');
  fs.writeFileSync(bin, '#!/usr/bin/env node\n' + `
const fs=require('fs'),args=process.argv.slice(2),command=args[0];
fs.appendFileSync(${JSON.stringify(calls)},JSON.stringify(args)+'\\n');
if(command==='import') {
  const html=fs.readFileSync(args[args.indexOf('--input')+1],'utf8');
  fs.writeFileSync(${JSON.stringify(path.join(dir, 'import-input.html'))},html);
  if(args.includes('--font-request-metadata')) process.exit(2);
  const out=args[args.indexOf('--output')+1];
  if(fs.existsSync(out.replace(/\\.pagx$/,'.fonts.json'))) process.exit(3);
  fs.writeFileSync(out,'<pagx><Layer><Text text="A" fontFamily="From native import"/></Layer></pagx>');
} else if(command==='render') fs.writeFileSync(args[args.indexOf('--output')+1],'PNG');
`, { mode: 0o755 });
  return { input, pagxBin: bin, scriptDir: dir, outputDir: path.join(dir, 'out'),
    snapshotImpl: async args => {
      fs.writeFileSync(args.output, html);
      if (args.fontManifest) fs.writeFileSync(args.fontManifest, '');
      return { code: 0, stdout: '', stderr: '', durationMs: 0 };
    } };
}
test.each([{ doResolve: false }, { doRender: false }, { doRender: false, embedFonts: true }])(
  'maps before native import independently of embedding: %j', async options => {
    const result = await runHtmlToPagx({ ...setup(), ...options, availableFonts });
    const input = fs.readFileSync(path.join(dir, 'import-input.html'), 'utf8');
    expect(input).toBe(html);
    const calls = fs.readFileSync(path.join(dir, 'calls.jsonl'), 'utf8').trim().split('\n').map(JSON.parse);
    const args = calls[0];
    const offset = args.indexOf('--font-replacement');
    expect(args.slice(offset + 1, offset + 6)).toEqual(['Source,sans-serif', '650', 'normal', 'SOURCE', 'SemiBold']);
    expect(input).not.toContain('data-pagx-font-request');
    expect(result.fontMapping).toEqual({ replaced: 1, unmatched: 0, missingRequests: 0 });
    expect(result).not.toHaveProperty('fontMetadataPath');
    expect(fs.existsSync(path.join(path.dirname(result.pagx), 'page.fonts.json'))).toBe(false);
    expect(fs.readFileSync(result.pagx, 'utf8')).toContain('fontFamily="From native import"');
  });

test('without a catalog the snapshot is unchanged and no inventory is published', async () => {
  const result = await runHtmlToPagx({ ...setup(), doResolve: false });
  expect(fs.readFileSync(result.subsetHtml, 'utf8')).toBe(html);
  expect(result.fontMapping).toBeUndefined();
  expect(result).not.toHaveProperty('fontMetadataPath');
  expect(fs.existsSync(path.join(path.dirname(result.pagx), 'page.fonts.json'))).toBe(false);
});

test('does not publish font JSON when native import fails', async () => {
  const options = setup();
  fs.writeFileSync(options.pagxBin, '#!/usr/bin/env node\nprocess.exit(1);', { mode: 0o755 });
  await expect(runHtmlToPagx(options)).rejects.toMatchObject({ step: 'pagx import' });
  expect(fs.existsSync(path.join(options.outputDir, 'page.fonts.json'))).toBe(false);
});

test('rejects malformed catalogs before snapshot or native conversion', async () => {
  const options = setup();
  const snapshot = jest.fn(options.snapshotImpl);
  await expect(runHtmlToPagx({ ...options, snapshotImpl: snapshot, availableFonts: [{}] }))
    .rejects.toThrow(/INVALID_AVAILABLE_FONTS/);
  expect(snapshot).not.toHaveBeenCalled();
  expect(fs.existsSync(path.join(dir, 'calls.jsonl'))).toBe(false);
});
