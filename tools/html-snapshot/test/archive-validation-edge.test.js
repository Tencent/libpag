// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { preflightArchiveImport, isZipInput } = require('../dist/lib/archive-import');
const { validateImportRequest, validateImportArchive } = require('../dist/lib/import-request');
const { writeZip } = require('./archive-helpers');
let dir, opts;
beforeEach(async () => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-validation-'));
  opts = { version: 1, format: 'html', input: path.join(dir, 'site.ZIP'), outputDir: path.join(dir, 'out'),
    pagxBin: process.execPath, scriptDir: path.resolve(__dirname, '..') };
  await writeZip(opts.input, { 'index.html': 'home' });
});
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

test('only local ZIP paths use the archive pipeline', async () => {
  expect(isZipInput(opts.input)).toBe(true);
  for (const value of ['http://host/site.zip', 'HTTPS://host/site.ZIP', 'index.html', 'site.zip.backup']) {
    expect(isZipInput(value)).toBe(false);
  }
  await expect(validateImportArchive({ ...opts, input: 'https://host/site.zip' }, opts.scriptDir)).resolves.toBeUndefined();
});

test.each([{ outputDir: undefined }, { outputFile: 'one.pagx' }, { outputName: 'name' }])
  ('rejects ambiguous destinations without creating output: %j', async values => {
    await expect(preflightArchiveImport({ ...opts, ...values })).rejects.toThrow();
    expect(fs.existsSync(opts.outputDir)).toBe(false);
  });

test('detects artifact collisions, existing directories, and resource directory conflicts', async () => {
  await expect(preflightArchiveImport({ ...opts, outputDir: undefined,
    outputFile: path.join(dir, 'chosen.png'), doRender: true })).rejects.toThrow(/collision/);
  fs.mkdirSync(path.join(opts.outputDir, 'index.html.pagx'), { recursive: true });
  await expect(preflightArchiveImport(opts)).rejects.toThrow(/must be a file/);
  fs.rmdirSync(path.join(opts.outputDir, 'index.html.pagx'));
  fs.writeFileSync(path.join(opts.outputDir, 'index.html.fonts'), 'not a directory');
  await expect(preflightArchiveImport(opts)).rejects.toThrow(/parent must be a directory/);
  fs.unlinkSync(path.join(opts.outputDir, 'index.html.fonts'));
  await expect(preflightArchiveImport({ ...opts, downloadImages: true,
    imageDir: path.join(opts.outputDir, 'index.html.pagx') })).rejects.toThrow(/file\/directory collision/);
});

test.each([{ cookies: ['session=bad\r\n'] }, { headers: ['Bad Header: value'] }])
  ('ZIP credentials fail at request validation, before browser initialization: %j', values => {
    expect(() => validateImportRequest({ ...opts, ...values })).toThrow();
    expect(fs.existsSync(opts.outputDir)).toBe(false);
  });

test('archive-only options are rejected for HTML and malformed ZIP errors retain CLI exit status', async () => {
  const input = path.join(dir, 'index.html'); fs.writeFileSync(input, '<html/>');
  for (const values of [{ archiveRoot: 'site' }, { allowOrigins: ['https://cdn.example'] }]) {
    expect(() => validateImportRequest({ ...opts, input, ...values })).toThrow(/require ZIP/);
  }
  fs.writeFileSync(opts.input, 'invalid ZIP');
  await expect(validateImportArchive(opts, opts.scriptDir)).rejects.toMatchObject({ exitCode: 2 });
  expect(fs.existsSync(opts.outputDir)).toBe(false);
});
