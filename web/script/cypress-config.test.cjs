// Copyright (C) 2026 Tencent. All rights reserved.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const ts = require('typescript');

const Source = ts.transpileModule(fs.readFileSync(path.join(__dirname, '../cypress.config.ts'), 'utf8'), {
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
}).outputText;

function CreateHarness(backend, platform = 'linux', env = {}) {
  const files = new Map();
  const handlers = new Map();
  const mockFS = {
    mkdirSync() {},
    writeFileSync(name, data) { files.set(name, data); },
  };
  function MockRequire(name) {
    if (name === 'cypress') return { defineConfig(config) { return config; } };
    if (name === 'fs') return mockFS;
    if (name === 'path') return path;
    if (name === 'brotli') return {};
    throw new Error(`Unexpected dependency: ${name}`);
  }
  const context = { exports: {}, require: MockRequire, process: { platform, env }, Buffer };
  new vm.Script(Source, { filename: 'cypress.config.ts' }).runInNewContext(context);
  const definition = context.exports.default;
  function Register(event, handler) { handlers.set(event, handler); }
  const config = definition.e2e.setupNodeEvents(Register, { env: { BACKEND: backend } });
  return { config, definition, handlers, files };
}

test('both backends return explicit lane configuration and isolated diagnostics', function () {
  for (const backend of ['webgl', 'webgpu']) {
    const harness = CreateHarness(backend);
    assert.equal(harness.config.env.BACKEND, backend);
    assert.equal(harness.config.reporter, 'junit');
    assert.equal(harness.config.reporterOptions.mochaFile, path.join('cypress/results', backend, 'junit-[hash].xml'));
    assert.equal(harness.config.screenshotsFolder, path.join('cypress/screenshots', backend));
    assert.equal(harness.definition.e2e.screenshotOnRunFailure, true);
    assert.equal(harness.definition.e2e.baseUrl, 'http://127.0.0.1:8080/');
  }
});

test('unknown explicit backend fails instead of running WebGL', function () {
  assert.equal(CreateHarness(undefined).config.env.BACKEND, 'webgl');
  for (const backend of ['', 'webgup', 'metal', true]) {
    assert.throws(function invalid() { CreateHarness(backend); }, /Unsupported Cypress backend/);
  }
});

test('Linux WebGPU uses a software adapter without changing the WebGL lane', function () {
  const gpu = CreateHarness('webgpu');
  const launch = { args: [] };
  assert.equal(gpu.handlers.get('before:browser:launch')({ name: 'chrome', family: 'chromium' }, launch), launch);
  assert.deepEqual(launch.args, ['--enable-unsafe-webgpu', '--use-webgpu-adapter=swiftshader']);
  const gl = CreateHarness('webgl');
  const defaultLaunch = { args: [] };
  gl.handlers.get('before:browser:launch')({ name: 'electron', family: 'chromium' }, defaultLaunch);
  assert.deepEqual(defaultLaunch.args, []);
  const mac = CreateHarness('webgpu', 'darwin');
  const macLaunch = { args: [] };
  mac.handlers.get('before:browser:launch')({ name: 'chrome', family: 'chromium' }, macLaunch);
  assert.deepEqual(macLaunch.args, ['--enable-unsafe-webgpu']);
  const forced = CreateHarness('webgpu', 'darwin', { CYPRESS_WEBGPU_SWIFTSHADER: '1' });
  const forcedLaunch = { args: [] };
  forced.handlers.get('before:browser:launch')({ name: 'chrome', family: 'chromium' }, forcedLaunch);
  assert.deepEqual(forcedLaunch.args, ['--enable-unsafe-webgpu', '--use-webgpu-adapter=swiftshader']);
});

test('WebGPU rejects unsupported launchers before running tests', function () {
  const launch = CreateHarness('webgpu').handlers.get('before:browser:launch');
  for (const browser of [{ name: 'electron', family: 'chromium' }, { name: 'firefox', family: 'firefox' }]) {
    assert.throws(function invalid() { launch(browser, { args: [] }); }, /requires Chrome/);
  }
});

test('a completed run without a status field records the exact Cypress outcome', function () {
  const harness = CreateHarness('webgpu');
  const result = { totalTests: 12, totalPassed: 11, totalPending: 1, totalFailed: 0, totalSkipped: 0 };
  assert.doesNotThrow(function completed() { harness.handlers.get('after:run')(result); });
  const report = JSON.parse(harness.files.get(path.join('cypress/results/webgpu', 'summary.json')));
  assert.deepEqual(report, { backend: 'webgpu', ...result });
});

test('zero tests and entirely skipped runs are not successful gates', function () {
  for (const result of [
    { totalTests: 0, totalPassed: 0, totalPending: 0, totalSkipped: 0, totalFailed: 0 },
    { totalTests: 12, totalPassed: 0, totalPending: 12, totalSkipped: 0, totalFailed: 0 },
    { totalTests: 12, totalPassed: 0, totalPending: 0, totalSkipped: 12, totalFailed: 0 },
  ]) {
    const harness = CreateHarness('webgl');
    assert.throws(function failed() { harness.handlers.get('after:run')(result); }, /did not execute/);
    assert.ok(harness.files.has(path.join('cypress/results/webgl', 'summary.json')));
  }
});

test('test failures are reported without masking them with a plugin exception', function () {
  for (const result of [
    { totalTests: 178, totalPassed: 164, totalFailed: 1, totalPending: 13, totalSkipped: 0 },
    { totalTests: 12, totalPassed: 0, totalFailed: 12, totalPending: 0, totalSkipped: 0 },
  ]) {
    const harness = CreateHarness('webgl');
    assert.doesNotThrow(function failedTests() { harness.handlers.get('after:run')(Object.freeze(result)); });
    const report = JSON.parse(harness.files.get(path.join('cypress/results/webgl', 'summary.json')));
    assert.deepEqual(report, { backend: 'webgl', ...result });
  }
});

test('startup failures preserve the original error message in the report and exception', function () {
  const harness = CreateHarness('webgl');
  const result = { status: 'failed', failures: 1, message: 'browser crashed' };
  assert.throws(function crashed() { harness.handlers.get('after:run')(result); }, /could not run: browser crashed/);
  const report = JSON.parse(harness.files.get(path.join('cypress/results/webgl', 'summary.json')));
  assert.deepEqual(report, { backend: 'webgl', ...result });
});

test('missing or malformed counts are not accepted as a completed run', function () {
  const valid = { totalTests: 1, totalPassed: 1, totalFailed: 0, totalPending: 0, totalSkipped: 0 };
  for (const result of [{}, { ...valid, totalTests: -1 }, { ...valid, totalPassed: undefined },
    { ...valid, totalSkipped: 0.5 }]) {
    const harness = CreateHarness('webgl');
    assert.throws(function malformed() { harness.handlers.get('after:run')(result); }, /invalid test counts/);
  }
});
