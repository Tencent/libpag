// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFile, spawn } = require('child_process');
const { promisify } = require('util');
const { pathToFileURL } = require('url');
const { PNG } = require('pngjs');
const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const exec = promisify(execFile);
const dir = path.resolve(__dirname, '..');
const pagx = path.resolve(dir, '../../cmake-build-debug/pagx');
const input = path.join(__dirname, 'fixtures/interaction.html');
const scenario = path.join(__dirname, 'fixtures/interaction.scenario.json');
const nativeTest = fs.existsSync(pagx) ? test : test.skip;
jest.setTimeout(60000);
let output;
beforeAll(() => { output = fs.mkdtempSync(path.join(os.tmpdir(), "pagx states ' ")); });
afterAll(() => { fs.rmSync(output, {recursive:true,force:true}); });
const run = (bin, args) => exec(bin, args, {cwd:dir, timeout:50000, maxBuffer:16 * 1024 * 1024,
  env:{...process.env, PAGX_HTML_SNAPSHOT:'1'}});

test('snapshot CLI selects one state and keeps stdout pure', async () => {
  const {stdout} = await run(process.execPath, ['snapshot.js',input,'--scenario',scenario,'--checkpoint','advanced','-o','-']);
  expect(stdout).toMatch(/^<!DOCTYPE html>/i);
  expect(stdout).toContain('Result: PAGX');
  expect(stdout).not.toContain('step 1/');
  await expect(run(process.execPath, ['snapshot.js',input,'--scenario',scenario,'-o','-']))
    .rejects.toMatchObject({stderr:expect.stringContaining('Multiple checkpoints')});
});

nativeTest('native import forwards scenario arguments including quoted paths', async () => {
  const copied = path.join(output,"state ' plan.json"); fs.copyFileSync(scenario,copied);
  const file = path.join(output,'native.pagx');
  await run(pagx, ['import','--input',input,'--scenario',copied,'--checkpoint','advanced','--output',file]);
  const xml = fs.readFileSync(file,'utf8');
  expect(xml).toContain('Result: PAGX');
  expect(xml).not.toContain('pagx-capture-mode');
  expect(xml).not.toContain('<Flex');
  await expect(run(pagx,['import','--input',input,'--checkpoint','advanced','--output',file]))
    .rejects.toMatchObject({stderr:expect.stringContaining('--checkpoint requires --scenario')});
});

nativeTest('html2pagx replays once and renders all named checkpoints', async () => {
  await run(process.execPath,['html2pagx',input,'--scenario',scenario,'-o',output,'--pagx-bin',pagx,'--pagx-images','embed']);
  const manifest = JSON.parse(fs.readFileSync(path.join(output,'interaction.captures.json'),'utf8'));
  expect(manifest.outputs.map(s => s.name)).toEqual(['initial','advanced']);
  const initial = fs.readFileSync(path.join(output,'interaction.initial.pagx'),'utf8');
  const advanced = fs.readFileSync(path.join(output,'interaction.advanced.pagx'),'utf8');
  expect(initial).not.toContain('Result: PAGX'); expect(advanced).toContain('Result: PAGX');
  // Compare final native pixels with the original page after real interactions.
  const engine = await launchBrowser();
  try {
    const page = await newPage(engine,{viewport:{width:640,height:480}});
    await page.goto(pathToFileURL(input).href);
    await page.click('#open'); await page.click('#tab'); await page.type('#query','PAGX');
    await page.waitForFunction(() => document.querySelector('#result').textContent === 'Result: PAGX');
    const source = PNG.sync.read(Buffer.from(await page.screenshot()));
    const actual = PNG.sync.read(fs.readFileSync(path.join(output,'interaction.advanced.png')));
    expect([actual.width,actual.height]).toEqual([640,480]);
    let mismatch = 0;
    for (let i=0;i<actual.data.length;i+=4) {
      if ([0,1,2].some(c => Math.abs(actual.data[i+c] - source.data[i+c]) > 35)) mismatch++;
    }
    expect(mismatch / (640*480)).toBeLessThan(0.06);
  } finally { await engine.browser.close(); }
});

test('HTTP accepts a selected checkpoint and rejects ambiguous/invalid scenarios before execution', async () => {
  const child = spawn(process.execPath,['-e',
    `require('./server').startServer({host:'127.0.0.1',port:0,browserEngine:'puppeteer',maxBodyBytes:1048576,pagxBin:${JSON.stringify(pagx)}})`],{cwd:dir});
  let logs = '';
  const listen = new Promise((resolve,reject) => {
    const onData = data => { logs += data; const m=logs.match(/listening on (http:\/\/[^\s]+)/); if(m) resolve(m[1]); };
    child.stdout.on('data',onData); child.stderr.on('data',onData);
    child.once('error',reject); child.once('exit',() => reject(new Error(logs)));
  });
  try {
    const origin = await listen;
    const spec = JSON.parse(fs.readFileSync(scenario,'utf8'));
    const request = options => fetch(origin+'/snapshot',{method:'POST',headers:{'Content-Type':'application/json','Accept':'application/json'},
      body:JSON.stringify({html:fs.readFileSync(input,'utf8'),format:'html',options})});
    const response = await request({scenario:spec,checkpoint:'advanced'});
    const body = await response.json();
    expect(response.status).toBe(200);
    expect(body).toMatchObject({checkpoint:'advanced',width:640,height:480,warnings:[]});
    expect(body.html).toContain('Result: PAGX');
    expect((await request({scenario:spec})).status).toBe(400);
    expect((await request({scenario:spec,checkpoint:'missing'})).status).toBe(400);
    expect((await request({scenario:spec,checkpoint:'advanced',captureAnimations:true})).status).toBe(400);
  } finally {
    const done = new Promise(resolve => child.once('exit',resolve));
    child.kill('SIGTERM'); await done;
  }
});
