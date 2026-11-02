// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { readImportRequest, validateImportRequest, importNeedsBrowser } = require('../dist/lib/import-request');
const fixture = path.join(__dirname, 'fixtures/interaction.html');
const scenario = path.join(__dirname, 'fixtures/interaction.scenario.json');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'import-plan-')); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });
const request = values => ({ version: 1, input: fixture, pagxBin: '/native/pagx', format: 'html',
  doRender: false, doResolve: true, keepSubsetHtml: false, ...values });

test('only the internal versioned request is accepted by the runtime', () => {
  expect(readImportRequest(['--request', JSON.stringify(request())])).toEqual(request());
  for (const args of [[], ['--help'], ['page.html'], ['--request', '{}'], ['--request', 'not JSON']]) {
    expect(() => readImportRequest(args)).toThrow();
  }
});

test('multi-state file output fails before creating any directories', () => {
  const outputFile = path.join(dir, 'new', 'all.pagx');
  expect(() => validateImportRequest(request({ scenario, outputFile }))).toThrow('multiple checkpoints');
  expect(fs.readdirSync(dir)).toEqual([]);
  expect(() => validateImportRequest(request({ scenario }))).toThrow('multiple checkpoints');
});

test('selected checkpoints retain exact destinations and validate names', () => {
  const outputFile = path.join(dir, 'custom.ext');
  const planned = validateImportRequest(request({ scenario, checkpoint: 'advanced', outputFile }));
  expect(planned).toMatchObject({ outputFile, outputName: 'custom', outputDir: dir });
  expect(() => validateImportRequest(request({ scenario, checkpoint: 'absent', outputFile }))).toThrow('Unknown checkpoint');
  expect(fs.readdirSync(dir)).toEqual([]);
});

test('output collisions include PNGs, subsets, sources and scenario manifests', () => {
  expect(() => validateImportRequest(request({ outputFile: path.join(dir, 'result.png'), doRender: true }))).toThrow('collision');
  expect(() => validateImportRequest(request({ outputFile: fixture }))).toThrow('overwrite');
  const plan = path.join(dir, 'result.captures.json');
  fs.copyFileSync(scenario, plan);
  expect(() => validateImportRequest(request({ scenario: plan, checkpoint: 'initial',
    outputFile: path.join(dir, 'result.pagx') }))).toThrow('overwrite');
});

test('invalid credentials and output parents are rejected before browser setup', () => {
  for (const credentials of [{ cookies: ['a=b\r\n'] }, { headers: ['Bad Header: value'] }]) {
    expect(() => validateImportRequest(request({ input: 'https://example.test', outputDir: dir,
      outputName: 'web', ...credentials }))).toThrow();
  }
  const file = path.join(dir, 'file');
  fs.writeFileSync(file, 'keep');
  expect(() => validateImportRequest(request({ outputDir: path.join(file, 'out') }))).toThrow('not a directory');
  expect(fs.readFileSync(file, 'utf8')).toBe('keep');
});

test('only page acquisition prepares a browser', () => {
  expect(importNeedsBrowser(request())).toBe(true);
  expect(importNeedsBrowser(request({ format: 'svg', preview: true }))).toBe(false);
  expect(importNeedsBrowser(request({ htmlNoSnapshot: true, preview: true }))).toBe(false);
  expect(importNeedsBrowser(request({ batchDir: dir, dryRun: true }))).toBe(false);
});
