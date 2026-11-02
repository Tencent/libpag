// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.
'use strict';

const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const { captureCurrentPage, collectCurrentState } = require('../dist/lib/current-state');
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

test('reports unsupported control appearance and retains the surrounding content', async () => {
  const page = await newPage(source, { viewport: { width:320, height:200 } });
  try {
    await page.setContent('<input type="checkbox" checked><p>Editable content</p>');
    const result = await captureCurrentPage(page, { mirror });
    expect(result.html).toContain('Editable content');
    expect(result.warnings).toEqual(expect.arrayContaining([expect.stringContaining('checkbox control')]));
  } finally { await page.close(); }
});

test('the read-only collector does not invoke custom element constructors', async () => {
  const page = await newPage(source,{viewport:{width:320,height:200}});
  try {
    await page.setContent('<!doctype html><custom-box></custom-box>');
    await page.evaluate(()=>{
      window.constructed=0;
      customElements.define('custom-box',class extends HTMLElement {
        constructor(){super();window.constructed++}
      });
    });
    const before=await page.evaluate(()=>window.constructed);
    await page.evaluate(collectCurrentState);
    expect(await page.evaluate(()=>window.constructed)).toBe(before);
  } finally {await page.close();}
});

test('document extent keeps the current materialized document and root scroll intact',async()=>{
  const page=await newPage(source,{viewport:{width:320,height:200}});
  try {
    await page.setContent('<!doctype html><body style="margin:0;height:1200px;background:#abc"><div style="position:absolute;top:1000px">Bottom</div></body>');
    await page.evaluate(()=>scrollTo(0,600));
    const result=await captureCurrentPage(page,{mirror,extent:'document'});
    expect(result).toMatchObject({width:320,height:1200});
    expect(await page.evaluate(()=>scrollY)).toBe(600);
    expect(result.html).toContain('Bottom');
    await page.evaluate(()=>document.body.style.height='20000px');
    await expect(captureCurrentPage(page,{mirror,extent:'document'})).rejects.toThrow('capture limit');
  } finally {await page.close();}
});

test('missing CSS resources are omitted with a warning while text is retained',async()=>{
  const page=await newPage(source,{viewport:{width:320,height:200}});
  try {
    await page.setContent('<!doctype html><div style="width:100px;height:100px;background-image:url(http://127.0.0.1:1/image.png)">Retained text</div>');
    const result=await captureCurrentPage(page,{mirror});
    expect(result.warnings).toContain('Uncaptured resource omitted: http://127.0.0.1:1/image.png');
    expect(result.html).toContain('Retained text');
    expect(result.html).not.toContain('http://127.0.0.1:1/image.png');
  } finally {await page.close();}
});

test('propagates a translucent body background across the canvas only once',async()=>{
  const page=await newPage(source,{viewport:{width:320,height:200}});
  const preview=await newPage(mirror,{viewport:{width:320,height:200}});
  try {
    await page.setContent('<!doctype html><body style="margin:20px;background:rgba(255,0,0,.5)"><div style="height:30px">Text</div></body>');
    const before=PNG.sync.read(Buffer.from(await page.screenshot()));
    const result=await captureCurrentPage(page,{mirror});
    await preview.setContent(result.html);
    const after=PNG.sync.read(Buffer.from(await preview.screenshot()));
    for(const offset of [(10*320+10)*4,(45*320+150)*4,(150*320+150)*4]) {
      expect(Array.from(after.data.slice(offset,offset+4))).toEqual(Array.from(before.data.slice(offset,offset+4)));
    }
  } finally {await page.close();await preview.close();}
});
