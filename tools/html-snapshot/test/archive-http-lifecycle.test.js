// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
jest.mock('../dist/lib/archive-import', () => ({ preflightArchiveImport: jest.fn(), runArchiveImport: jest.fn() }));
const { preflightArchiveImport, runArchiveImport } = require('../dist/lib/archive-import');
const { handleArchiveHttp } = require('../dist/lib/archive-http');
const { HttpError } = require('../dist/lib/http-utils');
const { readZip } = require('./archive-helpers');
let server, origin, context, work, error, savedEnv;
beforeEach(async () => {
  jest.resetAllMocks();
  savedEnv = { ...process.env };
  delete process.env.HTML_SNAPSHOT_ARCHIVE_ALLOW_ORIGINS;
  delete process.env.HTML_SNAPSHOT_ARCHIVE_TIMEOUT_MS;
  error = undefined;
  context = { pagxBin: process.execPath, scriptDir: path.resolve(__dirname, '..'), browserEngine: 'puppeteer',
    maxBodyBytes: 32, log: jest.fn(), readBody: jest.fn(async req => {
      const chunks = [];
      for await (const chunk of req) chunks.push(chunk);
      const bytes = Buffer.concat(chunks);
      if (bytes.length > context.maxBodyBytes) throw new HttpError(413, 'body too large');
      return bytes;
    }) };
  preflightArchiveImport.mockResolvedValue({});
  runArchiveImport.mockImplementation(async opts => {
    fs.mkdirSync(path.join(opts.outputDir, 'nested'), { recursive: true });
    fs.writeFileSync(path.join(opts.outputDir, 'nested/page.html.pagx'), '<pagx/>');
    fs.writeFileSync(path.join(opts.outputDir, 'import-manifest.json'), '{"status":"partial"}');
    fs.writeFileSync(path.join(opts.outputDir, 'nested/page.html.import.json'), 'internal');
    return { total: 1, manifest: { status: 'partial' } };
  });
  server = http.createServer((req, res) => {
    work = handleArchiveHttp(req, res, context).catch(reason => {
      error = reason;
      if (!res.headersSent && !res.destroyed) { res.writeHead(reason.status || 500); res.end(reason.message); }
    });
  });
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(0, '127.0.0.1', resolve); });
  origin = `http://127.0.0.1:${server.address().port}`;
});
afterEach(async () => {
  await work;
  await new Promise(resolve => { server.close(resolve); server.closeAllConnections(); });
  for (const [opts] of preflightArchiveImport.mock.calls) expect(fs.existsSync(path.dirname(opts.input))).toBe(false);
  process.env = savedEnv;
});
const post = (query = '', options = {}) => fetch(origin + '/snapshot-archive' + query, {
  method: 'POST', headers: { 'Content-Type': 'application/zip' }, body: 'archive bytes', ...options,
});

test('streams final artifacts with status headers and removes all upload files afterward', async () => {
  process.env.HTML_SNAPSHOT_ARCHIVE_ALLOW_ORIGINS = 'https://cdn.example/';
  const response = await post('?root=site&format=both&render=true&waitMs=0', {
    headers: { 'Content-Type': ' Application/X-Zip-Compressed; charset=binary' },
  });
  expect(response.status).toBe(200);
  expect(response.headers.get('x-snapshot-status')).toBe('partial');
  expect(response.headers.get('x-snapshot-pages')).toBe('1');
  expect(response.headers.get('content-disposition')).toContain('imported-webpages.zip');
  const bytes = Buffer.from(await response.arrayBuffer());
  expect(Number(response.headers.get('content-length'))).toBe(bytes.length);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-http-result-'));
  try {
    const file = path.join(dir, 'response.zip'); fs.writeFileSync(file, bytes);
    expect([...(await readZip(file)).keys()]).toEqual(['import-manifest.json', 'nested/page.html.pagx']);
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
  expect(runArchiveImport).toHaveBeenCalledWith(expect.objectContaining({
    archiveRoot: 'site', keepSubsetHtml: true, doRender: true, waitMs: 0, allowOrigins: ['https://cdn.example'],
  }));
});

test.each([
  [{ headers: { 'Content-Type': 'text/plain' } }, '', 415],
  [{ headers: {} }, '', 415],
  [{}, '?format=html', 400],
  [{ body: 'a'.repeat(33) }, '', 413],
])('rejects invalid uploads before conversion: %j %s', async (options, query, status) => {
  const response = await post(query, options);
  expect(response.status).toBe(status); await response.text();
  await work;
  expect(runArchiveImport).not.toHaveBeenCalled();
});

test('missing native binaries fail before reading the upload', async () => {
  context.pagxBin = path.join(os.tmpdir(), 'missing-pagx-archive-binary');
  const response = await post();
  expect(response.status).toBe(503); await response.text();
  expect(context.readBody).not.toHaveBeenCalled();
});

test('invalid archives return 400, while conversion failures preserve their server error', async () => {
  preflightArchiveImport.mockRejectedValueOnce(new Error('invalid ZIP'));
  const invalid = await post();
  expect(invalid.status).toBe(400); expect(await invalid.text()).toBe('invalid ZIP');
  await work;
  expect(runArchiveImport).not.toHaveBeenCalled();
  runArchiveImport.mockRejectedValueOnce(new Error('native conversion failed'));
  const failed = await post();
  expect(failed.status).toBe(500); expect(await failed.text()).toBe('native conversion failed');
});

test('parent cancellation takes precedence over a preflight error', async () => {
  const controller = new AbortController(); context.signal = controller.signal;
  preflightArchiveImport.mockImplementationOnce(async () => {
    controller.abort(new Error('server stopping'));
    throw new Error('validation interrupted');
  });
  const response = await post(); await response.text(); await work;
  expect(error.message).toBe('server stopping');
  expect(error.status).not.toBe(400);
  expect(runArchiveImport).not.toHaveBeenCalled();
});

test('an already cancelled owner never reads or converts an upload', async () => {
  const controller = new AbortController(); controller.abort(new Error('already stopped')); context.signal = controller.signal;
  const response = await post(); await response.text(); await work;
  expect(error.message).toBe('already stopped');
  expect(context.readBody).not.toHaveBeenCalled();
});

test('a disconnected client cancels conversion and waits for its finalizer before deleting inputs', async () => {
  const controller = new AbortController();
  let entered, release;
  const started = new Promise(resolve => { entered = resolve; });
  const finalized = new Promise(resolve => { release = resolve; });
  runArchiveImport.mockImplementationOnce(async opts => {
    entered(opts);
    await new Promise(resolve => opts.signal.addEventListener('abort', resolve, { once: true }));
    await finalized;
    expect(fs.existsSync(opts.input)).toBe(true);
    opts.signal.throwIfAborted();
  });
  const response = post('', { signal: controller.signal }).catch(reason => reason);
  const opts = await started;
  try {
    controller.abort(); expect((await response).name).toBe('AbortError');
    if (!opts.signal.aborted) await new Promise(resolve => opts.signal.addEventListener('abort', resolve, { once: true }));
    expect(fs.existsSync(opts.input)).toBe(true);
  } finally { release(); await work; }
  expect(error.message).toBe('Archive client disconnected');
});

test('the archive deadline interrupts an unfinished upload', async () => {
  process.env.HTML_SNAPSHOT_ARCHIVE_TIMEOUT_MS = '40';
  let entered;
  const started = new Promise(resolve => { entered = resolve; });
  const read = context.readBody.getMockImplementation();
  context.readBody.mockImplementation(req => { entered(); return read(req); });
  const request = http.request(origin + '/snapshot-archive', { method: 'POST',
    headers: { 'Content-Type': 'application/zip', 'Content-Length': '100' } });
  const closed = new Promise(resolve => { request.on('error', resolve); request.on('close', resolve); });
  request.write('incomplete');
  await started; await closed; await work;
  expect(error).toMatchObject({ status: 408, stage: 'archive-upload' });
  expect(runArchiveImport).not.toHaveBeenCalled();
});
