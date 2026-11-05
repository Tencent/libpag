// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');

test('HTTP deadlines cancel native work, reject invalid output and isolate concurrent/disconnected requests', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-http-deadline-'));
  const native = path.join(directory, 'native.js');
  fs.writeFileSync(native, `#!${process.execPath}
const fs = require('node:fs'), args = process.argv.slice(2);
const input = fs.readFileSync(args[args.indexOf('--input') + 1], 'utf8');
if (input.includes('bad-output')) fs.writeFileSync(args[args.indexOf('--output') + 1], '<html/>');
else setInterval(() => {}, 1000);
`);
  fs.chmodSync(native, 0o755);
  const child = spawn(process.execPath, ['-e',
    `require('./server').startServer({host:'127.0.0.1',port:0,browserEngine:'puppeteer',maxBodyBytes:1048576,pagxBin:${JSON.stringify(native)}})`],
  { cwd: path.resolve(__dirname, '..'), env: { ...process.env, HTML_SNAPSHOT_TIMEOUT_MS: '6000', HTML_SNAPSHOT_SETTLE_TIMEOUT_MS: '4000' } });
  let logs = '';
  const started = new Promise((resolve, reject) => {
    const data = value => { logs += value; const match = logs.match(/listening on (http:\/\/[^\s]+)/); if (match) resolve(match[1]); };
    child.stdout.on('data', data); child.stderr.on('data', data);
    child.once('error', reject); child.once('exit', () => reject(new Error(logs)));
  });
  try {
    const origin = await started;
    const request = (format, text, options = {}, signal) => fetch(origin + '/snapshot', {
      method: 'POST', headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ html: `<!doctype html><body><p>${text}</p></body>`, format,
        options: { waitMs: 0, inlineIconFonts: false, ...options } }), signal,
    });
    const [blocked, unaffected] = await Promise.all([request('pagx', 'stalled native'), request('html', 'another request')]);
    expect(blocked.status).toBe(408);
    expect((await blocked.json()).error).toMatch(/timed out during import/);
    expect(unaffected.status).toBe(200);
    const body = await unaffected.json();
    expect(body.html).toContain('another request');
    expect(['ready', 'partial']).toContain(body.readiness.status);
    expect(unaffected.headers.get('X-Snapshot-Readiness')).toBe(body.readiness.status);
    const malformed = await request('pagx', 'bad-output');
    expect(malformed.status).toBe(502);
    expect((await malformed.json()).error).toMatch(/invalid PAGX/);
    const controller = new AbortController();
    const cancelled = request('html', 'disconnect', { waitMs: 10000 }, controller.signal).catch(error => error);
    await new Promise(resolve => setTimeout(resolve, 100));
    controller.abort();
    expect((await cancelled).name).toBe('AbortError');
    expect((await request('html', 'still available')).status).toBe(200);
    const health = await (await fetch(origin + '/health')).json();
    expect(health.activeRequests).toBe(0);
    expect(logs).not.toMatch(/cleanup did not complete/);
  } finally {
    if (child.exitCode === null) {
      const exited = new Promise(resolve => child.once('exit', resolve));
      child.kill('SIGTERM'); await exited;
    }
    fs.rmSync(directory, { recursive: true, force: true });
  }
}, 40000);
