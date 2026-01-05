// UnitTest.js
const { performance } = require('perf_hooks');
const spawn = require('cross-spawn');
const { AbortController } = require('abort-controller');

const controller = new AbortController();
const { signal } = controller;

const server = spawn('npm', ['run', 'server:cypress'], {
  stdio: 'inherit',
  signal,
});
server.on('error', err => {
  if (err.code !== 'ABORT_ERR') console.error(err);
});

const start = performance.now();
const cypress = spawn('npm', ['run', 'cypress'], { stdio: 'inherit' });

cypress.on('close', code => {
  const duration = ((performance.now() - start) / 1000).toFixed(3);
  console.log(`cypress exit code: ${code}, elapsed: ${duration}s`);
  controller.abort();
});
