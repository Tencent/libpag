// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { promisify } = require('node:util');
const { execFile } = require('node:child_process');
const { DOMParser } = require('@xmldom/xmldom');
const { PNG } = require('pngjs');
const exec = promisify(execFile);
const root = path.resolve(__dirname, '../../..');
const pagx = process.env.PAGX_TEST_BIN || path.join(root, 'cmake-build-debug/pagx');
const nativeTest = fs.existsSync(pagx) ? test : test.skip;

nativeTest.each(['puppeteer', 'playwright'])('%s preserves waited text, scroll images and embedded fonts', async engine => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-readiness-'));
  const font = fs.readFileSync(path.join(root, 'resources/font/NotoSansHebrew-Regular.ttf'));
  const image = new PNG({ width: 8, height: 8 });
  for (let i = 0; i < image.data.length; i += 4) image.data.set([240, 30, 20, 255], i);
  const imageBytes = PNG.sync.write(image);
  const hits = new Map();
  const server = http.createServer((req, res) => {
    hits.set(req.url, (hits.get(req.url) || 0) + 1);
    if (req.url === '/font.ttf') { res.setHeader('Content-Type', 'font/ttf'); res.end(font); }
    else if (req.url === '/lazy.png') { res.setHeader('Content-Type', 'image/png'); res.end(imageBytes); }
    else if (req.url === '/') {
      res.setHeader('Content-Type', 'text/html; charset=utf-8');
      res.end(`<!doctype html><style>
        @font-face{font-family:CaptureFont;src:url('/font.ttf')}
        html,body{margin:0;width:320px}body{height:1400px;background:white}
        p{margin:0;font:24px CaptureFont,sans-serif}#late{position:absolute;top:120px}
        #reveal{position:absolute;left:20px;top:1050px;width:80px;height:80px;opacity:0}
        img{display:block;width:80px;height:80px}
      </style><p>שלום</p><p id="late"></p><div id="reveal"><img></div><script>
        setTimeout(()=>document.querySelector('#late').textContent='Ready after wait',100);
        const target=document.querySelector('#reveal');
        const observer=new IntersectionObserver(entries=>{
          if(entries.some(entry=>entry.isIntersecting)){
            target.style.opacity='1';target.firstChild.src='/lazy.png';observer.disconnect();
          }
        });observer.observe(target);
      </script>`);
    } else { res.statusCode = 404; res.end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  try {
    await exec(pagx, ['import', '--input', `http://127.0.0.1:${server.address().port}/`,
      '--output-dir', directory, '--output-name', 'quality', '--html-browser-engine', engine,
      '--html-viewport-width', '320', '--html-viewport-height', '240', '--html-scroll-reveal',
      '--html-wait-ms', '150', '--html-embed-fonts', '--render'],
    { cwd: root, timeout: 45000, maxBuffer: 4 * 1024 * 1024 });
    const xml = fs.readFileSync(path.join(directory, 'quality.pagx'), 'utf8');
    expect(xml).toContain('Ready after wait');
    expect(xml).toContain('שלום');
    const document = new DOMParser().parseFromString(xml, 'application/xml');
    expect(document.documentElement.getAttribute('width')).toBe('320');
    expect(document.documentElement.getAttribute('height')).toBe('1400');
    expect(document.getElementsByTagName('Image').length).toBe(1);
    const fonts = new Set(Array.from(document.getElementsByTagName('Font'), node => '@' + node.getAttribute('id')));
    const runs = Array.from(document.getElementsByTagName('GlyphRun'));
    expect(runs.length).toBeGreaterThan(0);
    for (const run of runs) expect(fonts.has(run.getAttribute('font'))).toBe(true);
    expect(document.getElementsByTagName('Glyph').length).toBeGreaterThan(0);
    expect(document.getElementsByTagName('Animation').length).toBe(0);
    expect(fs.readdirSync(path.join(directory, 'quality.fonts')).length).toBeGreaterThan(0);
    const rendered = PNG.sync.read(fs.readFileSync(path.join(directory, 'quality.png')));
    const offset = (1090 * rendered.width + 60) * 4;
    expect([...rendered.data.subarray(offset, offset + 4)]).toEqual([240, 30, 20, 255]);
    expect(hits.get('/lazy.png')).toBe(1);
    expect(hits.get('/font.ttf')).toBe(1);
  } finally {
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
    fs.rmSync(directory, { recursive: true, force: true });
  }
}, 60000);
