// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.

// Opt-in CLI lifetime. Library/server callers do not acquire process signal handlers.
// Browsers and native children register cleanup only while a CLI owns this scope.
type Cleanup = () => void | Promise<unknown>;
class CancelledError extends Error {
  readonly exitCode: number;
  constructor(signal: 'SIGINT' | 'SIGTERM') {
    super(`capture cancelled (${signal})`);
    this.exitCode = signal === 'SIGINT' ? 130 : 143;
  }
}
interface Scope {
  error?: CancelledError;
  cleanups: Set<Cleanup>;
}
let scope: Scope | undefined;

export function throwIfCancelled(): void {
  if (scope?.error) throw scope.error;
}

export function trackCliResource(cleanup: Cleanup): () => void {
  const owner = scope;
  if (!owner) return () => {};
  owner.cleanups.add(cleanup);
  if (owner.error) void Promise.resolve().then(cleanup).catch(() => {});
  return () => { owner.cleanups.delete(cleanup); };
}

export async function withCliLifecycle<T>(work: () => Promise<T>): Promise<T> {
  if (scope) throw new Error('a CLI lifecycle is already active');
  const owner: Scope = { cleanups: new Set() };
  scope = owner;
  let deadline: ReturnType<typeof setTimeout> | undefined;
  const cancel = (signal: 'SIGINT' | 'SIGTERM') => {
    if (owner.error) return;
    owner.error = new CancelledError(signal);
    for (const cleanup of owner.cleanups) {
      void Promise.resolve().then(cleanup).catch(() => {});
    }
    // Bound shutdown even if a browser driver never settles its pending operation.
    deadline = setTimeout(() => process.exit(owner.error!.exitCode), 10000);
    deadline.unref();
  };
  const interrupt = () => cancel('SIGINT');
  const terminate = () => cancel('SIGTERM');
  process.on('SIGINT', interrupt);
  process.on('SIGTERM', terminate);
  try {
    const result = await work();
    throwIfCancelled();
    return result;
  } catch (error) {
    throw owner.error || error;
  } finally {
    process.off('SIGINT', interrupt);
    process.off('SIGTERM', terminate);
    if (deadline) clearTimeout(deadline);
    scope = undefined;
  }
}
