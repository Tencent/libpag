// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { withCliLifecycle, trackCliResource, throwIfCancelled } = require('../dist/lib/cli-lifecycle');

test('the first signal wins and late resources are cleaned up once', async () => {
  const cleanup = jest.fn().mockRejectedValue(new Error('cleanup failed'));
  const late = jest.fn();
  const released = jest.fn();
  const listeners = [process.listenerCount('SIGINT'), process.listenerCount('SIGTERM')];
  await expect(withCliLifecycle(async () => {
    trackCliResource(cleanup);
    trackCliResource(released)();
    process.emit('SIGINT');
    process.emit('SIGTERM');
    trackCliResource(late);
    await Promise.resolve();
    return 'a cancelled result';
  })).rejects.toMatchObject({ exitCode: 130 });
  expect(cleanup).toHaveBeenCalledTimes(1);
  expect(late).toHaveBeenCalledTimes(1);
  expect(released).not.toHaveBeenCalled();
  expect([process.listenerCount('SIGINT'), process.listenerCount('SIGTERM')]).toEqual(listeners);
  expect(() => throwIfCancelled()).not.toThrow();
});

test('nested lifetimes are rejected without corrupting the outer lifetime', async () => {
  const error = new Error('ordinary failure');
  await expect(withCliLifecycle(async () => {
    await expect(withCliLifecycle(async () => 1)).rejects.toThrow('already active');
    throw error;
  })).rejects.toBe(error);
  expect(await withCliLifecycle(async () => 42)).toBe(42);
});

test('forced shutdown uses the cancellation exit code if cleanup stalls', async () => {
  jest.useFakeTimers();
  const exit = jest.spyOn(process, 'exit').mockImplementation(() => {});
  try {
    await expect(withCliLifecycle(async () => {
      process.emit('SIGTERM');
      await jest.advanceTimersByTimeAsync(9999);
      expect(exit).not.toHaveBeenCalled();
      await jest.advanceTimersByTimeAsync(1);
      expect(exit).toHaveBeenCalledWith(143);
    })).rejects.toMatchObject({ exitCode: 143 });
    expect(jest.getTimerCount()).toBe(0);
  } finally { exit.mockRestore(); jest.useRealTimers(); }
});
