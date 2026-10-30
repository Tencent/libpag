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

function CreateHarness(backend, platform = 'linux') {
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
  const context = { exports: {}, require: MockRequire, process: { platform }, Buffer };
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
});

test('WebGPU rejects unsupported launchers before running tests', function () {
  const launch = CreateHarness('webgpu').handlers.get('before:browser:launch');
  for (const browser of [{ name: 'electron', family: 'chromium' }, { name: 'firefox', family: 'firefox' }]) {
    assert.throws(function invalid() { launch(browser, { args: [] }); }, /requires Chrome/);
  }
});

test('a successful run records backend and the exact Cypress outcome', function () {
  const harness = CreateHarness('webgpu');
  const result = { status: 'finished', totalTests: 12, totalPassed: 11, totalPending: 1, totalFailed: 0 };
  harness.handlers.get('after:run')(result);
  const report = JSON.parse(harness.files.get(path.join('cypress/results/webgpu', 'summary.json')));
  assert.deepEqual(report, { backend: 'webgpu', ...result });
});

test('zero tests and entirely skipped or failed runs are not successful gates', function () {
  for (const result of [
    { status: 'failed', message: 'browser crashed' },
    { status: 'finished', totalTests: 0, totalPassed: 0 },
    { status: 'finished', totalTests: 12, totalPassed: 0, totalPending: 12 },
    { status: 'finished', totalTests: 12, totalPassed: 0, totalFailed: 12 },
  ]) {
    const harness = CreateHarness('webgl');
    assert.throws(function failed() { harness.handlers.get('after:run')(result); }, /did not complete/);
    assert.ok(harness.files.has(path.join('cypress/results/webgl', 'summary.json')));
  }
});

test('mixed failures remain visible in the report without changing the Cypress result', function () {
  const harness = CreateHarness('webgl');
  const result = { status: 'finished', totalTests: 3, totalPassed: 2, totalFailed: 1 };
  harness.handlers.get('after:run')(Object.freeze(result));
  const report = JSON.parse(harness.files.get(path.join('cypress/results/webgl', 'summary.json')));
  assert.equal(report.totalFailed, 1);
  assert.equal(result.totalFailed, 1);
});
