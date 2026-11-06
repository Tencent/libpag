// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const binary = process.env.PAGX_TEST_BIN || path.resolve(__dirname, '../../../cmake-build-debug/pagx');
const nativeTest = fs.existsSync(binary) ? test : test.skip;
let dir, bridge;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-cli-'));
  bridge = path.join(dir, 'bridge.js');
  fs.writeFileSync(bridge, 'process.stdout.write(JSON.stringify(process.argv.slice(2)));');
});
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));
function run(args, env = {}) {
  return spawnSync(binary, ['import', ...args], { cwd: dir, encoding: 'utf8', timeout: 10000,
    env: { ...process.env, PAGX_NODE_BIN: process.execPath, PAGX_IMPORT_BIN: bridge,
      PUPPETEER_EXECUTABLE_PATH: '/missing-browser', ...env } });
}

nativeTest('ZIP options cross the native bridge without default output names or lost origins', () => {
  const result = run(['网站.ZIP', '--html-root=site 中文', '--html-allow-origin', 'https://cdn.example',
    '--html-allow-origin=https://assets.example:8443', '--output-dir', 'out space', '--skip-existing', '--dry-run']);
  expect(result.status).toBe(0);
  const [flag, payload] = JSON.parse(result.stdout);
  expect(flag).toBe('--request');
  expect(JSON.parse(payload)).toMatchObject({ input: '网站.ZIP', format: 'html', archiveRoot: 'site 中文',
    allowOrigins: ['https://cdn.example', 'https://assets.example:8443'], outputDir: 'out space',
    skipExisting: true, dryRun: true, pagxBin: fs.realpathSync(binary) });
  expect(JSON.parse(payload).outputFile || '').toBe('');
  expect(JSON.parse(payload).outputName || '').toBe('');
  const exact = run(['site.zip', '--format=htm', '-o', 'chosen.ext']);
  expect(exact.status).toBe(0);
  expect(JSON.parse(JSON.parse(exact.stdout)[1])).toMatchObject({ outputFile: 'chosen.ext', format: 'html' });
});

nativeTest('a ZIP import reports an unavailable Node runtime without creating artifacts or falling back', () => {
  const result = run(['site.zip', '--html-root', 'site', '--html-allow-origin', 'https://cdn.example',
    '--output-dir', 'out'],
  { PAGX_NODE_BIN: path.join(dir, 'missing-node') });
  expect(result.status).toBe(1);
  expect(result.stderr).toContain('failed to start Node');
  expect(fs.readdirSync(dir)).toEqual(['bridge.js']);
});

nativeTest.each([
  [['site.zip'], 'requires --output-dir or --output'],
  [['site.zip', '--output-dir', 'out', '--output-name', 'one'], 'names derive from page paths'],
  [['site.zip', '--output-dir', 'out', '--html-no-snapshot'], 'cannot combine with batch or scenario'],
  [['site.zip', '--output-dir', 'out', '--html-scenario', 'states.json'], 'cannot combine with batch or scenario'],
  [['site.zip', '--output-dir', 'out', '--html-batch', 'pages'], 'cannot combine with batch or scenario'],
  [['site.zip', '--output-dir', 'out', '--format', 'svg'], 'requires HTML format'],
  [['site.zip', '--output-dir', 'out', '--html-cookie', 'session=value'], 'require a URL input'],
  [['site.zip', '--output-dir', 'out', '--html-header', 'X-Language: en'], 'require a URL input'],
  [['site.zip', '--output-dir', 'out', '-o', 'one.pagx'], 'exclusively'],
  [['page.html', '--html-root', 'site'], 'require ZIP input'],
  [['page.html', '--html-allow-origin', 'https://cdn.example'], 'require ZIP input'],
  [['https://example.test/site.zip', '--html-root', 'site', '-o', 'one.pagx'], 'require ZIP input'],
  [['site.zip', '--output-dir', 'out', '--html-allow-origin'], 'requires a value'],
  [['site.zip', '--output-dir', 'out', '--html-root='], 'requires a value'],
])('invalid ZIP arguments fail before invoking the runtime: %j', (args, message) => {
  const result = run(args);
  expect(result.status).toBe(2);
  expect(result.stderr).toContain(message);
  expect(result.stdout).toBe('');
  expect(fs.readdirSync(dir)).toEqual(['bridge.js']);
});
