// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { ConversionTask, ConversionTimeoutError, conversionLimits } = require('../dist/lib/conversion-task');

afterEach(() => { jest.restoreAllMocks(); jest.useRealTimers(); });

test('validates a single total budget and an earlier optional-wait deadline', () => {
  expect(conversionLimits({ totalMs: 180000, settleMs: 120000 })).toEqual({ totalMs: 180000, settleMs: 120000 });
  for (const totalMs of [0, -1, NaN, Infinity, 1.5, 2147483648]) {
    expect(() => conversionLimits({ totalMs })).toThrow();
  }
  expect(() => conversionLimits({ totalMs: 100, settleMs: 100 })).toThrow();
});

test('the soft deadline preserves an explicit wait and later stages use the remaining total', async () => {
  jest.useFakeTimers();
  const task = new ConversionTask({ totalMs: 1000, settleMs: 100 });
  try {
    let completed = false;
    const wait = task.wait(300).then(() => { completed = true; });
    await jest.advanceTimersByTimeAsync(100);
    expect(task.settleExpired).toBe(true);
    expect(completed).toBe(false);
    await jest.advanceTimersByTimeAsync(200);
    await wait;
    task.checkpoint('import');
    expect(task.remaining()).toBe(700);
    await jest.advanceTimersByTimeAsync(200);
    task.checkpoint('embed');
    expect(task.remaining()).toBe(500);
  } finally { await task.finish(); }
  expect(jest.getTimerCount()).toBe(0);
});

test('hard cancellation rejects blocked work and a late completion cannot start publication', async () => {
  jest.useFakeTimers();
  const task = new ConversionTask({ totalMs: 100, settleMs: 30 });
  let complete;
  const blocked = new Promise(resolve => { complete = resolve; });
  const publish = jest.fn();
  const result = task.run(async () => {
    await blocked;
    task.checkpoint('publish');
    publish();
  }).catch(error => error);
  await jest.advanceTimersByTimeAsync(100);
  expect(await result).toBeInstanceOf(ConversionTimeoutError);
  complete();
  await jest.advanceTimersByTimeAsync(0);
  expect(publish).not.toHaveBeenCalled();
  await task.finish();
  expect(jest.getTimerCount()).toBe(0);
});

test('publication checks the monotonic clock even before the timer callback runs', async () => {
  jest.useFakeTimers();
  const task = new ConversionTask({ totalMs: 100, settleMs: 30 });
  jest.spyOn(performance, 'now').mockReturnValue(101);
  expect(() => task.checkpoint('publish')).toThrow(ConversionTimeoutError);
  await task.finish();
});

test('late resources close once and cleanup grace cannot report an unfinished close as success', async () => {
  jest.useFakeTimers();
  const task = new ConversionTask({ totalMs: 100, settleMs: 30 });
  let created, closed;
  const creation = new Promise(resolve => { created = resolve; });
  const closing = new Promise(resolve => { closed = resolve; });
  const close = jest.fn(() => closing);
  const use = jest.fn();
  const result = task.run(async () => {
    const resource = await task.own(() => creation, close);
    use(resource.value);
  }).catch(error => error);
  await jest.advanceTimersByTimeAsync(100);
  expect(await result).toBeInstanceOf(ConversionTimeoutError);
  const drained = task.finish(20);
  created({});
  await jest.advanceTimersByTimeAsync(20);
  expect(await drained).toBe(false);
  expect(use).not.toHaveBeenCalled();
  expect(close).toHaveBeenCalledTimes(1);
  closed();
  expect(await task.finish()).toBe(true);
  expect(jest.getTimerCount()).toBe(0);
});

test('one cancelled task leaves another active and retains the first termination reason', async () => {
  const first = new ConversionTask(), second = new ConversionTask();
  const error = new Error('page crashed');
  try {
    first.abort(error);
    first.abort(new Error('cleanup failed'));
    expect(() => first.checkpoint()).toThrow(error);
    expect(await second.run(async () => 'complete')).toBe('complete');
    expect(second.signal.aborted).toBe(false);
  } finally { await Promise.all([first.finish(), second.finish()]); }
});

test('a failed close preserves the conversion error and still closes other resources', async () => {
  const task = new ConversionTask();
  const failed = await task.own(async () => ({}), async () => { throw new Error('close failed'); });
  const close = jest.fn(async () => {});
  await task.own(async () => ({}), close);
  const conversionError = new Error('navigation failed');
  await expect(task.run(async () => {
    try { throw conversionError; } finally { await failed.close(); }
  })).rejects.toBe(conversionError);
  expect(await task.finish()).toBe(false);
  expect(close).toHaveBeenCalledTimes(1);
});

test('an enclosing task retains shared files until a child finishes late cleanup', async () => {
  jest.useFakeTimers();
  const parent = new ConversionTask(), child = new ConversionTask();
  let closed;
  await child.own(async () => ({}), () => new Promise(resolve => { closed = resolve; }));
  const stopping = child.finish(10);
  await jest.advanceTimersByTimeAsync(10);
  expect(await stopping).toBe(false);
  parent.trackCleanup(child.waitForCleanup());
  const removeSharedFiles = jest.fn(async () => {});
  parent.deferCleanup(removeSharedFiles);
  const parentStopping = parent.finish(10);
  await jest.advanceTimersByTimeAsync(10);
  expect(await parentStopping).toBe(false);
  expect(removeSharedFiles).not.toHaveBeenCalled();
  closed();
  expect(await parent.waitForCleanup()).toBe(true);
  expect(removeSharedFiles).toHaveBeenCalledTimes(1);
  expect(jest.getTimerCount()).toBe(0);
});

test('soft expiry only marks partial when optional preparation actually remains unfinished', async () => {
  jest.useFakeTimers();
  const task = new ConversionTask({ totalMs: 1000, settleMs: 100 });
  let release;
  const slow = new Promise(resolve => { release = resolve; });
  const result = task.prepare('network-idle', 500, () => slow);
  await jest.advanceTimersByTimeAsync(100);
  expect(await result).toBe(false);
  expect(task.readiness).toEqual({ status: 'partial', reason: 'prepare-timeout', stages: ['network-idle'] });
  release(true);
  await task.finish();
  const ready = new ConversionTask({ totalMs: 1000, settleMs: 100 });
  await ready.prepare('network-idle', 500, async () => true);
  await jest.advanceTimersByTimeAsync(200);
  expect(ready.readiness.status).toBe('ready');
  await ready.finish();
});
