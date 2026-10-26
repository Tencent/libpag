// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {execFile} = require('child_process');
const {promisify} = require('util');
const {DOMParser} = require('@xmldom/xmldom');
const {PNG} = require('pngjs');
const {runPagxImport} = require('../dist/lib/pagx-runner');
const exec = promisify(execFile);
const root = path.resolve(__dirname, '../../..');
const pagx = process.env.PAGX_TEST_BIN || path.join(root, 'cmake-build-debug', process.platform === 'win32' ? 'pagx.exe' : 'pagx');
const nativeTest = fs.existsSync(pagx) ? test : test.skip;

nativeTest('returned font resources survive source and import directory cleanup', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-font-lifetime-'));
  try {
    const fontPath = path.join(dir, 'source.ttf');
    const bytes = fs.readFileSync(path.join(root, 'resources/font/NotoSansHebrew-Regular.ttf'));
    fs.writeFileSync(fontPath, bytes);
    const html = '<html><head><style>@font-face{font-family:ResponseAlias;src:url("' +
      fontPath.split(path.sep).join('/') + '");font-weight:400}</style></head>' +
      '<body style="width:120px;height:60px"><span style="position:absolute;left:40px;top:10px;' +
      'font-family:ResponseAlias;font-size:24px;color:#000000">אב</span></body></html>';
    const result = await runPagxImport({pagxBin: pagx, html});
    const doc = new DOMParser().parseFromString(result, 'text/xml');
    const fonts = Array.from(doc.getElementsByTagName('Font'));
    expect(fonts.length).toBeGreaterThan(0);
    for (const font of fonts) {
      const uri = font.getAttribute('file');
      expect(uri).toMatch(/^data:.*;base64,/);
      expect(Buffer.from(uri.slice(uri.indexOf(',') + 1), 'base64')).toEqual(bytes);
    }
    expect(doc.getElementsByTagName('GlyphRun').length).toBe(0);
    expect(doc.getElementsByTagName('Text')[0].getAttribute('fontFamily')).toBe('ResponseAlias');
    fs.unlinkSync(fontPath);
    const moved = path.join(dir, 'relocated');
    fs.mkdirSync(moved);
    const document = path.join(moved, 'result.pagx');
    const image = path.join(moved, 'result.png');
    fs.writeFileSync(document, result);
    const rendered = await exec(pagx, ['render', document, '-o', image], {cwd: moved, timeout: 30000});
    expect(rendered.stderr).not.toMatch(/failed to load font|missing glyph/i);
    const png = PNG.sync.read(fs.readFileSync(image));
    expect([png.width, png.height]).toEqual([120, 60]);
    let painted = 0;
    for (let i = 3; i < png.data.length; i += 4) if (png.data[i] !== 0) painted++;
    expect(painted).toBeGreaterThan(0);
  } finally {
    fs.rmSync(dir, {recursive: true, force: true});
  }
}, 60000);
