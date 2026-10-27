// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
jest.mock('../dist/lib/browser-engine', () => ({
  ...jest.requireActual('../dist/lib/browser-engine'),
  launchBrowser: jest.fn(() => { throw new Error('unexpected browser launch'); }),
}));
const { launchBrowser } = require('../dist/lib/browser-engine');
const { validateImportRequest, runImportAdapter } = require('../dist/lib/import-adapter');
const fixture = path.join(__dirname, 'fixtures/interaction.html');
const scenario = path.join(__dirname, 'fixtures/interaction.scenario.json');
const request = { version: 1, operation: 'inspect', inputs: [{ id: '0', input: fixture }], options: { scenario } };
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-import-protocol-')); });
afterEach(() => { fs.rmSync(dir, { recursive: true, force: true }); jest.clearAllMocks(); });

test('inspect validates scenario and returns checkpoint order without browser resources', async () => {
  const output = path.join(dir, 'result.json');
  expect(await runImportAdapter(request, output)).toBe(0);
  expect(JSON.parse(fs.readFileSync(output))).toEqual({ version: 1, status: 'complete', checkpoints: ['initial', 'advanced'] });
  expect(launchBrowser).not.toHaveBeenCalled();
  expect(fs.readdirSync(dir)).toEqual(['result.json']);
});
test('invalid checkpoint and schema fail before browser/output mutation', async () => {
  const output = path.join(dir, 'result.json');
  await expect(runImportAdapter({ ...request, options: { scenario, checkpoint: 'absent' } }, output)).rejects.toThrow('Unknown checkpoint');
  expect(() => validateImportRequest({ ...request, version: 2 })).toThrow('version');
  expect(() => validateImportRequest({ ...request, inputs: [...request.inputs, ...request.inputs] })).toThrow('duplicate');
  expect(launchBrowser).not.toHaveBeenCalled();
  expect(fs.readdirSync(dir)).toEqual([]);
});
test('role locators are checked during inspect and credentials require URL input', () => {
  const file = path.join(dir, 'role.json');
  fs.writeFileSync(file, JSON.stringify({ version: 1, steps: [
    { type: 'click', target: { role: 'button', name: 'Open' } }, { type: 'capture', name: 'opened' },
  ] }));
  expect(() => validateImportRequest({ ...request, options: { scenario: file } })).toThrow('--html-browser-engine playwright');
  expect(() => validateImportRequest({ ...request, options: { scenario: file, browserEngine: 'playwright' } })).not.toThrow();
  expect(() => validateImportRequest({ ...request, options: { cookies: ['a=b'] } })).toThrow('URL');
  expect(() => validateImportRequest({ ...request, options: { cookies: ['invalid'] } })).toThrow('--html-cookie expects name=value');
  expect(() => validateImportRequest({ ...request, options: { headers: ['invalid\n: value'] } })).toThrow('--html-header expects Key: Value');
});
