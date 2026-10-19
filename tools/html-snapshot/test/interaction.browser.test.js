// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const path = require('path');
const { pathToFileURL } = require('url');
const { launchBrowser } = require('../dist/lib/browser-engine');
const { runScenario } = require('../dist/lib/interaction');
const url = pathToFileURL(path.join(__dirname, 'fixtures/interaction.html')).href;
jest.setTimeout(60000);
let browser;
beforeAll(async () => { browser = await launchBrowser(); });
afterAll(async () => { if (browser) await browser.browser.close(); });

const scenario = {
  version:1, viewport:{width:640,height:480}, steps:[
    {type:'capture',name:'initial'},
    {type:'click',target:{testId:'open'}},
    {type:'waitFor',target:{css:'#dialog'},state:'visible'},
    {type:'click',target:{css:'#tab'}},
    {type:'fill',target:{css:'#query'},value:'PAGX'},
    {type:'waitFor',target:{css:'#result'},attribute:{name:'data-ready',value:'true'}},
    {type:'capture',name:'advanced'},
    {type:'fill',target:{css:'#query'},value:'next'},
    {type:'waitFor',target:{css:'#result'},text:'Result: next'},
    {type:'capture',name:'next'},
  ],
};

test('runs asynchronous interactions and multiple checkpoints in the same isolated session', async () => {
  const results = await runScenario(browser, url, scenario);
  expect(results.map(r => r.name)).toEqual(['initial','advanced','next']);
  expect(results[0].html).not.toContain('Waiting');
  expect(results[1].html).toContain('Result: PAGX');
  expect(results[2].html).toContain('Result: next');
  expect(results[2].html).not.toContain('Result: PAGX');
});

test('selects a checkpoint and does not execute subsequent steps', async () => {
  const invalidAfter = { ...scenario, steps:scenario.steps.slice(0,7).concat(
    {type:'click',target:{css:'#does-not-exist'}}) };
  const results = await runScenario(browser, url, invalidAfter, {checkpoint:'advanced'});
  expect(results).toHaveLength(1);
  expect(results[0].html).toContain('Result: PAGX');
});

test('fails on ambiguous locators and unmet conditions without producing checkpoints', async () => {
  const onCheckpoint = jest.fn();
  await expect(runScenario(browser, url, {version:1, steps:[
    {type:'click',target:{css:'button'}}, {type:'capture',name:'wrong'},
  ]}, {onCheckpoint})).rejects.toThrow('multiple elements');
  await expect(runScenario(browser, url, {version:1, steps:[
    {type:'waitFor',target:{css:'#dialog'},state:'visible',timeoutMs:100},
    {type:'capture',name:'wrong'},
  ]}, {onCheckpoint})).rejects.toThrow(/timed out/);
  expect(onCheckpoint).not.toHaveBeenCalled();
});

test('supports the Playwright driver and accessible-role locators', async () => {
  const previous = process.env.PLAYWRIGHT_EXECUTABLE_PATH;
  process.env.PLAYWRIGHT_EXECUTABLE_PATH = require('puppeteer').executablePath();
  let engine;
  try {
    engine = await launchBrowser({engine:'playwright'});
    const steps = scenario.steps.slice(1,7).map(s => s.type === 'click' && s.target.testId
      ? {...s,target:{role:'button',name:'Open settings'}} : s);
    const results = await runScenario(engine, url, {...scenario,steps});
    expect(results[0].html).toContain('Result: PAGX');
  } finally {
    if (engine) await engine.browser.close();
    if (previous === undefined) delete process.env.PLAYWRIGHT_EXECUTABLE_PATH;
    else process.env.PLAYWRIGHT_EXECUTABLE_PATH = previous;
  }
});

test('a scenario deadline closes its session and produces no output', async () => {
  const onCheckpoint = jest.fn();
  await expect(runScenario(browser, url, {version:1, timeoutMs:200,
    steps:[{type:'capture',name:'late'}]}, {onCheckpoint})).rejects.toThrow('Scenario timed out');
  expect(onCheckpoint).not.toHaveBeenCalled();
});

test('supports form actions, scoped frame targets, scroll and hover through real inputs', async () => {
  const html = `<!doctype html><body style="height:1200px">
    <input id="entry"><input type="checkbox" id="check"><select id="pick"><option>a</option><option>b</option></select>
    <button id="finish">Finish</button><p id="status"></p>
    <div id="scroller" style="width:150px;height:60px;overflow:auto"><div style="height:300px">Scrollable</div></div>
    <iframe id="frame" srcdoc="<button onclick='parent.document.getElementById(&quot;frame&quot;).style.display=&quot;none&quot;'>Frame action</button>"></iframe>
    <div id="hover" style="width:150px;height:30px">Hover</div>
    <style>#hover:hover::after{content:'hover captured'}</style>
    <script>
      entry.onkeydown=e=>{if(e.key==='Enter')entry.dataset.enter='yes'};
      scroller.onscroll=()=>scroller.dataset.y=String(scroller.scrollTop);
      finish.onclick=()=>{document.querySelector('#status').textContent=entry.value+'/'+check.checked+'/'+pick.value+'/'+entry.dataset.enter;
        check.style.display='none';pick.style.display='none'};
    </script>`;
  const results=await runScenario(browser,'data:text/html,'+encodeURIComponent(html),{version:1,steps:[
    {type:'fill',target:{css:'#entry'},value:'A'},
    {type:'type',target:{css:'#entry'},value:'B'},
    {type:'focus',target:{css:'#entry'}},
    {type:'press',target:{css:'#entry'},key:'Enter'},
    {type:'check',target:{css:'#check'},value:true},
    {type:'select',target:{css:'#pick'},value:'b'},
    {type:'click',target:{css:'#finish'}},
    {type:'waitFor',target:{css:'#status'},text:'AB/true/b/yes'},
    {type:'scroll',target:{css:'#scroller'},y:50},
    {type:'waitFor',target:{css:'#scroller'},attribute:{name:'data-y',value:'50'}},
    {type:'click',target:{frame:'#frame',css:'button'}},
    {type:'waitFor',target:{css:'#frame'},state:'hidden'},
    {type:'hover',target:{css:'#hover'}},
    {type:'capture',name:'form'},
  ]});
  expect(results[0].html).toContain('AB/true/b/yes');
  expect(results[0].html.replace(/<[^>]*>/g,' ').replace(/\s+/g,' ')).toContain('hover captured');
});

test('a capture step deadline includes readiness and does not emit a checkpoint', async () => {
  const onCheckpoint=jest.fn();
  await expect(runScenario(browser,url,{version:1,stableMs:500,steps:[
    {type:'capture',name:'late',timeoutMs:100},
  ]},{onCheckpoint})).rejects.toThrow(/step 1.*timed out/);
  expect(onCheckpoint).not.toHaveBeenCalled();
});
