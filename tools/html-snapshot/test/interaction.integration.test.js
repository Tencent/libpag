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
  expect(stdout).not.toContain('data-pagx-intrinsic-width');
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

nativeTest('import replays once and renders all named checkpoints', async () => {
  await run(pagx,['import',input,'--scenario',scenario,'--output-dir',output,'--images','embed','--preview','--overwrite']);
  const manifest = JSON.parse(fs.readFileSync(path.join(output,'manifest.json'),'utf8'));
  expect(manifest.outputs.map(s => s.checkpoint)).toEqual(['initial','advanced']);
  expect(manifest.preview.status).toBe('complete');
  expect(manifest.preview.pages[0].items).toEqual(['initial','advanced']);
  const sheet = PNG.sync.read(fs.readFileSync(path.join(output, manifest.preview.pages[0].file)));
  expect([sheet.width, sheet.height]).toEqual([manifest.preview.pages[0].width, manifest.preview.pages[0].height]);
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

nativeTest('selected checkpoint named preview is safe at scale 2 and on repeated runs', async () => {
  const fixture = path.join(output, 'preview-scale.html'), plan = path.join(output, 'preview-scale.json');
  fs.writeFileSync(fixture, '<!doctype html><body style="margin:0;background:#f00"><button id="next" onclick="document.body.style.background=\'#00f\'">Next</button></body>');
  fs.writeFileSync(plan, JSON.stringify({ version:1, viewport:{width:320,height:200}, steps:[
    {type:'capture',name:'preview'}, {type:'click',target:{css:'#next'}}, {type:'capture',name:'blue'},
  ] }));
  const args = ['import',fixture,'--scenario',plan,'--output-dir',output,'--preview','--scale','2','--overwrite'];
  await run(pagx, args);
  const sourcePath = path.join(output, 'preview-scale.preview.png');
  const original = fs.readFileSync(sourcePath);
  await run(pagx, [...args,'--checkpoint','preview','--preview-columns','4']);
  const manifest = JSON.parse(fs.readFileSync(path.join(output,'manifest.json'),'utf8'));
  expect(manifest.outputs.map(item => item.checkpoint)).toEqual(['preview']);
  expect(manifest.preview.pages).toHaveLength(1);
  expect(manifest.preview.pages[0].items).toEqual(['preview']);
  expect(fs.readFileSync(sourcePath)).toEqual(original);
  const source = PNG.sync.read(original);
  expect([source.width,source.height]).toEqual([640,400]);
  const sheet = PNG.sync.read(fs.readFileSync(path.join(output,manifest.preview.pages[0].file)));
  // PNG dimensions, rather than CSS viewport dimensions or a second --scale, determine the thumbnail.
  expect([sheet.width,sheet.height]).toEqual([384,316]);
});

nativeTest('downloaded snapshot images resolve and render through the native pipeline',async()=>{
  const png=new PNG({width:10,height:10});
  for(let i=0;i<png.data.length;i+=4)png.data.set([255,0,0,255],i);
  const fixture=path.join(output,'image.html'),plan=path.join(output,'image.json');
  fs.writeFileSync(fixture,`<!doctype html><body style="margin:0"><img width="100" height="100" src="data:image/png;base64,${PNG.sync.write(png).toString('base64')}"></body>`);
  fs.writeFileSync(plan,JSON.stringify({version:1,viewport:{width:320,height:200},steps:[{type:'capture',name:'ready'}]}));
  await run(pagx,['import',fixture,'--scenario',plan,'--output-dir',output,'--images','external','--render','--overwrite']);
  const actual=PNG.sync.read(fs.readFileSync(path.join(output,'image.ready.png')));
  expect(Array.from(actual.data.slice((30*320+30)*4,(30*320+30)*4+4))).toEqual([255,0,0,255]);
  expect(Array.from(actual.data.slice((150*320+150)*4,(150*320+150)*4+4))).toEqual([255,255,255,255]);
  expect(fs.readFileSync(path.join(output,'image.ready.pagx'),'utf8')).not.toContain('data:image/');
});

nativeTest('scenario exports visible forms and CSS-painted SVG outlines', async () => {
  const fixture = path.join(output, 'form-svg.html');
  const plan = path.join(output, 'form-svg.json');
  fs.writeFileSync(fixture, `<!doctype html><html><head><style>
    body { margin:0; background:white; font:14px Arial }
    form { position:absolute; left:10px; top:10px; width:200px; height:130px; background:#eee }
    input,textarea,button { display:block; width:180px; margin:8px; font:14px Arial }
    svg { position:absolute; left:230px; top:20px; width:60px; height:60px;
      fill:none !important; stroke:#ff0000 !important; stroke-width:4px !important;
      stroke-linecap:round !important; stroke-linejoin:round !important }
  </style></head><body><form><input value="Captured input"><textarea>Captured draft</textarea>
    <button type="button">Send comment</button></form>
    <svg viewBox="0 0 60 60"><circle cx="30" cy="30" r="20"/></svg></body></html>`);
  fs.writeFileSync(plan, JSON.stringify({ version:1, viewport:{width:320,height:200},
    steps:[{type:'capture',name:'ready'}] }));
  const result = await run(pagx, ['import',fixture,'--scenario',plan,'--output-dir',output,'--render','--keep-snapshot','--overwrite']);
  const subset = fs.readFileSync(path.join(output, 'form-svg.ready.subset.html'), 'utf8');
  const xml = fs.readFileSync(path.join(output, 'form-svg.ready.pagx'), 'utf8');
  for (const text of ['Captured input', 'Captured draft', 'Send comment']) {
    expect(subset).toContain(text);
    expect(xml).toContain(text);
  }
  expect(result.stderr).not.toMatch(/Invalid Line(?:Cap|Join)/);
  const rendered = PNG.sync.read(fs.readFileSync(path.join(output, 'form-svg.ready.png')));
  const pixel = (x,y) => Array.from(rendered.data.slice((y*320+x)*4,(y*320+x)*4+4));
  expect(pixel(260,50)).toEqual([255,255,255,255]); // The circle must stay hollow.
  expect(pixel(260,30)).toEqual([255,0,0,255]); // Its CSS-driven outline must survive.
  expect(pixel(14,135)).toEqual([238,238,238,255]); // The form's painted box survives too.
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
