// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { prepareZip } = require('../dist/lib/zip-source');
const { startWebsiteServer } = require('../dist/lib/website-server');
const { writeZip } = require('./archive-helpers');
let dir, source, site;
beforeEach(async () => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'website-server-test-'));
  const zip = await writeZip(path.join(dir, 'site.zip'), { 'index.html': '<html>home</html>',
    'nested/index.htm': '<html>nested</html>', 'assets/中文 #%.mjs': 'export const value = 42;',
    'assets/data.json': '{"value":42}', 'old.html': Buffer.from([0xff, 0xfe, 0x61, 0]), 'empty.txt': '' });
  source = await prepareZip(zip); site = await startWebsiteServer(source);
});
afterEach(async () => { if (site) await site.close(); if (source) await source.dispose(); fs.rmSync(dir, { recursive: true, force: true }); });
function request(url, options = {}) {
  return new Promise((resolve, reject) => {
    const req = http.request(url, options, res => {
      const chunks = []; res.on('data', bytes => chunks.push(bytes));
      res.on('end', () => resolve({ status: res.statusCode, headers: res.headers, bytes: Buffer.concat(chunks) }));
      res.on('error', reject);
    }); req.on('error', reject); req.end();
  });
}

test('serves exact file names, MIME, original bytes and query strings', async () => {
  const script = await request(site.url('assets/中文 #%.mjs') + '?v=1');
  expect(script.status).toBe(200); expect(script.headers['content-type']).toMatch(/javascript/);
  expect(script.bytes.toString()).toBe('export const value = 42;');
  const html = await request(site.url('old.html'));
  expect(html.headers['content-type']).toBe('text/html');
  expect(html.bytes).toEqual(Buffer.from([0xff, 0xfe, 0x61, 0]));
  expect((await request(site.origin + '/')).bytes.toString()).toContain('home');
});

test('redirects directory paths and does not use SPA fallback for missing files', async () => {
  expect(await request(site.origin + '/nested?x=1')).toMatchObject({ status: 308, headers: { location: '/nested/?x=1' } });
  expect((await request(site.origin + '/nested/')).bytes.toString()).toContain('nested');
  expect((await request(site.origin + '/missing.js')).status).toBe(404);
  expect((await request(site.origin + '/assets/')).status).toBe(404);
  expect(() => site.url('../outside')).toThrow(/Unknown/);
});

test('limits methods and host, and never serves files outside the inspected index', async () => {
  fs.writeFileSync(path.join(source.directory, 'secret.txt'), 'not indexed');
  expect((await request(site.origin + '/secret.txt')).status).toBe(404);
  expect((await request(site.origin + '/', { method: 'POST' })).status).toBe(405);
  expect((await request(site.origin + '/', { headers: { Host: 'attacker.example' } })).status).toBe(403);
  for (const name of ['/%2Fetc/passwd', '/%5csecret.txt', '/%00', '/%ZZ']) {
    expect((await request(site.origin + name)).status).toBe(400);
  }
});

test('supports HEAD, byte ranges and empty resources', async () => {
  expect((await request(site.origin + '/index.html', { method: 'HEAD' })).bytes.length).toBe(0);
  const partial = await request(site.origin + '/index.html', { headers: { Range: 'bytes=0-5' } });
  expect(partial.status).toBe(206); expect(partial.bytes.toString()).toBe('<html>');
  expect((await request(site.origin + '/index.html', { headers: { Range: 'bytes=-7' } })).bytes.toString()).toBe('</html>');
  expect((await request(site.origin + '/index.html', { headers: { Range: 'bytes=999-' } })).status).toBe(416);
  expect((await request(site.origin + '/empty.txt')).bytes.length).toBe(0);
});
