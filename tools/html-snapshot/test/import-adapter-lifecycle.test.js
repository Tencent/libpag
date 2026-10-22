// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');

jest.mock('../dist/lib/browser-engine', () => ({ launchBrowser: jest.fn() }));
jest.mock('../dist/lib/snapshot-runner', () => ({ runSnapshot: jest.fn() }));
jest.mock('../dist/lib/interaction', () => ({
  ...jest.requireActual('../dist/lib/interaction'), runScenario: jest.fn(),
}));
jest.mock('../dist/lib/contact-sheet', () => ({
  ...jest.requireActual('../dist/lib/contact-sheet'), generateContactSheet: jest.fn(),
}));
const { launchBrowser } = require('../dist/lib/browser-engine');
const { runSnapshot } = require('../dist/lib/snapshot-runner');
const { runScenario } = require('../dist/lib/interaction');
const { generateContactSheet } = require('../dist/lib/contact-sheet');
const { validateImportRequest, runImportAdapter } = require('../dist/lib/import-adapter');
const { withCliLifecycle } = require('../dist/lib/cli-lifecycle');
let dir, request, responseFile, contexts, browser, stderr;
const result = () => ({ html: '<html>captured</html>', width: 32, height: 24 });
const response = () => JSON.parse(fs.readFileSync(responseFile, 'utf8'));
beforeEach(() => {
  jest.resetAllMocks();
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'import-adapter-lifecycle-'));
  const input = path.join(dir, 'page.html');
  fs.writeFileSync(input, '<html/>');
  request = { version: 1, operation: 'capture', workDir: path.join(dir, 'work'),
    inputs: [{ id: 'first', input }], options: {} };
  responseFile = path.join(dir, 'response.json');
  contexts = [];
  const context = async () => {
    const value = { close: jest.fn().mockResolvedValue() };
    contexts.push(value);
    return value;
  };
  browser = { createBrowserContext: jest.fn(context), newContext: jest.fn(context),
    close: jest.fn().mockResolvedValue() };
  launchBrowser.mockResolvedValue({ engine: 'puppeteer', browser });
  runSnapshot.mockResolvedValue(result());
  stderr = jest.spyOn(process.stderr, 'write').mockImplementation(() => true);
});
afterEach(() => {
  stderr.mockRestore();
  fs.rmSync(dir, { recursive: true, force: true });
});

test.each(['puppeteer', 'playwright'])('isolates each %s source and persists usable snapshots', async engine => {
  launchBrowser.mockResolvedValue({ engine, browser });
  request.options = { browserEngine: engine, captureAnimations: true, cookies: ['token=a=b'],
    headers: ['X-Token: abc:123'] };
  request.inputs = [{ id: 'first', input: 'https://example.test/a' },
    { id: 'second', input: 'https://example.test/b' }];
  runSnapshot.mockImplementation(async (handle, url, options) => {
    expect(response().status).toBe('running');
    expect(handle.browser).toBe(contexts.at(-1));
    expect(options).toMatchObject({ reducedMotion: false, cookies: [{ name: 'token', value: 'a=b' }],
      headers: [['X-Token', 'abc:123']] });
    options.log('ordinary progress');
    options.log('unsupported CSS warning');
    return { ...result(), html: `<html>${url}</html>`, fonts: [{ path: '/captured/font.ttf' }] };
  });
  expect(await runImportAdapter(request, responseFile)).toBe(0);
  const data = response();
  expect(data.status).toBe('complete');
  expect(data.errors).toEqual([]);
  expect(data.outputs.map(o => o.id)).toEqual(['first', 'second']);
  for (const output of data.outputs) {
    expect(output).toMatchObject({ checkpoint: '', width: 32, height: 24, baseDir: '',
      warnings: ['unsupported CSS warning'], fonts: ['/captured/font.ttf'] });
    expect(fs.readFileSync(output.snapshot, 'utf8')).toBe(`<html>${output.url}</html>`);
  }
  expect(contexts).toHaveLength(2);
  expect(contexts[0]).not.toBe(contexts[1]);
  contexts.forEach(context => expect(context.close).toHaveBeenCalledTimes(1));
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('a failed source closes its context and allows later sources to finish', async () => {
  request.inputs.push({ id: 'second', input: request.inputs[0].input });
  runSnapshot.mockRejectedValueOnce('capture rejected').mockResolvedValueOnce(result());
  expect(await runImportAdapter(request, responseFile)).toBe(1);
  expect(response()).toMatchObject({ status: 'failed', errors: [{ id: 'first', error: 'capture rejected' }],
    outputs: [{ id: 'second', baseDir: dir, url: pathToFileURL(request.inputs[0].input).href }] });
  expect(response().outputs).toHaveLength(1);
  contexts.forEach(context => expect(context.close).toHaveBeenCalledTimes(1));
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('scenario checkpoints are durable before a later interaction fails', async () => {
  const scenario = path.join(dir, 'states.json');
  fs.writeFileSync(scenario, JSON.stringify({ version: 1, steps: [
    { type: 'capture', name: 'initial' }, { type: 'capture', name: 'later' },
  ] }));
  request.options.scenario = scenario;
  runScenario.mockImplementation(async (_, url, __, options) => {
    await options.onCheckpoint({ ...result(), name: 'initial', url: `${url}#state`, warnings: ['partial state'] });
    expect(response().outputs[0].checkpoint).toBe('initial');
    expect(fs.readFileSync(response().outputs[0].snapshot, 'utf8')).toBe(result().html);
    throw new Error('later interaction failed');
  });
  expect(await runImportAdapter(request, responseFile)).toBe(1);
  expect(response()).toMatchObject({ status: 'failed', outputs: [{ checkpoint: 'initial',
    warnings: ['partial state'] }], errors: [{ id: 'first', error: 'later interaction failed' }] });
  expect(runSnapshot).not.toHaveBeenCalled();
  expect(browser.createBrowserContext).not.toHaveBeenCalled();
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('cancellation closes the current source and browser without capturing later sources', async () => {
  request.inputs.push({ id: 'second', input: request.inputs[0].input });
  runSnapshot.mockImplementation(async () => {
    process.emit('SIGINT');
    throw new Error('driver closed');
  });
  await expect(withCliLifecycle(() => runImportAdapter(request, responseFile)))
    .rejects.toMatchObject({ exitCode: 130 });
  expect(runSnapshot).toHaveBeenCalledTimes(1);
  expect(response()).toMatchObject({ status: 'failed', outputs: [],
    errors: [{ id: '', error: 'capture cancelled (SIGINT)' }] });
  expect(contexts[0].close).toHaveBeenCalledTimes(1);
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('a work directory failure is recorded and still closes the browser', async () => {
  fs.writeFileSync(request.workDir, 'a file');
  await expect(runImportAdapter(request, responseFile)).rejects.toThrow();
  expect(response()).toMatchObject({ status: 'failed', outputs: [], errors: [{ id: '' }] });
  expect(runSnapshot).not.toHaveBeenCalled();
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('preview returns ordered pages without browser acquisition', async () => {
  const preview = { items: [{ name: 'first', png: '/first.png' }], outputDir: dir,
    pagxBin: '/pagx', columns: 2 };
  const pages = [{ file: path.join(dir, 'sheet.png'), items: ['first'] }];
  generateContactSheet.mockImplementation(async (items, options) => {
    expect(items).toEqual(preview.items);
    expect(options).toMatchObject(preview);
    options.log('rendered');
    return { pages };
  });
  expect(await runImportAdapter({ version: 1, operation: 'preview', preview }, responseFile)).toBe(0);
  expect(response()).toEqual({ version: 1, status: 'complete', pages });
  expect(launchBrowser).not.toHaveBeenCalled();
});

test.each([
  [null, /must be an object/],
  [{ version: 1, operation: 'delete' }, /invalid import operation/],
  [{ version: 1, operation: 'preview' }, /invalid preview request/],
  [{ version: 1, operation: 'inspect', inputs: [] }, /needs inputs/],
])('rejects malformed protocol envelopes (%j)', (value, error) => {
  expect(() => validateImportRequest(value)).toThrow(error);
  expect(launchBrowser).not.toHaveBeenCalled();
});

test.each([
  [{ inputs: [null] }, /source id/],
  [{ inputs: [{ id: '../escape', input: 'https://example.test' }] }, /source id/],
  [{ inputs: [{ id: 'ok', input: '' }] }, /invalid import input/],
  [{ inputs: [{ id: 'ok', input: '/missing/page.html' }] }, /input not found/],
  [{ options: { browserEngine: 'unknown' } }, /unsupported browser engine/],
  [{ options: { cookies: ['missing-separator'] } }, /cookie expects/],
  [{ options: { cookies: ['a=b\r\n'] } }, /cookie expects/],
  [{ options: { headers: ['missing-separator'] } }, /header expects/],
  [{ options: { headers: ['Bad Header: v'] } }, /header expects/],
  [{ options: { headers: ['X: v\0'] } }, /header expects/],
  [{ options: { checkpoint: 'missing' } }, /checkpoint requires/],
  [{ workDir: 'relative' }, /absolute work directory/],
  [{ workDir: '' }, /absolute work directory/],
])('rejects invalid capture requests before publishing a response (%j)', async (patch, error) => {
  await expect(runImportAdapter({ ...request, ...patch }, responseFile)).rejects.toThrow(error);
  expect(fs.existsSync(responseFile)).toBe(false);
  expect(fs.existsSync(request.workDir)).toBe(false);
  expect(launchBrowser).not.toHaveBeenCalled();
});

test('scenario requests require one source and valid preview dimensions', () => {
  const scenario = path.join(dir, 'states.json');
  fs.writeFileSync(scenario, JSON.stringify({ version: 1, steps: [{ type: 'capture', name: 'one' }] }));
  request.inputs.push({ id: 'second', input: request.inputs[0].input });
  request.options.scenario = scenario;
  expect(() => validateImportRequest(request)).toThrow('exactly one input');
  expect(() => validateImportRequest({ version: 1, operation: 'preview', preview: {
    items: [{}], outputDir: dir, pagxBin: '/pagx', columns: 0,
  } })).toThrow('columns');
});
