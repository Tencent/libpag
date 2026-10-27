// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
jest.mock('../dist/lib/browser-engine', () => ({ launchBrowser: jest.fn() }));
jest.mock('../dist/lib/snapshot-runner', () => ({ runSnapshot: jest.fn() }));
jest.mock('../dist/lib/pipeline', () => ({ ...jest.requireActual('../dist/lib/pipeline'), runHtmlToPagx: jest.fn() }));
jest.mock('../dist/lib/contact-sheet', () => ({ ...jest.requireActual('../dist/lib/contact-sheet'), generateContactSheet: jest.fn() }));
const { launchBrowser } = require('../dist/lib/browser-engine');
const { runSnapshot } = require('../dist/lib/snapshot-runner');
const { runHtmlToPagx } = require('../dist/lib/pipeline');
const { generateContactSheet } = require('../dist/lib/contact-sheet');
const { runBatch } = require('../dist/lib/batch');
const { withCliLifecycle } = require('../dist/lib/cli-lifecycle');
let dir, options, browser, contexts, stderr;
beforeEach(() => {
  jest.resetAllMocks();
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'batch 中文 # & '));
  fs.writeFileSync(path.join(dir, 'a.html'), '<html/>');
  fs.writeFileSync(path.join(dir, 'b.html'), '<html/>');
  options = { inputDir: dir, scriptDir: dir, log: () => {} };
  contexts = [];
  const createContext = async () => {
    const context = { close: jest.fn().mockResolvedValue() };
    contexts.push(context);
    return context;
  };
  browser = { createBrowserContext: jest.fn(createContext), newContext: jest.fn(createContext), close: jest.fn().mockResolvedValue() };
  launchBrowser.mockResolvedValue({ engine: 'puppeteer', browser });
  runSnapshot.mockResolvedValue({ html: '<html>captured</html>', fonts: [] });
  runHtmlToPagx.mockImplementation(async opts => {
    const subsetHtml = path.join(opts.outputDir, `${opts.outputName}.subset.html`);
    const captured = await opts.snapshotImpl({ input: opts.input, output: subsetHtml, captureAnimations: opts.captureAnimations });
    if (captured.code) throw new Error(captured.stderr);
    return { subsetHtml, pagx: path.join(opts.outputDir, `${opts.outputName}.pagx`),
      png: path.join(opts.outputDir, `${opts.outputName}.png`), fonts: captured.fontFiles };
  });
  generateContactSheet.mockImplementation(async (items, opts) => ({ pages: [{
    file: path.join(opts.outputDir, 'sheet-001.png'), items: items.map(item => item.name),
  }] }));
  stderr = jest.spyOn(process.stderr, 'write').mockImplementation(() => true);
});
afterEach(() => {
  stderr.mockRestore();
  fs.rmSync(dir, { recursive: true, force: true });
});

test.each(['puppeteer', 'playwright'])('%s batches isolate inputs and close every context', async engine => {
  launchBrowser.mockResolvedValue({ engine, browser });
  runSnapshot.mockImplementation(async (handle, url, opts) => {
    expect(handle.browser).toBe(contexts.at(-1));
    expect(url).toBe(pathToFileURL(path.join(dir, contexts.length === 1 ? 'a.html' : 'b.html')).href);
    expect(opts.reducedMotion).toBe(false);
    return { html: url, fonts: [{ path: `/fonts/${contexts.length}.ttf` }] };
  });
  expect(await runBatch({ ...options, captureAnimations: true })).toEqual({ total: 2, processed: 2, skipped: 0, failed: 0 });
  expect(contexts).toHaveLength(2);
  expect(contexts[0]).not.toBe(contexts[1]);
  contexts.forEach(context => expect(context.close).toHaveBeenCalledTimes(1));
  expect(browser.close).toHaveBeenCalledTimes(1);
  expect(fs.readFileSync(path.join(dir, 'a.subset.html'), 'utf8')).toBe(pathToFileURL(path.join(dir, 'a.html')).href);
});

test('a failed input closes its context and later inputs still convert', async () => {
  runSnapshot.mockRejectedValueOnce(new Error('capture failed'));
  expect(await runBatch(options)).toEqual({ total: 2, processed: 1, skipped: 0, failed: 1 });
  expect(contexts).toHaveLength(2);
  contexts.forEach(context => expect(context.close).toHaveBeenCalledTimes(1));
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('cancellation closes the active context without converting later inputs', async () => {
  runSnapshot.mockImplementation(async () => { process.emit('SIGINT'); throw new Error('driver closed'); });
  await expect(withCliLifecycle(() => runBatch(options))).rejects.toMatchObject({ exitCode: 130 });
  expect(runHtmlToPagx).toHaveBeenCalledTimes(1);
  expect(contexts[0].close).toHaveBeenCalledTimes(1);
  expect(browser.close).toHaveBeenCalledTimes(1);
});

test('preview contains only newly completed inputs, in order, after browser shutdown', async () => {
  fs.writeFileSync(path.join(dir, 'a.pagx'), 'old');
  fs.writeFileSync(path.join(dir, 'a.png'), 'old');
  fs.writeFileSync(path.join(dir, 'c.html'), '<html/>');
  fs.writeFileSync(path.join(dir, 'd.html'), '<html/>');
  runSnapshot.mockRejectedValueOnce(new Error('b failed'));
  generateContactSheet.mockImplementation(async (items, opts) => {
    expect(browser.close).toHaveBeenCalledTimes(1);
    expect(items).toEqual(['c', 'd'].map(name => ({ name: `${name}.html`, png: path.join(dir, `${name}.png`) })));
    expect(opts).toMatchObject({ outputDir: path.join(dir, 'preview'), columns: 2 });
    return { pages: [{ file: path.join(opts.outputDir, 'sheet-001.png') }] };
  });
  expect(await runBatch({ ...options, skipExisting: true, preview: true, previewColumns: 2 }))
    .toMatchObject({ total: 4, processed: 2, skipped: 1, failed: 1, preview: { pages: [{}] } });
  expect(generateContactSheet).toHaveBeenCalledTimes(1);
});

test('an entirely skipped batch never previews files from an earlier run', async () => {
  for (const name of ['a', 'b']) fs.writeFileSync(path.join(dir, `${name}.pagx`), 'old');
  expect(await runBatch({ ...options, skipExisting: true, preview: true })).toMatchObject({ skipped: 2, preview: { pages: [] } });
  expect(generateContactSheet).not.toHaveBeenCalled();
});

test('colliding HTML basenames fail before creating outputs or a browser', async () => {
  fs.writeFileSync(path.join(dir, 'a.htm'), '<html/>');
  const outputRoot = path.join(dir, 'output');
  await expect(runBatch({ ...options, outputRoot })).rejects.toThrow('output name collision');
  expect(launchBrowser).not.toHaveBeenCalled();
  expect(fs.existsSync(outputRoot)).toBe(false);
});
