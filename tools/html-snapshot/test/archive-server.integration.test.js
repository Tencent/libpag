// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const { writeZip, readZip } = require('./archive-helpers');
const binary = process.env.PAGX_TEST_BIN || path.resolve(__dirname, '../../../cmake-build-debug/pagx');
const nativeTest = fs.existsSync(binary) ? test : test.skip;
jest.setTimeout(90000);

nativeTest('HTTP ZIP upload returns portable artifacts, rejects invalid archives, and cancels disconnected work', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-server-test-'));
  const child = spawn(process.execPath, ['-e', `require('./server').startServer({host:'127.0.0.1',port:0,browserEngine:'puppeteer',maxBodyBytes:1048576,pagxBin:${JSON.stringify(binary)}})`],
    { cwd: path.resolve(__dirname, '..'), env: { ...process.env, HTML_SNAPSHOT_NO_AUTOSCROLL: '1',
      HTML_SNAPSHOT_ARCHIVE_MAX_CONCURRENT: '1', HTML_SNAPSHOT_ARCHIVE_TIMEOUT_MS: '30000' } });
  let logs = '';
  const started = new Promise((resolve, reject) => {
    const data = value => { logs += value; const match = logs.match(/listening on (http:\/\/[^\s]+)/); if (match) resolve(match[1]); };
    child.stdout.on('data', data); child.stderr.on('data', data);
    child.once('error', reject); child.once('exit', () => reject(new Error(logs)));
  });
  try {
    const origin = await started;
    const file = await writeZip(path.join(directory, 'input.zip'), {
      'site/index.html': '<body style="width:200px;height:100px">HTTP home</body>',
      'site/nested/other.htm': '<body style="width:200px;height:100px">HTTP other</body>',
    });
    const body = fs.readFileSync(file);
    const post = (data, query = '', signal) => fetch(origin + '/snapshot-archive' + query, {
      method: 'POST', headers: { 'Content-Type': 'application/zip' }, body: data, signal,
    });
    const ready = async () => {
      for (let i = 0; i < 100; i++) {
        if ((await (await fetch(origin + '/health')).json()).activeRequests === 0) return;
        await new Promise(resolve => setTimeout(resolve, 50));
      }
      throw new Error('archive request did not finish cleanup: ' + logs);
    };
    const result = await post(body, '?root=site&format=both&render=true&waitMs=0');
    expect(result.status).toBe(200); expect(result.headers.get('content-type')).toBe('application/zip');
    expect(result.headers.get('x-snapshot-pages')).toBe('2');
    const output = path.join(directory, 'output.zip'); fs.writeFileSync(output, Buffer.from(await result.arrayBuffer()));
    const entries = await readZip(output);
    const manifest = JSON.parse(entries.get('import-manifest.json'));
    expect(manifest.status).toBe('complete');
    for (const item of manifest.outputs) {
      expect(entries.get(item.pagx).toString()).toContain('HTTP ');
      expect(entries.has(item.png)).toBe(true);
      expect(entries.get(item.source + '.subset.html').toString()).not.toContain('127.0.0.1');
    }
    expect([...entries.keys()].some(name => name.endsWith('.import.json'))).toBe(false);
    await ready();
    expect((await post('not a ZIP')).status).toBe(400); await ready();
    expect((await post(Buffer.alloc(1048577))).status).toBe(413); await ready();
    const controller = new AbortController();
    const pending = post(body, '?waitMs=20000', controller.signal).catch(error => error);
    for (let i = 0; i < 100 && !(await (await fetch(origin + '/health')).json()).activeRequests; i++) {
      await new Promise(resolve => setTimeout(resolve, 20));
    }
    expect((await post(body)).status).toBe(429);
    controller.abort(); expect((await pending).name).toBe('AbortError');
    await ready();
    const single = await fetch(origin + '/snapshot', { method: 'POST', body: '<body>still available</body>' });
    expect(single.status).toBe(200); expect(await single.text()).toContain('still available');
    expect(logs).not.toMatch(/cleanup did not complete/);
  } finally {
    if (child.exitCode === null) {
      const exited = new Promise(resolve => child.once('exit', resolve)); child.kill('SIGTERM'); await exited;
    }
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
