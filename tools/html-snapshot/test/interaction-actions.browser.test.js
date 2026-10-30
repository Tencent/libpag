// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const { runScenario, waitForCaptureReady } = require('../dist/lib/interaction');
jest.setTimeout(60000);
// Capturing also starts the mirror browser. Use the production capture budget;
// action timeout regressions below still exercise their explicit short deadlines.
const capture = { type: 'capture', name: 'done', timeoutMs: 15000 };

describe.each(['puppeteer', 'playwright'])('%s interaction regressions', engine => {
  let handle;
  beforeAll(async () => {
    // Each driver uses its supported browser build for both source and mirror.
    handle = await launchBrowser({ engine });
  });
  afterAll(async () => {
    if (handle) await handle.browser.close();
  });
  const run = async (html, steps, options = {}) => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'interaction-actions-'));
    try {
      const file = path.join(dir, 'page.html');
      fs.writeFileSync(file, html);
      return await runScenario(handle, pathToFileURL(file).href,
        { version: 1, stableMs: 0, stepTimeoutMs: 3000, steps }, options);
    } finally { fs.rmSync(dir, { recursive: true, force: true }); }
  };

  test('form, frame, scrolling and navigation actions preserve their observed state', async () => {
    const html = `<!doctype html><body style="height:1400px">
      <input id="entry"><input type="checkbox" id="check" checked>
      <select id="pick"><option value="a">a</option><option value="b">b</option></select>
      <div id="scroller" style="width:100px;height:40px;overflow:auto"><div style="height:300px">scroll</div></div>
      <button id="finish">finish</button><p id="status"></p>
      <div id="hover" style="width:200px;height:40px">hover target</div>
      <style>#hover:hover::after{content:'hover observed'}</style>
      <iframe id="frame" srcdoc="<button onclick='parent.document.getElementById(&quot;frame&quot;).remove()'>close frame</button>"></iframe>
      <script>
      entry.onkeydown=e=>{if(e.key==='Enter')entry.dataset.enter='yes'};
      scroller.onscroll=()=>scroller.dataset.y=String(scroller.scrollTop);
      finish.onclick=()=>{statusText=entry.value+'/'+check.checked+'/'+pick.value+'/'+entry.dataset.enter;
        document.querySelector('#status').textContent=statusText;check.style.display='none';pick.style.display='none';location.hash='done'};
      </script>`;
    const results = await run(html, [
      { type: 'fill', target: { css: '#entry' }, value: 'A' },
      { type: 'type', target: { css: '#entry' }, value: 'B' },
      { type: 'focus', target: { css: '#entry' } },
      { type: 'press', target: { css: '#entry' }, key: 'Enter' },
      { type: 'check', target: { css: '#check' }, value: true },
      { type: 'check', target: { css: '#check' }, value: false },
      { type: 'select', target: { css: '#pick' }, value: 'b' },
      { type: 'scroll', target: { css: '#scroller' }, y: 70 },
      { type: 'waitFor', target: { css: '#scroller' }, attribute: { name: 'data-y', value: '70' } },
      { type: 'click', target: { frame: '#frame', css: 'button' } },
      { type: 'waitFor', target: { css: '#frame' }, state: 'detached' },
      { type: 'click', target: { css: '#finish' }, url: '#done' },
      { type: 'waitFor', url: '#done' },
      { type: 'waitFor', target: { css: '#status' }, text: 'AB/false/b/yes' },
      { type: 'scroll', y: 100 }, { type: 'scroll', x: 0, y: 0 },
      { type: 'hover', target: { css: '#hover' } }, capture,
    ]);
    expect(results).toHaveLength(1);
    expect(results[0].url).toContain('#done');
    expect(results[0].html).toContain('AB/false/b/yes');
    expect(results[0].html.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ')).toContain('hover observed');
  });

  test.each([
    ['ambiguous frames', '<iframe></iframe><iframe></iframe>', { type: 'click', target: { frame: 'iframe', css: 'button' } }, /exactly one frame/],
    ['ambiguous conditions', '<div>a</div><div>b</div>', { type: 'waitFor', target: { css: 'div' }, text: 'a' }, /multiple elements/],
    ['unmet URL', '<div>page</div>', { type: 'waitFor', url: '#missing', timeoutMs: 100 }, /timed out/],
  ])('%s fail before emitting a checkpoint and release the context', async (_, html, action, message) => {
    const contexts = () => engine === 'playwright' ? handle.browser.contexts().length : handle.browser.browserContexts().length;
    const before = contexts();
    const onCheckpoint = jest.fn();
    await expect(run(html, [action, capture], { onCheckpoint })).rejects.toThrow(message);
    expect(onCheckpoint).not.toHaveBeenCalled();
    expect(contexts()).toBe(before);
  });

  test.each(['viewport', 'document'])('%s captures continue past missing images and embedded content', async extent => {
    const results = await run(`<!doctype html><body style="font:12px Arial">
      <custom-box style="display:block"><span>Retained child</span></custom-box>
      <img id="broken" width="20" height="20" src="data:image/png;base64,invalid">
      <iframe style="width:40px;height:30px" srcdoc="<p>Embedded</p>"></iframe>
      <button id="next" onclick="document.querySelector('#status').textContent='Updated'">Next</button>
      <p id="status">Initial</p>`, [
      { ...capture, name: 'initial', extent },
      { type: 'click', target: { css: '#next' } },
      { type: 'waitFor', target: { css: '#status' }, text: 'Updated' },
      { ...capture, name: 'updated', extent },
    ]);
    expect(results.map(result => result.name)).toEqual(['initial', 'updated']);
    for (const result of results) {
      expect(result.html).toContain('Retained child');
      expect(result.html).not.toContain('base64,invalid');
      expect(result.warnings).toEqual(expect.arrayContaining([
        expect.stringContaining('img#broken'), expect.stringContaining('iframe'), expect.stringContaining('custom-box'),
      ]));
      expect(result.warnings.length).toBe(new Set(result.warnings).size);
    }
    expect(results[1].html).toContain('Updated');
  });

  test('image decode failures warn while decode and stability deadlines remain errors', async () => {
    const page = await newPage(handle, { viewport: { width: 320, height: 200 } });
    try {
      await page.setContent('<!doctype html><img id="image" width="10" height="10">');
      // Use a valid completed image, then exercise the browser decode failure path.
      await page.evaluate(async () => {
        const img = document.querySelector('#image');
        img.src = 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"/>');
        await img.decode();
        img.decode = () => Promise.reject(new DOMException('Unsupported bytes', 'EncodingError'));
      });
      expect(await waitForCaptureReady(page, 2000, 0)).toEqual(['img#image: image decode failed: Unsupported bytes']);
      await page.evaluate(() => { document.querySelector('#image').decode = () => new Promise(() => {}); });
      await expect(waitForCaptureReady(page, 100, 0)).rejects.toThrow('Image decode timed out');
      await page.evaluate(() => { document.querySelector('#image').remove(); });
      await expect(waitForCaptureReady(page, 100, 500)).rejects.toThrow('Capture readiness timed out');
    } finally { await page.close(); }
  });

  test('consumer failures retain the checkpoint diagnostic and close the source context', async () => {
    const onCheckpoint = jest.fn().mockRejectedValue(new Error('cannot publish checkpoint'));
    await expect(run('<!doctype html><div>ready</div>', [capture], { onCheckpoint }))
      .rejects.toThrow(/step 1.*cannot publish checkpoint/);
    expect(onCheckpoint).toHaveBeenCalledTimes(1);
  });
});
