// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
jest.mock('../dist/lib/browser-engine', () => ({ launchBrowser: jest.fn() }));
jest.mock('../dist/lib/interaction', () => ({ ...jest.requireActual('../dist/lib/interaction'), runScenario: jest.fn() }));
jest.mock('../dist/lib/pipeline', () => ({ ...jest.requireActual('../dist/lib/pipeline'), runHtmlToPagx: jest.fn() }));
jest.mock('../dist/lib/contact-sheet', () => ({ ...jest.requireActual('../dist/lib/contact-sheet'), generateContactSheet: jest.fn() }));
const { launchBrowser } = require('../dist/lib/browser-engine');
const { runScenario } = require('../dist/lib/interaction');
const { runHtmlToPagx } = require('../dist/lib/pipeline');
const { generateContactSheet } = require('../dist/lib/contact-sheet');
const { runScenarioToPagx } = require('../dist/lib/scenario-pipeline');
const { withCliLifecycle } = require('../dist/lib/cli-lifecycle');

const checkpoint = name => ({ name, url: 'file:///page.html', width: 390, height: 844, warnings: [], html: name });
const capture = names => async (_, __, ___, opts) => {
  const results = names.map(checkpoint);
  for (const result of results) await opts.onCheckpoint(result);
  return results;
};

let dir, options, close;
const readManifest = () => JSON.parse(fs.readFileSync(path.join(dir, 'page.captures.json'), 'utf8'));
beforeEach(() => {
  jest.clearAllMocks();
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'scenario preview '));
  const scenario = path.join(dir, 'states.json');
  fs.writeFileSync(scenario, JSON.stringify({ version: 1, steps: [{ type: 'capture', name: 'preview' }, { type: 'capture', name: 'last' }] }));
  options = { input: path.join(dir, 'page.html'), scenario, outputDir: dir };
  close = jest.fn().mockResolvedValue(undefined);
  launchBrowser.mockResolvedValue({ browser: { close } });
  runScenario.mockImplementation(capture(['preview', 'last']));
  runHtmlToPagx.mockImplementation(async opts => {
    const files = { subsetHtml: path.join(dir, `${opts.outputName}.subset.html`), pagx: path.join(dir, `${opts.outputName}.pagx`),
      png: path.join(dir, `${opts.outputName}.png`), fonts: [] };
    fs.writeFileSync(files.png, opts.outputName);
    await opts.snapshotImpl({ output: files.subsetHtml });
    return files;
  });
  generateContactSheet.mockImplementation(async (items, opts) => {
    expect(readManifest().preview).toEqual({ status: 'pending', pages: [] });
    return { pages: [{ file: path.join(opts.outputDir, 'sheet-001.png'), width: 500, height: 600, items: items.map(item => item.name) }] };
  });
});
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });

test('default export retains its manifest format and does not generate a preview', async () => {
  await runScenarioToPagx(options);
  expect(readManifest().outputs.map(output => output.name)).toEqual(['preview', 'last']);
  expect(readManifest()).not.toHaveProperty('preview');
  expect(generateContactSheet).not.toHaveBeenCalled();
  expect(close).toHaveBeenCalledTimes(1);
  expect(readManifest().status).toBe('complete');
});

test('a capture failure invalidates an earlier successful manifest', async () => {
  await runScenarioToPagx({ ...options, preview: true });
  runScenario.mockRejectedValueOnce(new Error('navigation failed'));
  await expect(runScenarioToPagx({ ...options, preview: true })).rejects.toThrow('navigation failed');
  expect(readManifest()).toMatchObject({ status: 'failed', outputs: [], error: 'navigation failed',
    preview: { status: 'failed', pages: [] } });
});

test('conversion failure records only completed checkpoints from this run', async () => {
  runHtmlToPagx.mockImplementationOnce(async () => ({ pagx: 'first.pagx', png: 'first.png', subsetHtml: 'first.html', fonts: [] }))
    .mockRejectedValueOnce(new Error('import failed'));
  await expect(runScenarioToPagx(options)).rejects.toThrow('import failed');
  expect(readManifest()).toMatchObject({ status: 'failed', error: 'import failed', outputs: [{ name: 'preview' }] });
  expect(readManifest().outputs).toHaveLength(1);
});

test('preview uses only this run, in capture order, with relative paths and no replay', async () => {
  fs.writeFileSync(path.join(dir, 'page.stale.png'), 'old run');
  await runScenarioToPagx({ ...options, preview: true, previewColumns: 4, scale: 2 });
  expect(runScenario).toHaveBeenCalledTimes(1);
  expect(launchBrowser).toHaveBeenCalledTimes(1);
  expect(close).toHaveBeenCalledTimes(1);
  expect(runHtmlToPagx).toHaveBeenCalledTimes(2);
  expect(generateContactSheet).toHaveBeenCalledWith([
    { name: 'preview', png: path.join(dir, 'page.preview.png') },
    { name: 'last', png: path.join(dir, 'page.last.png') },
  ], expect.objectContaining({ outputDir: path.join(dir, 'page.preview'), columns: 4 }));
  expect(readManifest().preview).toEqual({ status: 'complete', pages: [{ file: 'page.preview/sheet-001.png', width: 500, height: 600, items: ['preview', 'last'] }] });
  expect(fs.readFileSync(path.join(dir, 'page.preview.png'), 'utf8')).toBe('page.preview');
});

test('a selected checkpoint generates a one-item preview', async () => {
  runScenario.mockImplementation(capture(['last']));
  await runScenarioToPagx({ ...options, checkpoint: 'last', preview: true });
  expect(runScenario.mock.calls[0][3].checkpoint).toBe('last');
  expect(readManifest().preview.pages[0].items).toEqual(['last']);
  expect(runHtmlToPagx).toHaveBeenCalledTimes(1);
});

test('a failed repeated preview replaces the old success status and keeps state exports', async () => {
  await runScenarioToPagx({ ...options, preview: true });
  generateContactSheet.mockRejectedValueOnce(new Error('renderer unavailable'));
  await expect(runScenarioToPagx({ ...options, preview: true })).rejects.toThrow('preview generation failed: renderer unavailable');
  const manifest = readManifest();
  expect(manifest.preview).toEqual({ status: 'failed', pages: [], error: 'renderer unavailable' });
  expect(manifest.outputs).toHaveLength(2);
  for (const output of manifest.outputs) expect(fs.existsSync(output.png)).toBe(true);
});

test.each([
  { preview: true, doRender: false }, { preview: true, doResolve: false },
  { previewColumns: 2 }, { preview: true, previewColumns: 0 },
])('programmatic invalid options fail before browser launch: %j', async invalid => {
  await expect(runScenarioToPagx({ ...options, ...invalid })).rejects.toThrow();
  expect(launchBrowser).not.toHaveBeenCalled();
});

test.each([
  ['missing.html', '--scenario', 'missing.json', '--preview', '--no-render'],
  ['missing.html', '--scenario', 'missing.json', '--no-resolve', '--preview'],
  ['missing.html', '--scenario', 'missing.json', '--preview-columns', '4'],
  ...['0', '-1', '1.5', 'NaN', 'Infinity', '33', ''].map(value => ['missing.html', '--scenario', 'missing.json', '--preview', '--preview-columns', value]),
])('CLI rejects invalid preview arguments before input/browser preflight: %j', (...args) => {
  const result = spawnSync(process.execPath, [path.resolve(__dirname, '../html2pagx'), ...args], { encoding: 'utf8' });
  expect(result.status).toBe(2);
  expect(result.stderr).toMatch(/preview/);
  expect(result.stderr).not.toMatch(/input not found|browser|Chrome is missing/);
});

test('a later interaction failure still converts saved checkpoints after closing the browser', async () => {
  runScenario.mockImplementation(async (_, __, ___, opts) => {
    await opts.onCheckpoint({ ...checkpoint('preview'), fonts: [{ path: '/fonts/first.ttf' }] });
    expect(runHtmlToPagx).not.toHaveBeenCalled();
    expect(close).not.toHaveBeenCalled();
    throw new Error('later interaction failed');
  });
  const convert = runHtmlToPagx.getMockImplementation();
  runHtmlToPagx.mockImplementation(async opts => {
    expect(close).toHaveBeenCalledTimes(1);
    expect(opts.fontFiles).toEqual(['/fonts/first.ttf']);
    return convert(opts);
  });
  await expect(runScenarioToPagx({ ...options, preview: true })).rejects.toThrow('later interaction failed');
  expect(fs.readFileSync(path.join(dir, 'page.preview.subset.html'), 'utf8')).toBe('preview');
  expect(readManifest()).toMatchObject({ status: 'failed', error: 'later interaction failed',
    outputs: [{ name: 'preview' }], preview: { status: 'failed', pages: [] } });
  expect(readManifest().outputs).toHaveLength(1);
  expect(generateContactSheet).not.toHaveBeenCalled();
  expect(runScenario).toHaveBeenCalledTimes(1);
});

test('cancellation stops conversion and rejects late checkpoint callbacks', async () => {
  let onCheckpoint;
  runScenario.mockImplementation(async (_, __, ___, opts) => {
    onCheckpoint = opts.onCheckpoint;
    await onCheckpoint(checkpoint('preview'));
    process.emit('SIGTERM');
    throw new Error('page closed');
  });
  await expect(withCliLifecycle(() => runScenarioToPagx(options))).rejects.toMatchObject({ exitCode: 143 });
  expect(runHtmlToPagx).not.toHaveBeenCalled();
  expect(close).toHaveBeenCalledTimes(1);
  const finished = readManifest();
  expect(finished).toMatchObject({ status: 'failed', outputs: [], error: expect.stringContaining('SIGTERM') });
  await expect(onCheckpoint(checkpoint('last'))).rejects.toThrow('session has ended');
  expect(readManifest()).toEqual(finished);
});
