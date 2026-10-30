// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { withCliLifecycle, trackCliResource, throwIfCancelled } = require('../dist/lib/cli-lifecycle');

test('SIGTERM closes registered resources and prevents subsequent work', async () => {
  const cleanup = jest.fn();
  const before = process.listenerCount('SIGTERM');
  await expect(withCliLifecycle(async () => {
    trackCliResource(cleanup);
    process.emit('SIGTERM');
    await Promise.resolve();
    throwIfCancelled();
  })).rejects.toMatchObject({ exitCode: 143 });
  expect(cleanup).toHaveBeenCalledTimes(1);
  expect(process.listenerCount('SIGTERM')).toBe(before);
  expect(() => throwIfCancelled()).not.toThrow();
});

test('completed resources and ordinary library calls do not acquire signal ownership', async () => {
  const cleanup = jest.fn();
  trackCliResource(cleanup);
  await withCliLifecycle(async () => { trackCliResource(cleanup)(); return 0; });
  expect(cleanup).not.toHaveBeenCalled();
});
