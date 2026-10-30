const { spawn } = require('node:child_process');
const path = require('node:path');
const { performance } = require('node:perf_hooks');

const WebRoot = path.resolve(__dirname, '..');
const ServerAddress = { host: '127.0.0.1', port: 8080, exclusive: true };
const StartupTimeout = 10000;
const CleanupTimeout = 5000;

function ParseBackend(args) {
  if (args.length === 0) return 'webgl';
  if (args.length === 1 && /^--backend=(webgl|webgpu)$/.test(args[0])) {
    return args[0].slice('--backend='.length);
  }
  throw new Error('Usage: node script/UnitTest.js [--backend=webgl|webgpu]');
}

function SignalChild(child, signal) {
  try {
    if (process.platform === 'win32') {
      child.kill(signal);
    } else {
      process.kill(-child.pid, signal);
    }
  } catch (error) {
    if (error.code !== 'ESRCH') console.error(error);
  }
}

function StopChild(child) {
  if (!child || !child.pid || child.exitCode !== null || child.signalCode !== null) {
    return Promise.resolve();
  }
  return new Promise(function (resolve) {
    const timer = setTimeout(function () {
      SignalChild(child, 'SIGKILL');
      child.unref();
      Finish();
    }, CleanupTimeout);
    function Finish() {
      clearTimeout(timer);
      child.removeListener('close', Finish);
      resolve();
    }
    child.once('close', Finish);
    SignalChild(child, 'SIGTERM');
  });
}

function CloseServer(server) {
  if (!server) return Promise.resolve();
  return new Promise(function (resolve) {
    const timer = setTimeout(function () {
      server.closeAllConnections();
      server.unref();
      Finish();
    }, CleanupTimeout);
    function Finish() {
      clearTimeout(timer);
      resolve();
    }
    server.close(Finish);
    server.closeAllConnections();
  });
}

async function Run(args = process.argv.slice(2)) {
  let server = null;
  let child = null;
  let startupTimer = null;
  let interruptCode = 0;
  let rejectRun = null;
  const start = performance.now();

  function Interrupt(signal) {
    interruptCode = signal === 'SIGINT' ? 130 : 143;
    rejectRun(new Error(`Cypress interrupted by ${signal}.`));
  }
  function OnInterrupt() {
    Interrupt('SIGINT');
  }
  function OnTerminate() {
    Interrupt('SIGTERM');
  }
  function OnServerError(error) {
    rejectRun(error);
  }

  try {
    const backend = ParseBackend(args);
    const cypressPath = require.resolve('cypress/bin/cypress');
    server = require('http-server').createServer({ root: WebRoot, cache: -1 }).server;
    const interrupted = new Promise(function (resolve, reject) {
      rejectRun = reject;
    });
    process.on('SIGINT', OnInterrupt);
    process.on('SIGTERM', OnTerminate);
    server.on('error', OnServerError);

    const listening = new Promise(function (resolve, reject) {
      startupTimer = setTimeout(function () {
        reject(new Error('Timed out starting the Cypress server at http://127.0.0.1:8080.'));
      }, StartupTimeout);
      server.listen(ServerAddress, resolve);
    });
    await Promise.race([listening, interrupted]);
    clearTimeout(startupTimer);

    const browser = backend === 'webgpu' ? 'chrome' : 'electron';
    child = spawn(
      process.execPath,
      [cypressPath, 'run', '--browser', browser, '--headless', '--env', `BACKEND=${backend}`],
      { cwd: WebRoot, stdio: 'inherit', detached: process.platform !== 'win32' },
    );
    const finished = new Promise(function (resolve, reject) {
      child.once('error', reject);
      child.once('close', function (code, signal) {
        if (signal) console.error(`Cypress terminated by ${signal}.`);
        resolve(Number.isInteger(code) && code >= 0 ? code : 1);
      });
    });
    const code = await Promise.race([finished, interrupted]);
    const duration = ((performance.now() - start) / 1000).toFixed(3);
    console.log(`Cypress ${backend} exit code: ${code}, elapsed: ${duration}s`);
    return code;
  } catch (error) {
    console.error(error);
    return interruptCode || 1;
  } finally {
    clearTimeout(startupTimer);
    await Promise.all([StopChild(child), CloseServer(server)]);
    process.removeListener('SIGINT', OnInterrupt);
    process.removeListener('SIGTERM', OnTerminate);
    if (server) server.removeListener('error', OnServerError);
  }
}

function SetExitCode(code) {
  process.exitCode = code;
}

module.exports = { Run };

if (require.main === module) {
  Run().then(SetExitCode);
}
