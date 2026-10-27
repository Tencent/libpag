// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const { parseCaptureArgs, validateCaptureOptions } = require('../capture-cli');
const fixture = path.join(__dirname, 'fixtures/interaction.html');
const scenario = path.join(__dirname, 'fixtures/interaction.scenario.json');

test('positional and named inputs share the same contract', () => {
  expect(parseCaptureArgs(['--input', fixture, '--scenario', scenario, '-o', 'out', '--preview']))
    .toEqual(parseCaptureArgs([fixture, '--scenario', scenario, '--output-dir', 'out', '--preview']));
  expect(() => parseCaptureArgs([fixture, '--input', fixture])).toThrow('only one input');
});

test.each([
  ['--scenario', '--preview'], ['--preview'], ['--checkpoint', 'missing'],
  ['--scenario', scenario, '--preview', '--no-resolve'],
  ['--scenario', scenario, '--preview', '--no-render'],
  ['--scenario', scenario, '--capture-animations'],
  ['--output-name', '../outside'], ['--output-name', '..\\outside'],
  ['--preview-columns', '4'], ['--unknown'],
])('rejects invalid options without requiring a browser: %j', (...args) => {
  expect(() => parseCaptureArgs([fixture, ...args])).toThrow();
});

test('scenario content and checkpoint names are validated before runtime preparation', () => {
  const opts = parseCaptureArgs([fixture, '--scenario', scenario, '--checkpoint', 'missing']);
  expect(() => validateCaptureOptions(opts)).toThrow('Unknown checkpoint');
  expect(() => validateCaptureOptions(parseCaptureArgs([fixture, '--cookie', 'a=b'])))
    .toThrow('cookies/headers require a URL');
  expect(() => validateCaptureOptions(parseCaptureArgs(['https://example.com', '--output-name', 'page', '--header', 'bad'])))
    .toThrow('expects');
});

test.each(['html2pagx', 'capture.js'])('%s help and dry-run need neither browser nor pagx', entry => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'capture dry run '));
  try {
    const env = { ...process.env, PAGX_BIN: path.join(dir, 'missing'), PUPPETEER_EXECUTABLE_PATH: path.join(dir, 'missing') };
    const run = args => spawnSync(process.execPath, [path.join(__dirname, '..', entry), ...args], { env, encoding: 'utf8' });
    expect(run(['--help']).status).toBe(0);
    fs.writeFileSync(path.join(dir, 'page.html'), '<body>hello</body>');
    const output = path.join(dir, 'output');
    const result = run(['--batch', dir, '--dry-run', '--output-root', output]);
    expect(result.status).toBe(0);
    expect(result.stdout + result.stderr).toContain('would process page.html');
    expect(fs.existsSync(output)).toBe(false);
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
});
