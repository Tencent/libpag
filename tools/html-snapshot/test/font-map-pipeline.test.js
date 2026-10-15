// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const fs = require('fs'), os = require('os'), path = require('path');
const { runHtmlToPagx } = require('../dist/lib/pipeline');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'font-map-pipeline-')); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });
const fontMap = [{ from: { 'font-family': '"PingFang SC", sans-serif', 'font-weight': '600', 'font-style': 'normal' },
  to: { 'font-family': 'Source Han Sans CN', 'font-weight': '600', 'font-style': 'normal', 'font-weight-style': 'Bold' } }];
test('maps after embedding with PNG disabled and does not need ids or a faces sidecar', async () => {
  const log = path.join(dir, 'calls.jsonl');
  const fixture = '<pagx width="100" height="40"><Layer><Text text="A" fontFamily="Linux Fallback" fontStyle="SemiBold" data-html-font-request="{&quot;families&quot;:[&quot;PingFang SC&quot;,&quot;sans-serif&quot;],&quot;weight&quot;:600,&quot;style&quot;:&quot;normal&quot;}"><GlyphRun font="@f" glyphs="1"/></Text></Layer></pagx>';
  const bin = path.join(dir, 'fake-pagx');
  fs.writeFileSync(bin, '#!/usr/bin/env node\n' + `
const fs=require('fs'),args=process.argv.slice(2),command=args[0];
fs.appendFileSync(${JSON.stringify(log)},JSON.stringify(args)+'\\n');
if(command==='import') {
  if(!args.includes('--font-request-metadata')) process.exit(2);
  if(!fs.existsSync(args[args.indexOf('--output')+1].replace(/\\.pagx$/,'.fonts.json'))) process.exit(5);
  fs.writeFileSync(args[args.indexOf('--output')+1],${JSON.stringify(fixture)});
} else if(command==='embed') {
  const content=fs.readFileSync(args[1],'utf8');
  if(!content.includes('fontFamily="Linux Fallback"')) process.exit(3);
  fs.writeFileSync(args[1],content.replace('<GlyphRun','<GlyphRun data-embedded="yes"'));
} else if(command==='render') process.exit(4);
`, { mode: 0o755 });
  const input = path.join(dir, 'page.html'); fs.writeFileSync(input, '<html/>');
  const result = await runHtmlToPagx({ input, scriptDir: dir, outputDir: path.join(dir, 'out'), pagxBin: bin,
    embedFonts: true, doRender: false, fontMap,
    snapshotImpl: async args => {
      fs.writeFileSync(args.output, `<html><body><span data-pagx-font-request='{"families":["PingFang SC","sans-serif"],"weight":600,"style":"normal"}'>A</span></body></html>`);
      fs.mkdirSync(args.fontDir, { recursive: true });
      fs.writeFileSync(args.fontManifest, '');
      expect(args.fontFaces).toBeUndefined();
      return { code: 0, stdout: '', stderr: '', durationMs: 0 };
    } });
  expect(fs.readFileSync(log, 'utf8').trim().split('\n').map(line => JSON.parse(line)[0])).toEqual(['import', 'resolve', 'embed']);
  expect(result.png).toBeNull();
  expect(result.fontMapping).toEqual({ replaced: 1, unmatched: 0, missingRequests: 0 });
  expect(JSON.parse(fs.readFileSync(result.fontMetadataPath, 'utf8'))).toEqual({
    version: 1, fonts: [{ families: ['PingFang SC', 'sans-serif'], weight: 600, weightName: 'SemiBold', style: 'normal' }], missingRequests: 0,
  });
  const output = fs.readFileSync(result.pagx, 'utf8');
  expect(output).toContain('fontFamily="Source Han Sans CN" fontStyle="Bold"');
  expect(output).toContain('data-embedded="yes"');
  expect(output).not.toContain('data-html-font-request');
  expect(output).not.toContain('id=');
});
test.each([{ doResolve: false, embedFonts: true }, { embedFonts: false }])('rejects options that could replace before embedding: %j', async options => {
  const input = path.join(dir, 'page.html'); fs.writeFileSync(input, '<html/>');
  await expect(runHtmlToPagx({ input, scriptDir: dir, outputDir: dir, fontMap, ...options })).rejects.toMatchObject({ code: 'UNSUPPORTED_MAPPING_OPTIONS' });
});

test.each([
  { doResolve: false },
  { doRender: false },
  { doRender: false, embedFonts: true },
  { doResolve: false, keepSubsetHtml: false },
  {},
])('exports original fonts without a mapping table: %j', async options => {
  const request = weight => JSON.stringify({ families: ['Missing Source', 'sans-serif'], weight, style: 'normal' })
    .replace(/"/g, '&quot;');
  const fixture = '<pagx width="100" height="40"><Layer>' + [650, 700, 650].map(weight =>
    `<Text text="same" fontFamily="Linux Fallback" fontStyle="Bold" data-html-font-request="${request(weight)}"/>`).join('') +
    '</Layer></pagx>';
  const calls = path.join(dir, 'calls.jsonl');
  const bin = path.join(dir, 'fake-pagx');
  fs.writeFileSync(bin, '#!/usr/bin/env node\n' + `
const fs=require('fs'),args=process.argv.slice(2),command=args[0];
fs.appendFileSync(${JSON.stringify(calls)},JSON.stringify(args)+'\\n');
if(command==='import') {
  if(!args.includes('--font-request-metadata')) process.exit(2);
  if(!fs.existsSync(args[args.indexOf('--output')+1].replace(/\\.pagx$/,'.fonts.json'))) process.exit(5);
  fs.writeFileSync(args[args.indexOf('--output')+1],${JSON.stringify(fixture)});
} else if(command==='render') {
  if(fs.readFileSync(args[1],'utf8').includes('data-html-font-request')) process.exit(3);
  fs.writeFileSync(args[args.indexOf('--output')+1],'PNG');
}
`, { mode: 0o755 });
  const input = path.join(dir, 'page.html');
  fs.writeFileSync(input, '<html/>');
  const result = await runHtmlToPagx({ input, scriptDir: dir, outputDir: path.join(dir, 'out'),
    pagxBin: bin, ...options, snapshotImpl: async args => {
      fs.writeFileSync(args.output, '<html><body>' + [650, 700, 650].map(weight =>
        `<span data-pagx-font-request="${request(weight)}">same</span>`).join('') + '</body></html>');
      if(args.fontManifest) fs.writeFileSync(args.fontManifest, '');
      return { code: 0, stdout: '', stderr: '', durationMs: 0 };
    } });
  expect(result.fontMetadataPath).toBe(path.join(dir, 'out', 'page.fonts.json'));
  expect(JSON.parse(fs.readFileSync(result.fontMetadataPath, 'utf8'))).toEqual({ version: 1, fonts: [
    { families: ['Missing Source', 'sans-serif'], weight: 650, weightName: '650', style: 'normal' },
    { families: ['Missing Source', 'sans-serif'], weight: 700, weightName: 'Bold', style: 'normal' },
  ], missingRequests: 0 });
  expect(result.fontMapping).toBeUndefined();
  const pagx = fs.readFileSync(result.pagx, 'utf8');
  expect(pagx).not.toContain('data-html-font-request');
  expect(pagx).toContain('fontFamily="Linux Fallback" fontStyle="Bold"');
  expect(pagx).not.toContain('id=');
  const commands = fs.readFileSync(calls, 'utf8').trim().split('\n').map(line => JSON.parse(line)[0]);
  expect(commands).toEqual(options.doResolve === false ? ['import']
    : ['import', 'resolve', ...(options.embedFonts ? ['embed'] : []), ...(options.doRender === false ? [] : ['render'])]);
  if(options.keepSubsetHtml === false) expect(fs.existsSync(result.subsetHtml)).toBe(false);
});

test('publishes the snapshot inventory even when native import fails', async () => {
  const input = path.join(dir, 'page.html');
  fs.writeFileSync(input, '<html/>');
  const bin = path.join(dir, 'failing-pagx');
  fs.writeFileSync(bin, '#!/usr/bin/env node\nprocess.exit(1);', { mode: 0o755 });
  const outputDir = path.join(dir, 'out');
  await expect(runHtmlToPagx({ input, scriptDir: dir, outputDir, pagxBin: bin,
    snapshotImpl: async args => {
      fs.writeFileSync(args.output, `<body><span data-pagx-font-request='{"families":["Snapshot Only"],"weight":650,"style":"normal"}'>A</span></body>`);
      return { code: 0, stdout: '', stderr: '', durationMs: 0 };
    },
  })).rejects.toMatchObject({ step: 'pagx import' });
  expect(JSON.parse(fs.readFileSync(path.join(outputDir, 'page.fonts.json'), 'utf8'))).toEqual({
    version: 1, fonts: [{ families: ['Snapshot Only'], weight: 650, weightName: '650', style: 'normal' }], missingRequests: 0,
  });
});
