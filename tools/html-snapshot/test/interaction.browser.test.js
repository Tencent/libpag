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
  ]}, {onCheckpoint})).rejects.toThrow('Condition timed out');
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
