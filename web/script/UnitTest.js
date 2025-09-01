// UnitTest.js
const { performance } = require('perf_hooks');
const spawn = require('cross-spawn');
const { AbortController } = require('abort-controller');

const controller = new AbortController();
const { signal } = controller;

// 1. 启动 server
const server = spawn('npm', ['run', 'server:cypress'], {
  stdio: 'inherit',
  signal,
});
server.on('error', err => {
  if (err.code !== 'ABORT_ERR') console.error(err);
});

// 2. 启动 cypress 并计时
const start = performance.now();
const cypress = spawn('npm', ['run', 'cypress'], { stdio: 'inherit' });

cypress.on('close', code => {
  const duration = ((performance.now() - start) / 1000).toFixed(3);
  console.log(`cypress exit code: ${code}, elapsed: ${duration}s`);
  controller.abort();      // 关闭 server
});