// Copyright (C) 2026 Tencent. All rights reserved.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const { ArtifactNames, Publish, ReadBuild, SnapshotBuild, HashText, ValidateBuildInfo, ValidateCache,
  ValidateResult } = require('./artifacts');
const { ParseOptions, StartServer, CloseResources, OutputPath, CollectFiles, CommitCache } = require('./run_test');
const { ValidateProvenance, ValidateSource } = require('./cache-source');

function TempDirectory(t) {
  const root = path.join(__dirname, 'results');
  fs.mkdirSync(root, { recursive: true });
  const directory = fs.mkdtempSync(path.join(root, 'unit-'));
  t.after(function cleanup() { fs.rmSync(directory, { recursive: true, force: true }); });
  return directory;
}

function BuildInfo(backend = 'webgl', mode = 'compare') {
  return { protocol: 1, backend, mode, buildId: 'a'.repeat(32), sourceRevision: 'b'.repeat(40),
    tgfxRevision: 'c'.repeat(40), configuration: 'Release' };
}

function Result() {
  return { exitCode: 0, tests: 2, failed: 0, skipped: 0,
    comparisons: { compared: 1, versionSkipped: 1, missingBaseline: 0 } };
}

function Cache() {
  return { md5: JSON.stringify({ suite: { image: 'd'.repeat(32) } }),
    version: JSON.stringify({ suite: { image: 'v1' } }) };
}

function SuccessfulUpdate(cache = Cache()) {
  const buildInfo = BuildInfo('webgl', 'update');
  const run = { buildInfo, result: Result(), filter: '', errors: [],
    source: { sourceRevision: buildInfo.sourceRevision, tgfxRevision: buildInfo.tgfxRevision,
      versionHash: HashText(cache.version), ref: 'origin/main', clean: true }, cacheHashes: {} };
  for (const name of ['md5', 'version', 'head']) {
    run.cacheHashes[name] = HashText(name === 'head' ? 'bbbbbbb' : cache[name]);
  }
  return run;
}

function MakeBuild(root, info) {
  const build = path.join(root, 'build');
  fs.mkdirSync(build, { recursive: true });
  for (const name of ArtifactNames) {
    fs.writeFileSync(path.join(name.startsWith('pag-test.') ? build : root, name), `fixture:${name}`);
  }
  fs.writeFileSync(path.join(build, 'build-info.json'), JSON.stringify(info));
  fs.writeFileSync(path.join(build, 'tgfx-binding-path.txt'), path.join(root, 'tgfx.esm.js'));
  return build;
}

function WriteCache(directory, cache) {
  fs.mkdirSync(directory, { recursive: true });
  fs.writeFileSync(path.join(directory, 'md5.json'), cache.md5);
  fs.writeFileSync(path.join(directory, 'version.json'), cache.version);
  fs.writeFileSync(path.join(directory, 'HEAD'), 'bbbbbbb');
}

test('CLI selects explicit backends and rejects malformed or filtered updates', function () {
  assert.equal(ParseOptions(['--backend', 'webgpu', '--filter=Suite.*']).backend, 'webgpu');
  assert.equal(ParseOptions(['--filter', 'Suite.*']).filter, 'Suite.*');
  for (const args of [['--backend'], ['--backend=metal'], ['--unknown'], ['--filter='],
    ['--mode=update', '--filter=Suite.*']]) {
    assert.throws(function parse() { ParseOptions(args); });
  }
});

test('build identity rejects stale generation mode and wrong backend', function () {
  assert.doesNotThrow(function valid() { ValidateBuildInfo(BuildInfo(), 'webgl', 'compare'); });
  assert.throws(function mode() { ValidateBuildInfo(BuildInfo('webgl', 'update'), 'webgl', 'compare'); });
  assert.throws(function backend() { ValidateBuildInfo(BuildInfo('webgpu'), 'webgl', 'compare'); });
  assert.throws(function missing() { ValidateBuildInfo({}, 'webgl', 'compare'); });
});

test('publishing isolates backends and verifies the entire artifact set', function (t) {
  const root = TempDirectory(t);
  const build = MakeBuild(root, BuildInfo());
  const gl = Publish('webgl', 'compare', build, root);
  const first = ReadBuild(gl, 'webgl', 'compare');
  fs.writeFileSync(path.join(build, 'build-info.json'), JSON.stringify(BuildInfo('webgpu')));
  const gpu = Publish('webgpu', 'compare', build, root);
  assert.notEqual(gl, gpu);
  assert.deepEqual(ReadBuild(gl, 'webgl', 'compare'), first);
  fs.appendFileSync(path.join(gpu, 'pag-test.wasm'), 'corrupt');
  assert.throws(function mismatched() { ReadBuild(gpu, 'webgpu', 'compare'); }, /hash mismatch/);
});

test('failed publishing leaves previous artifacts intact', function (t) {
  const root = TempDirectory(t);
  const build = MakeBuild(root, BuildInfo());
  const destination = Publish('webgl', 'compare', build, root);
  const before = ReadBuild(destination, 'webgl', 'compare');
  fs.unlinkSync(path.join(build, 'pag-test.data'));
  assert.throws(function missing() { Publish('webgl', 'compare', build, root); });
  assert.deepEqual(ReadBuild(destination, 'webgl', 'compare'), before);
});

test('cache validation rejects empty malformed and unversioned data', function () {
  assert.doesNotThrow(function valid() { ValidateCache(Cache()); });
  for (const cache of [{ md5: '{}', version: '{}' }, { md5: '{', version: '{}' },
    { md5: JSON.stringify({ suite: { missing: 'd'.repeat(32) } }), version: Cache().version },
    { md5: '{"suite":{"image":42}}', version: Cache().version }]) {
    assert.throws(function invalid() { ValidateCache(cache); });
  }
});

test('zero tests and an all-skipped run cannot succeed', function () {
  assert.doesNotThrow(function valid() { ValidateResult(Result()); });
  for (const result of [{ ...Result(), tests: 0 }, { ...Result(), skipped: 2 },
    { ...Result(), tests: -1 }, { ...Result(), failed: 1 },
    { ...Result(), comparisons: { compared: 0, versionSkipped: 0, missingBaseline: 1 } }]) {
    assert.throws(function invalid() { ValidateResult(result); });
  }
});

test('version skips are reported separately and failing runs retain diagnostics', function () {
  const result = Result();
  result.comparisons.compared = 0;
  assert.doesNotThrow(function skipped() { ValidateResult(result); });
  result.exitCode = 1;
  result.failed = 1;
  result.comparisons.missingBaseline = 1;
  assert.doesNotThrow(function failed() { ValidateResult(result); });
});

test('artifact paths cannot escape the run or overwrite a comparison cache', function (t) {
  const root = TempDirectory(t);
  assert.equal(OutputPath(root, '/test/out/image.webp', 'webgl', 'compare'), path.join(root, 'out/image.webp'));
  for (const file of ['/test/out/../../baseline/version.json', '/test/baseline/.cache/webgl/md5.json',
    '/test/out/..\\evil', '/etc/passwd', '/test/out/\0bad']) {
    assert.throws(function invalid() { OutputPath(root, file, 'webgl', 'compare'); });
  }
  assert.throws(function wrongBackend() {
    OutputPath(root, '/test/baseline/.cache/webgpu/md5.json', 'webgl', 'update');
  });
});

test('artifacts are transferred completely in bounded chunks', async function (t) {
  const root = TempDirectory(t);
  const data = Buffer.alloc(600000, 42);
  let reads = 0;
  const page = { async evaluate(fn, ...args) {
    if (fn.name === 'ListFiles') return [{ path: '/test/out/image.webp', size: data.length }];
    reads++;
    assert.ok(args[2] <= 256 * 1024);
    return data.subarray(args[1], args[1] + args[2]).toString('base64');
  } };
  const files = await CollectFiles(page, root, { backend: 'webgl', mode: 'compare' });
  assert.equal(files.length, 1);
  assert.equal(reads, 3);
  assert.deepEqual(fs.readFileSync(path.join(root, 'out/image.webp')), data);
});

test('short artifact transfers fail instead of reporting success', async function (t) {
  const root = TempDirectory(t);
  const page = { async evaluate(fn) {
    return fn.name === 'ListFiles' ? [{ path: '/test/report.json', size: 10 }] : 'eA==';
  } };
  await assert.rejects(CollectFiles(page, root, { backend: 'webgl', mode: 'compare' }), /Incomplete artifact/);
});

test('cache publication refuses failed or incomplete runs without changing old expectations', function (t) {
  const root = TempDirectory(t);
  const old = path.join(root, 'old');
  const source = path.join(root, 'source');
  WriteCache(old, Cache());
  WriteCache(source, Cache());
  const run = { result: { ...Result(), exitCode: 1, failed: 1 }, errors: [], buildInfo: BuildInfo('webgl', 'update') };
  assert.throws(function failed() { CommitCache(source, old, Cache().version, run); });
  run.result = Result();
  fs.unlinkSync(path.join(source, 'md5.json'));
  assert.throws(function missing() { CommitCache(source, old, Cache().version, run); });
  assert.equal(fs.readFileSync(path.join(old, 'md5.json'), 'utf8'), Cache().md5);
});

test('cache publication replaces digest and version together and records provenance', function (t) {
  const root = TempDirectory(t);
  const old = path.join(root, 'old');
  const source = path.join(root, 'source');
  WriteCache(old, Cache());
  const next = { md5: JSON.stringify({ suite: { image: 'e'.repeat(32) } }),
    version: JSON.stringify({ suite: { image: 'v2' } }) };
  WriteCache(source, next);
  const run = SuccessfulUpdate(next);
  CommitCache(source, old, next.version, run);
  assert.equal(fs.readFileSync(path.join(old, 'md5.json'), 'utf8'), next.md5);
  assert.equal(fs.readFileSync(path.join(old, 'version.json'), 'utf8'), next.version);
  assert.deepEqual(JSON.parse(fs.readFileSync(path.join(old, 'provenance.json'), 'utf8')), run);
});

test('HTTP server is loopback-only and serves only named build artifacts', async function (t) {
  const root = TempDirectory(t);
  fs.writeFileSync(path.join(root, 'index.html'), '<html></html>');
  fs.writeFileSync(path.join(root, 'secret.txt'), 'not published');
  const server = await StartServer(root);
  try {
    assert.equal(server.address().address, '127.0.0.1');
    const base = `http://127.0.0.1:${server.address().port}`;
    assert.equal((await fetch(`${base}/index.html`)).status, 200);
    assert.equal((await fetch(`${base}/secret.txt`)).status, 404);
    assert.equal((await fetch(`${base}/%2e%2e/secret.txt`)).status, 404);
    assert.equal((await fetch(`${base}/index.html`, { method: 'POST' })).status, 404);
  } finally {
    await new Promise(function close(resolve) { server.close(resolve); });
  }
});

test('run snapshots remain valid after publishing a new build with the same identity', function (t) {
  const root = TempDirectory(t);
  const build = MakeBuild(root, BuildInfo());
  const published = Publish('webgl', 'compare', build, root);
  const snapshot = path.join(root, 'snapshot');
  const before = SnapshotBuild(published, snapshot, 'webgl', 'compare');
  fs.appendFileSync(path.join(build, 'pag-test.wasm'), 'incremental');
  Publish('webgl', 'compare', build, root);
  assert.deepEqual(ReadBuild(snapshot, 'webgl', 'compare'), before);
  assert.notDeepEqual(ReadBuild(published, 'webgl', 'compare').files, before.files);
});

test('snapshot validation rejects content changed before the copy completes', function (t) {
  const root = TempDirectory(t);
  const published = Publish('webgl', 'compare', MakeBuild(root, BuildInfo()), root);
  fs.appendFileSync(path.join(published, 'pag-test.js'), 'mismatched');
  assert.throws(function copy() {
    SnapshotBuild(published, path.join(root, 'snapshot'), 'webgl', 'compare');
  }, /hash mismatch/);
});

test('baseline provenance rejects wrong backends and tampered digests or source versions', function () {
  const cache = { ...Cache(), head: 'bbbbbbb' };
  const run = SuccessfulUpdate();
  assert.doesNotThrow(function valid() { ValidateProvenance(cache, run, 'webgl', run.source); });
  assert.throws(function backend() { ValidateProvenance(cache, run, 'webgpu', run.source); });
  assert.throws(function md5() {
    ValidateProvenance({ ...cache, md5: '{}' }, run, 'webgl', run.source);
  }, /integrity/);
  assert.throws(function version() {
    ValidateProvenance(cache, run, 'webgl', { ...run.source, versionHash: 'wrong' });
  }, /version mismatch/);
  assert.throws(function old() { ValidateProvenance(cache, {}, 'webgl', run.source); });
});

test('a non-main TGFX revision and filtered or dirty baseline sources are rejected', function () {
  const cache = { ...Cache(), head: 'bbbbbbb' };
  const run = SuccessfulUpdate();
  assert.throws(function dependency() {
    ValidateSource({ ...run.buildInfo, tgfxRevision: 'd'.repeat(40) }, run.source);
  }, /pinned TGFX/);
  for (const bad of [{ ...run, filter: 'Suite.*' }, { ...run, errors: ['failed'] },
    { ...run, source: { ...run.source, clean: false } }]) {
    assert.throws(function invalid() { ValidateProvenance(cache, bad, 'webgl', run.source); });
  }
});

test('backup cleanup failure does not turn a committed cache into a failed run', function (t) {
  const root = TempDirectory(t);
  const old = path.join(root, 'old');
  const source = path.join(root, 'source');
  WriteCache(old, Cache());
  WriteCache(source, Cache());
  const original = fs.rmSync;
  const mock = t.mock.method(fs, 'rmSync', function remove(file, options) {
    if (String(file).endsWith('.previous')) throw new Error('simulated cleanup failure');
    return original(file, options);
  });
  const warn = t.mock.method(console, 'warn', function warning() {});
  try {
    assert.doesNotThrow(function commit() { CommitCache(source, old, Cache().version, SuccessfulUpdate()); });
    assert.ok(fs.existsSync(path.join(old, 'provenance.json')));
    assert.equal(warn.mock.calls.length, 1);
    assert.ok(fs.readdirSync(root).some(function backup(name) { return name.endsWith('.previous'); }));
  } finally {
    mock.mock.restore();
    warn.mock.restore();
  }
});

test('cleanup times out stalled browser shutdown and kills only its child', async function () {
  let killed = false;
  let connectionsClosed = false;
  const browser = {
    close() { return new Promise(function stalled() {}); },
    process() { return { kill(signal) { assert.equal(signal, 'SIGKILL'); killed = true; } }; },
  };
  const server = {
    close(done) { done(); },
    closeAllConnections() { connectionsClosed = true; },
  };
  const errors = await CloseResources(browser, server, 10);
  assert.deepEqual(errors, ['Browser cleanup timed out']);
  assert.equal(killed, true);
  assert.equal(connectionsClosed, true);
});

test('cleanup also bounds a stalled HTTP close', async function () {
  const server = { close() {}, closeAllConnections() {} };
  assert.deepEqual(await CloseResources(null, server, 10), ['HTTP server cleanup timed out']);
});

async function FixtureModule() {
  const config = window.__pagTestConfig;
  const files = new Map([['/test/baseline/version.json', config.fixtureVersion]]);
  const fs = {
    readFile(name) {
      if (!files.has(name)) throw new Error(`Missing fixture file: ${name}`);
      return files.get(name);
    },
    writeFile(name, value) { files.set(name, value); },
    mkdirTree() {},
    analyzePath(name) { return { exists: files.has(name) }; },
    lstat(name) { return { mode: 1, size: new TextEncoder().encode(files.get(name)).length }; },
    isDir() { return false; },
    isFile() { return true; },
    open(name) { return name; },
    close() {},
    read(name, bytes, offset, length, position) {
      const source = new TextEncoder().encode(files.get(name)).subarray(position, position + length);
      bytes.set(source, offset);
      return source.length;
    },
  };
  return {
    FS: fs,
    _registerSoftwareDecoderFactory() {},
    ccall(name) {
      if (name === 'GetBuildInfo') {
        return JSON.stringify(config.scenario === 'identity' ? { ...config.buildInfo, buildId: 'f'.repeat(32) } : config.buildInfo);
      }
      if (name === 'GetRunResult') return JSON.stringify(config.fixtureResult);
      if (config.scenario === 'crash') throw new Error('Simulated native abort');
      if (config.scenario !== 'report') {
        fs.writeFile('/test/report.json', JSON.stringify({ tests: config.fixtureResult.tests,
          disabled: 0, failures: config.fixtureResult.failed }));
      }
      return config.fixtureResult.exitCode;
    },
  };
}

function InstallFixture(config) {
  window.__pagTestConfig = config;
  window.__scopePops = 0;
  const scopes = [];
  const device = {
    lost: new Promise(function pending() {}),
    addEventListener() {},
    pushErrorScope(type) { scopes.push(type); },
    async popErrorScope() {
      await new Promise(function defer(resolve) { setTimeout(resolve, 10); });
      const type = scopes.pop();
      window.__scopePops++;
      return config.scenario === 'gpu-error' && type === 'validation' ? { message: 'fixture validation' } : null;
    },
    queue: { async onSubmittedWorkDone() {} },
  };
  Object.defineProperty(navigator, 'gpu', { value: {
    async requestAdapter() { return { async requestDevice() { return device; } }; },
  } });
}

if (process.env.PAG_HARNESS_BROWSER === '1') {
  test('browser executes the actual harness page with isolated protocol fixtures', async function (t) {
    const browser = await require('puppeteer').launch({ headless: true });
    try {
      for (const scenario of ['webgl', 'webgpu', 'identity', 'crash', 'report', 'gpu-error']) {
        await t.test(scenario, async function (subtest) {
          const root = TempDirectory(subtest);
          const backend = scenario === 'webgpu' || scenario === 'gpu-error' ? 'webgpu' : 'webgl';
          const buildInfo = BuildInfo(backend);
          fs.copyFileSync(path.join(__dirname, 'index.html'), path.join(root, 'index.html'));
          fs.writeFileSync(path.join(root, 'build-info.json'), JSON.stringify(buildInfo));
          fs.writeFileSync(path.join(root, 'pag-test.js'), `export default ${FixtureModule.toString()}`);
          fs.writeFileSync(path.join(root, 'tgfx.esm.js'), 'export function TGFXBind() {}');
          fs.writeFileSync(path.join(root, 'ffavc.min.js'),
            'window.ffavc = { async FFAVCInit() { return { FFAVCDecoderFactory: class {} }; } };');
          const server = await StartServer(root);
          const page = await browser.newPage();
          try {
            const config = { buildInfo, scenario, cache: Cache(), filter: '', baselineVersion: Cache().version,
              fixtureVersion: Cache().version, fixtureResult: Result() };
            await page.evaluateOnNewDocument(InstallFixture, config);
            await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
            await page.waitForFunction(function finished() { return window.__pagTest?.finished; }, { timeout: 10000 });
            const state = await page.evaluate(function read() { return window.__pagTest; });
            if (scenario === 'identity' || scenario === 'crash' || scenario === 'gpu-error') {
              assert.ok(state.errors.length > 0);
            } else {
              assert.deepEqual(state.errors, []);
              assert.deepEqual(state.result, Result());
              const output = path.join(root, 'output');
              fs.mkdirSync(output);
              await CollectFiles(page, output, { backend, mode: 'compare' });
              if (scenario === 'report') {
                assert.throws(function missing() { fs.readFileSync(path.join(output, 'report.json')); });
              } else {
                assert.equal(JSON.parse(fs.readFileSync(path.join(output, 'report.json'))).tests, 2);
              }
            }
            if (backend === 'webgpu') {
              assert.equal(await page.evaluate(function pops() { return window.__scopePops; }), 3);
            }
          } finally {
            await page.close();
            assert.deepEqual(await CloseResources(null, server), []);
          }
        });
      }
    } finally {
      assert.deepEqual(await CloseResources(browser, null), []);
    }
  });
}
