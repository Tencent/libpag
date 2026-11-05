// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const http = require('node:http');
const { launchBrowser, newPage } = require('../dist/lib/browser-engine');
const { ConversionTask, ConversionTimeoutError } = require('../dist/lib/conversion-task');
const { runSnapshot } = require('../dist/lib/snapshot-runner');
const { CaptureStore, installResourcePolicy } = require('../dist/lib/resource-policy');
const { PAGX_VIRTUAL_CLOCK_INIT_SCRIPT } = require('../dist/lib/animation-capture');

test.each(['puppeteer', 'playwright'])('%s keeps explicit wait/reveal after preparation timeout and cancels a hard timeout', async engine => {
  const browser = await launchBrowser({ engine });
  const sockets = new Set();
  const server = http.createServer((req, res) => {
    if (req.url === '/stalled.png') { res.writeHead(200, { 'Content-Type': 'image/png' }); res.flushHeaders(); return; }
    res.setHeader('Content-Type', 'text/html');
    res.end('<!doctype html><body style="height:600px"><p id="late">waiting</p><img src="/stalled.png">' +
      '<script>setTimeout(()=>document.querySelector("p").textContent="explicit wait completed",2300)</script>');
  });
  server.on('connection', socket => { sockets.add(socket); socket.on('close', () => sockets.delete(socket)); });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const url = `http://127.0.0.1:${server.address().port}/`;
  // Include cold context creation in the preparation budget. The text still changes
  // after preparation expires, so capturing it requires the explicit wait below.
  const task = new ConversionTask({ totalMs: 15000, settleMs: 2000 });
  try {
    const result = await runSnapshot(browser, url, { task, waitMs: 1500, scrollReveal: true,
      viewportWidth: 320, viewportHeight: 240, inlineIconFonts: false });
    expect(result.html).toContain('explicit wait completed');
    expect(result.readiness.status).toBe('partial');
    expect(result.readiness.reason).toBe('prepare-timeout');
    expect(await task.finish()).toBe(true);
    const hard = new ConversionTask({ totalMs: 2500, settleMs: 2000 });
    try {
      await expect(runSnapshot(browser, url, { task: hard, waitMs: 4000, scrollReveal: true }))
        .rejects.toBeInstanceOf(ConversionTimeoutError);
    } finally { expect(await hard.finish()).toBe(true); }
  } finally {
    await task.finish(); await browser.browser.close();
    for (const socket of sockets) socket.destroy();
    await new Promise(resolve => server.close(resolve));
  }
}, 30000);

test.each(['puppeteer', 'playwright'])('%s coalesces helper fetches and never retries failures across passes', async engine => {
  const browser = await launchBrowser({ engine });
  let hits = 0;
  const server = http.createServer((req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    hits++;
    if (req.url === '/hang') { res.writeHead(200); res.flushHeaders(); return; }
    res.statusCode = 404; res.end('missing');
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const page = await newPage(browser);
  try {
    await page.evaluate(installResourcePolicy);
    await page.evaluate(PAGX_VIRTUAL_CLOCK_INIT_SCRIPT);
    const result = await page.evaluate(async url => {
      const resources = window.__pagxResources;
      resources.configure(2000);
      const attempt = () => resources.fetch(url).then(() => false, () => true);
      const failures = await Promise.all([attempt(), attempt()]);
      failures.push(await attempt());
      resources.configure(80);
      const hanging = url.replace('/missing', '/hang');
      const blocked = () => resources.fetch(hanging).then(() => false, () => true);
      failures.push(await blocked(), await blocked(), resources.timedOut);
      failures.push(await resources.fetch(url + '/new').then(() => false, () => true));
      return failures;
    }, `http://127.0.0.1:${server.address().port}/missing`);
    expect(result).toEqual([true, true, true, true, true, true, true]);
    expect(hits).toBe(2);
  } finally { await page.close(); await browser.browser.close(); server.closeAllConnections(); await new Promise(resolve => server.close(resolve)); }
}, 20000);

test('normal caches accept late resources; sealing only rejects late network writes', () => {
  const cache = new CaptureStore();
  cache.set('early', 1); cache.set('late', 2);
  expect([...cache.keys()]).toEqual(['early', 'late']);
  cache.sealed = true;
  cache.set('late', 3); cache.set('stalled', 4); cache.setLocal('data:font', 5);
  expect([...cache.entries()]).toEqual([['early', 1], ['late', 2], ['data:font', 5]]);
});
