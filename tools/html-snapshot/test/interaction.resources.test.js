// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../dist/lib/browser-engine');
const { runScenario } = require('../dist/lib/interaction');
jest.setTimeout(60000);
let engine, server, origin, dir, hits;
const colored = color => {
  const png = new PNG({width:8,height:8});
  for(let i=0;i<png.data.length;i+=4) png.data.set([...color,255],i);
  return PNG.sync.write(png);
};
const red=colored([255,0,0]), blue=colored([0,0,255]);
const font = fs.readFileSync(path.resolve(__dirname,'../../../resources/html/corpus/cli/spec/glyph_run/fonts/font_f0.woff2'));
beforeAll(async () => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(),'state-assets-'));
  hits = [];
  server = http.createServer((req,res) => {
    if(req.url === '/') {
      res.setHeader('Content-Type','text/html');
      res.end(`<html><body><button id="load">Load</button><button id="next">Next</button>
        <img id="image" style="display:none;width:80px;height:80px"><span id="text">Ready</span>
        <script>
          load.onclick=async()=>{
            const link=document.createElement('link');link.rel='stylesheet';link.href='/css/app.css';document.head.append(link);
            image.style.display='block';image.onload=()=>text.textContent='red';
            image.src=location.origin.replace('127.0.0.1','localhost')+'/image.png';
          };
          next.onclick=()=>{image.onload=()=>text.textContent='blue';image.src='';image.src=location.origin.replace('127.0.0.1','localhost')+'/image.png';};
        </script></body></html>`);
      return;
    }
    if(req.url === '/favicon.ico') { res.writeHead(404); res.end(); return; }
    hits.push({url:req.url,authorized:req.headers['x-session']==='test'});
    if(req.headers['x-session'] !== 'test') {res.writeHead(403);res.end();return;}
    res.setHeader('Cache-Control','no-store');
    if(req.url === '/css/app.css') {
      res.setHeader('Content-Type','text/css');
      res.end('@font-face{font-family:StateFont;src:url(../font.woff2)}#text{font:24px StateFont}');
    } else if(req.url === '/font.woff2') {
      res.setHeader('Content-Type','font/woff2');res.end(font);
    } else if(req.url === '/image.png') {
      res.setHeader('Content-Type','image/png');res.end(hits.filter(h=>h.url === '/image.png').length===1 ? red : blue);
    } else {res.writeHead(404);res.end();}
  });
  await new Promise(resolve=>server.listen(0,resolve));
  origin=`http://127.0.0.1:${server.address().port}`;
  engine=await launchBrowser();
});
afterAll(async()=>{
  if(engine) await engine.browser.close();
  if(server) await new Promise(resolve=>server.close(resolve));
  if(dir) fs.rmSync(dir,{recursive:true,force:true});
});

test('captures lazy authenticated assets, relative font URLs and changed image bytes at each checkpoint',async()=>{
  const results=await runScenario(engine,origin,{version:1,viewport:{width:640,height:480},steps:[
    {type:'click',target:{css:'#load'}},
    {type:'waitFor',target:{css:'#text'},text:'red'},
    {type:'capture',name:'red'},
    {type:'click',target:{css:'#next'}},
    {type:'waitFor',target:{css:'#text'},text:'blue'},
    {type:'capture',name:'blue'},
  ]},{headers:[['X-Session','test']],downloadFonts:true,fontDir:path.join(dir,'fonts')});
  expect(results[0].html).toContain(red.toString('base64'));
  expect(results[1].html).toContain(blue.toString('base64'));
  expect(results[1].html).not.toContain(red.toString('base64'));
  expect(results[0].fonts.length).toBeGreaterThan(0);
  expect(fs.existsSync(results[0].fonts[0].path)).toBe(true);
  expect(hits.every(hit=>hit.authorized)).toBe(true);
  expect(hits.filter(hit=>hit.url==='/image.png')).toHaveLength(2);
  expect(hits.filter(hit=>hit.url==='/font.woff2')).toHaveLength(1);
});
