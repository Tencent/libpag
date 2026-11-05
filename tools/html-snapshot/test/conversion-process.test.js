// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const childProcess = require('node:child_process');
const { EventEmitter } = require('node:events');
const { ConversionTask } = require('../dist/lib/conversion-task');
const { runProcess } = require('../dist/lib/process-runner');

afterEach(() => { jest.restoreAllMocks(); jest.useRealTimers(); });

test('cancelled tasks never spawn a native process', async () => {
  const spawn = jest.spyOn(childProcess, 'spawn');
  const task = new ConversionTask();
  const reason = new Error('cancelled');
  task.abort(reason);
  try { expect(() => runProcess('/unused', [], { task })).toThrow(reason); }
  finally { await task.finish(); }
  expect(spawn).not.toHaveBeenCalled();
});

test('cancellation terminates a real child and waits for its actual exit', async () => {
  const original = childProcess.spawn;
  let ready, child;
  const started = new Promise(resolve => { ready = resolve; });
  jest.spyOn(childProcess, 'spawn').mockImplementation((...args) => {
    child = original(...args);
    child.stderr.on('data', () => ready());
    return child;
  });
  const task = new ConversionTask();
  const reason = new Error('request cancelled');
  try {
    const result = runProcess(process.execPath, ['-e', 'process.stderr.write("ready");setInterval(()=>{},1000)'], { task })
      .catch(error => error);
    await started;
    task.abort(reason);
    expect(await result).toBe(reason);
    expect(await task.finish()).toBe(true);
    expect(() => process.kill(child.pid, 0)).toThrow();
  } finally {
    if (child && child.exitCode === null && child.signalCode === null) child.kill('SIGKILL');
    await task.finish();
  }
});

test('a failed kill does not release files while the process may still use them', async () => {
  jest.useFakeTimers();
  const child = Object.assign(new EventEmitter(), {
    pid: 123, stdout: new EventEmitter(), stderr: new EventEmitter(),
    kill: jest.fn(() => { child.emit('error', new Error('EPERM')); return false; }),
  });
  jest.spyOn(childProcess, 'spawn').mockReturnValue(child);
  const task = new ConversionTask({ totalMs: 100, settleMs: 30 });
  const cleanup = jest.fn(async () => {});
  const result = runProcess('/fake', [], { task, killGraceMs: 10 }).catch(error => error);
  task.abort(new Error('cancelled'));
  task.deferCleanup(cleanup);
  await jest.advanceTimersByTimeAsync(10);
  expect(await result).toEqual(new Error('cancelled'));
  const finishing = task.finish(20);
  await jest.advanceTimersByTimeAsync(20);
  expect(await finishing).toBe(false);
  expect(cleanup).not.toHaveBeenCalled();
  child.emit('close', null);
  expect(await task.finish()).toBe(true);
  expect(cleanup).toHaveBeenCalledTimes(1);
  expect(jest.getTimerCount()).toBe(0);
});
