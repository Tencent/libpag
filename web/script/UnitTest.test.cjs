// Copyright (C) 2026 Tencent. All rights reserved.
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const { Tick: Drain, CreateTimers } = require('./test-support/harness.cjs');

const Source = fs.readFileSync(path.join(__dirname, 'UnitTest.js'), 'utf8');

function CreateHarness(options = {}) {
  const server = new EventEmitter();
  const child = new EventEmitter();
  const fakeProcess = new EventEmitter();
  const clock = CreateTimers();
  const timers = clock.timers;
  const calls = { spawns: [], signals: [], errors: [], closes: 0, connectionsClosed: 0, unrefs: 0 };
  let listenCallback = null;
  child.pid = 1234;
  child.exitCode = null;
  child.signalCode = null;
  child.unref = function () {
    calls.unrefs++;
  };
  child.kill = function (signal) {
    calls.signals.push(signal);
    if (!options.ignoreTermination) Exit(null, signal);
  };
  fakeProcess.platform = 'linux';
  fakeProcess.execPath = '/mock/node';
  fakeProcess.argv = ['/mock/node', 'UnitTest.js'];
  fakeProcess.kill = function (pid, signal) {
    assert.equal(pid, -child.pid);
    child.kill(signal);
  };
  server.listen = function (address, callback) {
    calls.address = { ...address };
    listenCallback = callback;
  };
  server.close = function (callback) {
    calls.closes++;
    if (!options.hangClose) queueMicrotask(callback);
  };
  server.closeAllConnections = function () {
    calls.connectionsClosed++;
  };
  server.unref = child.unref;

  function Exit(code, signal = null) {
    child.exitCode = code;
    child.signalCode = signal;
    child.emit('close', code, signal);
  }
  function MockRequire(name) {
    if (name === 'cypress/package.json') {
      if (options.installedCypress) return require(name);
      return { bin: { cypress: options.cliPath || 'bin/cypress' } };
    }
    if (name === 'http-server') {
      return {
        createServer(config) {
          calls.serverOptions = { ...config };
          return { server };
        },
      };
    }
    if (name === 'node:child_process') {
      return {
        spawn(command, args, config) {
          calls.spawns.push({ command, args: Array.from(args), config: { ...config } });
          if (options.spawnError) throw options.spawnError;
          return child;
        },
      };
    }
    return require(name);
  }
  MockRequire.resolve = function (name) {
    assert.equal(name, 'cypress/package.json');
    if (options.installedCypress) return require.resolve(name);
    return '/mock/cypress/package.json';
  };
  const fakeModule = { exports: {} };
  const context = {
    __dirname,
    module: fakeModule,
    require: MockRequire,
    process: fakeProcess,
    console: {
      log() {},
      error(error) {
        calls.errors.push(String(error));
      },
    },
    setTimeout: clock.setTimeout,
    clearTimeout: clock.clearTimeout,
  };
  new vm.Script(Source, { filename: 'UnitTest.js' }).runInNewContext(context);
  return {
    run: fakeModule.exports.Run,
    server,
    child,
    process: fakeProcess,
    calls,
    timers,
    exit: Exit,
    listen() {
      listenCallback();
    },
    timeout(delay) {
      for (const [id, timer] of timers) {
        if (timer.delay === delay) {
          timers.delete(id);
          timer.callback();
          return;
        }
      }
      assert.fail(`No pending ${delay}ms timeout.`);
    },
  };
}

function AssertClean(harness) {
  assert.equal(harness.timers.size, 0);
  assert.equal(harness.process.listenerCount('SIGINT'), 0);
  assert.equal(harness.process.listenerCount('SIGTERM'), 0);
  assert.equal(harness.server.listenerCount('error'), 0);
}

test('the default WebGL runner waits for its exclusive loopback server and returns success', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  assert.equal(harness.calls.spawns.length, 0);
  assert.deepEqual(harness.calls.address, { host: '127.0.0.1', port: 8080, exclusive: true });
  assert.equal(harness.calls.serverOptions.root, path.resolve(__dirname, '..'));
  assert.equal(harness.calls.serverOptions.cache, -1);
  harness.listen();
  await Drain();
  assert.deepEqual(harness.calls.spawns[0], {
    command: '/mock/node',
    args: ['/mock/cypress/bin/cypress', 'run', '--browser', 'electron', '--headless', '--env', 'BACKEND=webgl'],
    config: { cwd: path.resolve(__dirname, '..'), stdio: 'inherit', detached: true },
  });
  harness.exit(0);
  assert.equal(await result, 0);
  assert.equal(harness.calls.closes, 1);
  assert.equal(harness.calls.connectionsClosed, 1);
  AssertClean(harness);
});

test('WebGPU uses Chrome and preserves the failing Cypress exit code', async function () {
  const harness = CreateHarness();
  const result = harness.run(['--backend=webgpu']);
  harness.listen();
  await Drain();
  assert.deepEqual(harness.calls.spawns[0].args, [
    '/mock/cypress/bin/cypress',
    'run',
    '--browser',
    'chrome',
    '--headless',
    '--env',
    'BACKEND=webgpu',
  ]);
  harness.exit(7);
  assert.equal(await result, 7);
  assert.equal(harness.calls.closes, 1);
  AssertClean(harness);
});

test('the CLI path follows the executable declared by the package', async function () {
  const harness = CreateHarness({ cliPath: 'custom/cli.js' });
  const result = harness.run();
  harness.listen();
  await Drain();
  harness.exit(0);
  assert.equal(await result, 0);
  assert.equal(harness.calls.spawns[0].args[0], '/mock/cypress/custom/cli.js');
  AssertClean(harness);
});

test('the installed Cypress package resolves its CLI without importing a private subpath', async function () {
  const harness = CreateHarness({ installedCypress: true });
  const result = harness.run();
  if (harness.calls.address) {
    harness.listen();
    await Drain();
    harness.exit(0);
  }
  assert.equal(await result, 0, harness.calls.errors.join('\n'));
  const packagePath = require.resolve('cypress/package.json');
  const declaredCLI = require('cypress/package.json').bin.cypress;
  const executable = path.resolve(path.dirname(packagePath), declaredCLI);
  assert.equal(harness.calls.spawns[0].args[0], executable);
  assert.equal(fs.statSync(executable).isFile(), true);
  AssertClean(harness);
});

test('invalid or repeated backend options fail without starting anything', async function () {
  for (const args of [['--backend=vulkan'], ['--backend=webgl', '--backend=webgpu'], ['--unknown']]) {
    const harness = CreateHarness();
    assert.equal(await harness.run(args), 1);
    assert.equal(harness.calls.spawns.length, 0);
    assert.equal(harness.calls.address, undefined);
    assert.match(harness.calls.errors[0], /Usage:/);
    AssertClean(harness);
  }
});

test('port conflicts fail without connecting Cypress to another server', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  harness.server.emit('error', Object.assign(new Error('Port 8080 is already in use.'), { code: 'EADDRINUSE' }));
  assert.equal(await result, 1);
  assert.equal(harness.calls.spawns.length, 0);
  assert.equal(harness.calls.closes, 1);
  assert.match(harness.calls.errors[0], /already in use/);
  AssertClean(harness);
});

test('server readiness has a bounded timeout', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  harness.timeout(10000);
  assert.equal(await result, 1);
  assert.equal(harness.calls.spawns.length, 0);
  assert.match(harness.calls.errors[0], /Timed out starting/);
  AssertClean(harness);
});

test('synchronous spawn failures close the server and fail the run', async function () {
  const harness = CreateHarness({ spawnError: new Error('Cannot spawn Cypress.') });
  const result = harness.run();
  harness.listen();
  assert.equal(await result, 1);
  assert.equal(harness.calls.closes, 1);
  AssertClean(harness);
});

test('asynchronous spawn errors are not mistaken for successful exits', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  harness.listen();
  await Drain();
  harness.child.pid = undefined;
  harness.child.emit('error', new Error('ENOENT'));
  harness.exit(null);
  assert.equal(await result, 1);
  assert.match(harness.calls.errors[0], /ENOENT/);
  AssertClean(harness);
});

test('signal-only Cypress exits fail instead of returning a null success code', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  harness.listen();
  await Drain();
  harness.exit(null, 'SIGTERM');
  assert.equal(await result, 1);
  AssertClean(harness);
});

test('runtime server failures terminate the Cypress process group', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  harness.listen();
  await Drain();
  harness.server.emit('error', new Error('Server failed.'));
  assert.equal(await result, 1);
  assert.deepEqual(harness.calls.signals, ['SIGTERM']);
  AssertClean(harness);
});

test('interruptions during server startup fail and remove signal handlers', async function () {
  const harness = CreateHarness();
  const result = harness.run();
  harness.process.emit('SIGINT');
  assert.equal(await result, 130);
  assert.equal(harness.calls.spawns.length, 0);
  AssertClean(harness);
});

test('termination escalates and cleanup stays bounded with unresponsive child and server', async function () {
  const harness = CreateHarness({ ignoreTermination: true, hangClose: true });
  const result = harness.run();
  harness.listen();
  await Drain();
  harness.process.emit('SIGTERM');
  await Drain();
  assert.deepEqual(harness.calls.signals, ['SIGTERM']);
  harness.timeout(5000);
  harness.timeout(5000);
  assert.equal(await result, 143);
  assert.deepEqual(harness.calls.signals, ['SIGTERM', 'SIGKILL']);
  assert.equal(harness.calls.unrefs, 2);
  AssertClean(harness);
});
