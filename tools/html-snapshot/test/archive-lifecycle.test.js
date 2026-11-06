// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { EventEmitter } = require('events');
jest.mock('../dist/lib/browser-engine', () => ({
  ...jest.requireActual('../dist/lib/browser-engine'), launchBrowser: jest.fn(),
}));
jest.mock('../dist/lib/snapshot-runner', () => ({ runSnapshot: jest.fn() }));
jest.mock('../dist/lib/pipeline', () => ({
  ...jest.requireActual('../dist/lib/pipeline'), runHtmlToPagx: jest.fn(),
}));
jest.mock('../dist/lib/contact-sheet', () => ({
  ...jest.requireActual('../dist/lib/contact-sheet'), generateContactSheet: jest.fn(),
}));
const { launchBrowser } = require('../dist/lib/browser-engine');
const { runSnapshot } = require('../dist/lib/snapshot-runner');
const { runHtmlToPagx } = require('../dist/lib/pipeline');
const { generateContactSheet } = require('../dist/lib/contact-sheet');
const { runArchiveImport } = require('../dist/lib/archive-import');
const { writeZip } = require('./archive-helpers');
let dir, options, browser, contexts, origins;
beforeEach(async () => {
  jest.clearAllMocks();
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-lifecycle-'));
  options = { input: path.join(dir, 'site.zip'), outputDir: path.join(dir, 'out'),
    scriptDir: path.resolve(__dirname, '..'), pagxBin: process.execPath,
    keepSubsetHtml: true, log: jest.fn() };
  await writeZip(options.input, { 'a.html': '<body>first</body>', 'nested/b.htm': '<body>second</body>' });
  contexts = []; origins = new Set();
  browser = { close: jest.fn().mockResolvedValue(), createBrowserContext: jest.fn(async () => {
    const context = { close: jest.fn().mockResolvedValue() }; contexts.push(context); return context;
  }) };
  launchBrowser.mockResolvedValue({ engine: 'puppeteer', browser });
  runSnapshot.mockImplementation(async (_handle, url, opts) => {
    origins.add(new URL(url).origin);
    expect(opts.capturedResourcesOnly).toBe(true);
    expect(opts.downloadFonts).toBe(true);
    return { html: '<html><body>captured</body></html>', fonts: [] };
  });
  runHtmlToPagx.mockImplementation(async opts => {
    const subsetHtml = path.join(opts.outputDir, `${opts.outputName}.subset.html`);
    const captured = await opts.snapshotImpl({ ...opts, output: subsetHtml });
    if (captured.code) throw new Error(captured.stderr);
    const pagx = opts.outputFile || path.join(opts.outputDir, `${opts.outputName}.pagx`);
    const png = opts.doRender ? path.join(opts.outputDir, `${opts.outputName}.png`) : null;
    fs.writeFileSync(pagx, '<pagx/>');
    if (png) fs.writeFileSync(png, 'png');
    return { pagx, png, subsetHtml, fonts: captured.fontFiles, warnings: captured.warnings, readiness: opts.task.readiness };
  });
  generateContactSheet.mockImplementation(async (items, opts) => ({
    pages: [{ file: path.join(opts.outputDir, 'sheet-001.png'), items: items.map(item => item.name) }],
  }));
  jest.spyOn(process.stderr, 'write').mockImplementation(() => true);
});
afterEach(async () => {
  jest.restoreAllMocks();
  // The archive owns both servers and must release them on success and failure.
  for (const origin of origins) await expect(fetch(origin)).rejects.toThrow();
  fs.rmSync(dir, { recursive: true, force: true });
});

test('publishes relative manifests and previews, including cached pages on a repeat import', async () => {
  const opts = { ...options, preview: true, previewColumns: 2, skipExisting: true };
  const first = await runArchiveImport(opts);
  expect(first).toMatchObject({ processed: 2, skipped: 0, failed: 0, total: 2 });
  expect(first.manifest).toMatchObject({ status: 'complete', outputs: [
    { source: 'a.html', status: 'complete', pagx: 'a.html.pagx', png: 'a.html.png' },
    { source: 'nested/b.htm', status: 'complete', pagx: 'nested/b.htm.pagx', png: 'nested/b.htm.png' },
  ], preview: { pages: [{ file: 'preview/sheet-001.png', items: ['a.html', 'nested/b.htm'] }] } });
  expect(JSON.parse(fs.readFileSync(first.manifestFile))).toEqual(first.manifest);
  const second = await runArchiveImport(opts);
  expect(second).toMatchObject({ processed: 0, skipped: 2, failed: 0 });
  expect(second.manifest.outputs.every(item => item.skipped)).toBe(true);
  expect(generateContactSheet.mock.calls[1][0].map(item => item.name)).toEqual(['a.html', 'nested/b.htm']);
  expect(runSnapshot).toHaveBeenCalledTimes(2);
  contexts.forEach(context => expect(context.close).toHaveBeenCalledTimes(1));
});

test.each(['cookies', 'headers'])('changing %s invalidates cached pages, even during dry-run', async field => {
  await writeZip(options.input, { 'index.html': '<body>one</body>' });
  const values = field === 'cookies' ? ['session=first', 'session=second'] : ['X-Language: en', 'X-Language: zh'];
  const opts = { ...options, skipExisting: true, [field]: [values[0]] };
  await runArchiveImport(opts);
  expect(fs.readFileSync(path.join(options.outputDir, 'index.html.import.json'), 'utf8')).not.toContain(values[0]);
  expect(await runArchiveImport(opts)).toMatchObject({ processed: 0, skipped: 1 });
  expect(await runArchiveImport({ ...opts, dryRun: true, [field]: [values[1]] }))
    .toMatchObject({ processed: 1, skipped: 0, manifest: { status: 'planned' } });
  expect(await runArchiveImport({ ...opts, [field]: [values[1]] })).toMatchObject({ processed: 1, skipped: 0 });
});

test('remote origins disable archive-only caching and exact single-file destinations are preserved', async () => {
  await writeZip(options.input, { 'index.html': '<body>one</body>' });
  const opts = { ...options, outputDir: undefined, outputFile: path.join(dir, 'chosen.ext'),
    allowOrigins: ['https://cdn.example'], skipExisting: true };
  for (let i = 0; i < 2; i++) {
    expect(await runArchiveImport(opts)).toMatchObject({ processed: 1, skipped: 0,
      manifestFile: path.join(dir, 'chosen.import-manifest.json'),
      manifest: { outputs: [{ pagx: 'chosen.ext' }] } });
  }
  expect(fs.readFileSync(opts.outputFile, 'utf8')).toBe('<pagx/>');
});

test('reports resource failures once, ignores favicons, and retries partial pages', async () => {
  await writeZip(options.input, { 'index.html': '<body>one</body>' });
  runSnapshot.mockImplementationOnce(async (_handle, url, opts) => {
    const page = new EventEmitter(), frame = {};
    page.mainFrame = () => frame;
    await opts.beforeNavigate(page);
    const response = (name, status, navigation = false) => ({ status: () => status,
      url: () => new URL(name, url).href,
      request: () => ({ isNavigationRequest: () => navigation, frame: () => frame }) });
    page.emit('response', response('/ok.png', 200));
    page.emit('response', response('/favicon.ico', 404));
    page.emit('requestfailed', { url: () => new URL('/favicon.ico', url).href });
    page.emit('response', response('/missing.png', 404));
    page.emit('response', response('/missing.png', 404));
    page.emit('requestfailed', { url: () => new URL('/blocked.png', url).href });
    page.emit('pageerror', new Error('script crashed'));
    return { html: '<html><body>partial</body></html>', fonts: [] };
  });
  const result = await runArchiveImport({ ...options, skipExisting: true });
  expect(result.manifest.status).toBe('partial');
  expect(result.manifest.outputs[0].warnings).toEqual([
    'HTTP 404: zip:/missing.png', 'Resource request failed: zip:/blocked.png', 'Page exception: script crashed',
  ]);
  expect(await runArchiveImport({ ...options, skipExisting: true })).toMatchObject({ processed: 1, skipped: 0 });
});

test('a failed main navigation is recorded and does not prevent later pages from completing', async () => {
  runSnapshot.mockImplementationOnce(async (_handle, url, opts) => {
    const page = new EventEmitter(), frame = {};
    page.mainFrame = () => frame;
    await opts.beforeNavigate(page);
    page.emit('response', { status: () => 404, url: () => new URL('/missing.html', url).href,
      request: () => ({ isNavigationRequest: () => true, frame: () => frame }) });
    return { html: '<html/>', fonts: [] };
  });
  const result = await runArchiveImport(options);
  expect(result).toMatchObject({ processed: 1, failed: 1, manifest: { status: 'failed', outputs: [
    { status: 'failed', error: expect.stringContaining('HTTP 404') }, { status: 'complete' },
  ] } });
});

test('cancellation preserves completed pages, marks remaining pages, and releases resources', async () => {
  await writeZip(options.input, { 'a.html': 'a', 'b.html': 'b', 'c.html': 'c' });
  const controller = new AbortController();
  const capture = runSnapshot.getMockImplementation();
  runSnapshot.mockImplementation(async (...args) => {
    if (runSnapshot.mock.calls.length === 2) controller.abort(new Error('caller cancelled'));
    return capture(...args);
  });
  await expect(runArchiveImport({ ...options, signal: controller.signal })).rejects.toThrow('caller cancelled');
  const manifest = JSON.parse(fs.readFileSync(path.join(options.outputDir, 'import-manifest.json')));
  expect(manifest).toMatchObject({ status: 'failed', error: 'caller cancelled' });
  expect(manifest.outputs.map(item => item.status)).toEqual(['complete', 'failed', 'cancelled']);
  expect(runSnapshot).toHaveBeenCalledTimes(2);
  contexts.forEach(context => expect(context.close).toHaveBeenCalledTimes(1));
});

test('startup failures cancel pending pages and pre-cancelled imports create no outputs', async () => {
  const controller = new AbortController(); controller.abort(new Error('already cancelled'));
  await expect(runArchiveImport({ ...options, signal: controller.signal })).rejects.toThrow('already cancelled');
  expect(fs.existsSync(options.outputDir)).toBe(false);
  launchBrowser.mockRejectedValueOnce(new Error('browser unavailable'));
  await expect(runArchiveImport(options)).rejects.toThrow('browser unavailable');
  const manifest = JSON.parse(fs.readFileSync(path.join(options.outputDir, 'import-manifest.json')));
  expect(manifest.outputs.map(item => item.status)).toEqual(['cancelled', 'cancelled']);
  expect(manifest.error).toBe('browser unavailable');
});
