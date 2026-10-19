// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.
'use strict';

const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const { captureCurrentPage } = require('../dist/lib/current-state');
const { PNG } = require('pngjs');

jest.setTimeout(60000);
let source;
let mirror;
beforeAll(async () => {
  source = await launchBrowser();
  mirror = await launchBrowser();
});
afterAll(async () => {
  if (source) await source.browser.close();
  if (mirror) await mirror.browser.close();
});

test('captures current values, hover, animation and scrolling without mutating the live page', async () => {
  const page = await newPage(source, { viewport: { width: 640, height: 480 } });
  try {
    await page.setContent(`<!doctype html><html><head><style>
      body { margin:19px; padding:11px; background:#eee; height:1100px; font:16px Arial }
      #panel { position:absolute; top:240px; left:60px; width:300px; height:150px; background:#abc }
      #panel:hover { background:#aaffaa }
      #panel::before { content:""; position:absolute; left:5px; top:5px; width:20px; height:20px; background:red }
      #input { position:absolute; left:30px; top:50px; width:180px; height:32px; border:0; padding:4px; outline:0; background:white }
      #moving { position:absolute; top:420px; left:20px; width:30px; height:30px; background:blue }
      #fixed { position:fixed; right:20px; bottom:20px; width:40px; height:40px; background:#f80 }
      #scroll { position:absolute; top:520px; left:80px; width:140px; height:80px; overflow:auto; background:white }
      #content { width:120px; height:300px; background:linear-gradient(red,blue) }
    </style></head><body><div id="panel"><input id="input" value="old"></div>
      <div id="moving"></div><div id="fixed"></div><div id="scroll"><div id="content"></div></div>
    </body></html>`);
    await page.evaluate(() => {
      const input = document.querySelector('#input');
      input.value = 'entered state'; input.focus();
      window.scrollTo(0, 180);
      document.querySelector('#scroll').scrollTop = 73;
      const animation = document.querySelector('#moving').animate(
        [{ transform: 'translateX(0)' }, { transform: 'translateX(100px)' }],
        { duration: 1000, fill: 'forwards' });
      animation.pause(); animation.currentTime = 600;
      window.captureMutations = 0;
      new MutationObserver(() => window.captureMutations++).observe(document.documentElement,
        { attributes:true, childList:true, subtree:true, characterData:true });
      window.captureBlurs = 0;
      window.addEventListener('blur', () => window.captureBlurs++);
    });
    await page.mouse.move(75, 80);
    const inspect = () => ({ html: document.documentElement.outerHTML,
      scroll: [scrollX, scrollY, document.querySelector('#scroll').scrollTop],
      focus: document.activeElement.id, hover: document.querySelector('#panel').matches(':hover'),
      time: document.getAnimations()[0].currentTime,
      mutations: window.captureMutations, blurs: window.captureBlurs });
    const before = await page.evaluate(inspect);
    const baseline = PNG.sync.read(Buffer.from(await page.screenshot()));
    const result = await captureCurrentPage(page, { mirror });
    expect(await page.evaluate(inspect)).toEqual(before);
    expect(result).toMatchObject({ width:640, height:480, warnings:[] });
    expect(result.html).toContain('entered state');
    expect(result.html).not.toContain('>old<');
    expect(result.html).not.toContain('pagxAnim');
    const preview = await newPage(mirror, { viewport: { width:640, height:480 } });
    try {
      await preview.setContent(result.html);
      const actual = PNG.sync.read(Buffer.from(await preview.screenshot()));
      let mismatch = 0;
      for (let i = 0; i < actual.data.length; i += 4) {
        if ([0,1,2].some(c => Math.abs(actual.data[i+c] - baseline.data[i+c]) > 20)) mismatch++;
      }
      expect(mismatch / (640 * 480)).toBeLessThan(0.025);
    } finally { await preview.close(); }
    // Capture did not replace event handlers or prevent the next interaction.
    await page.type('#input', '!');
    expect(await page.$eval('#input', el => el.value)).toBe('entered state!');
  } finally { await page.close(); }
});

test('reports unsupported content and permits an explicit composited viewport fallback', async () => {
  const page = await newPage(source, { viewport: { width:320, height:200 } });
  try {
    await page.setContent('<input type="checkbox" checked>');
    await expect(captureCurrentPage(page, { mirror })).rejects.toThrow('checkbox control');
    const result = await captureCurrentPage(page, { mirror, unsupported:'rasterize' });
    expect(result.html).toContain('data:image/png;base64,');
    expect(result.warnings).toHaveLength(1);
  } finally { await page.close(); }
});
