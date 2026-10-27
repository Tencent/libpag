// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const { parseCaptureArgs, validateCaptureOptions } = require('../capture-cli');
const { collectBatchInputs } = require('../dist/lib/batch');
const { parseCookie, parseHeader } = require('../dist/lib/cli');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'capture-validation-')); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); });

test.each([
  ['--cookie', 'missing-separator'], ['--cookie', 'a=b\r\n'], ['--cookie', 'a=\0'],
  ['--header', 'missing-separator'], ['--header', 'Bad Header: value'],
  ['--header', 'X: value\n'], ['--header', 'X: value\0'],
])('invalid %s values fail before browser setup', (flag, value) => {
  const options = parseCaptureArgs(['https://example.test', '--output-name', 'page', flag, value]);
  expect(() => validateCaptureOptions(options)).toThrow();
});

test('credentials preserve delimiters and empty values', () => {
  expect(parseCookie('--cookie', 'token=a=b')).toEqual({ name: 'token', value: 'a=b' });
  expect(parseCookie('--cookie', 'empty=')).toEqual({ name: 'empty', value: '' });
  expect(parseHeader('--header', 'X-Token: abc:123')).toEqual(['X-Token', 'abc:123']);
  expect(parseHeader('--header', 'X-Empty:')).toEqual(['X-Empty', '']);
});

test('batch name collisions fail before binary and browser preflight', () => {
  fs.writeFileSync(path.join(dir, 'page.htm'), '<html/>');
  fs.writeFileSync(path.join(dir, 'page.html'), '<html/>');
  const output = path.join(dir, 'out');
  const result = spawnSync(process.execPath, [path.resolve(__dirname, '../html2pagx'),
    '--batch', dir, '--output-root', output], { encoding: 'utf8',
    env: { ...process.env, PAGX_BIN: '/missing-pagx', PUPPETEER_EXECUTABLE_PATH: '/missing-browser' } });
  expect(result.status).toBe(2);
  expect(result.stderr).toContain('output name collision');
  expect(fs.existsSync(output)).toBe(false);
});

test('batch scans exclude a nested output directory and prepared snapshots', () => {
  const input = path.join(dir, 'page.html'), output = path.join(dir, 'out');
  fs.writeFileSync(input, '<html/>');
  fs.writeFileSync(path.join(dir, 'old.subset.html'), '<html/>');
  fs.mkdirSync(output);
  fs.writeFileSync(path.join(output, 'old.html'), '<html/>');
  expect(collectBatchInputs(dir, output)).toEqual([input]);
  expect(collectBatchInputs(dir, dir)).toEqual([path.join(output, 'old.html'), input]);
});

test('a non-directory output ancestor is rejected without creating descendants', () => {
  const file = path.join(dir, 'file');
  fs.writeFileSync(file, 'keep');
  const opts = parseCaptureArgs(['https://example.test', '--output-name', 'page', '-o', path.join(file, 'out')]);
  expect(() => validateCaptureOptions(opts)).toThrow('not a directory');
  expect(fs.readFileSync(file, 'utf8')).toBe('keep');
});
