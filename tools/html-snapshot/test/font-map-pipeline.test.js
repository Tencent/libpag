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
      fs.writeFileSync(args.output, '<html/>');
      fs.mkdirSync(args.fontDir, { recursive: true });
      fs.writeFileSync(args.fontManifest, '');
      expect(args.fontFaces).toBeUndefined();
      return { code: 0, stdout: '', stderr: '', durationMs: 0 };
    } });
  expect(fs.readFileSync(log, 'utf8').trim().split('\n').map(line => JSON.parse(line)[0])).toEqual(['import', 'resolve', 'embed']);
  expect(result.png).toBeNull();
  expect(result.fontMapping).toEqual({ replaced: 1, unmatched: 0, missingRequests: 0 });
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
