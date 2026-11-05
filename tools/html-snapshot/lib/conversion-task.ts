// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import { setTimeout as delay } from 'node:timers/promises';
import { trackCliResource } from './cli-lifecycle';

export interface ConversionLimits {
  totalMs?: number;
  settleMs?: number;
}

export interface ConversionReadiness {
  status: 'ready' | 'partial';
  reason?: 'prepare-timeout';
  stages: string[];
}

export class ConversionTimeoutError extends Error {
  readonly status = 408;
  constructor(readonly stage: string) {
    super(`HTML conversion timed out during ${stage}`);
    this.name = 'ConversionTimeoutError';
  }
}

export function conversionLimits(options: ConversionLimits = {}): Required<ConversionLimits> {
  const totalMs = options.totalMs ?? Number(process.env.HTML_SNAPSHOT_TIMEOUT_MS ?? 180000);
  const settleMs = options.settleMs ?? Number(process.env.HTML_SNAPSHOT_SETTLE_TIMEOUT_MS ?? Math.min(120000, Math.floor(totalMs * 2 / 3)));
  if (!Number.isSafeInteger(totalMs) || !Number.isSafeInteger(settleMs) ||
      settleMs <= 0 || totalMs <= settleMs || totalMs > 2147483647) {
    throw new Error('Conversion limits require integer milliseconds with 0 < settle < total <= 2147483647');
  }
  return { totalMs, settleMs };
}

export interface OwnedResource<T> {
  value: T;
  close(): Promise<void>;
}

/** One owner controls the deadline and cleanup, including resources created after cancellation. */
export class ConversionTask {
  private readonly controller = new AbortController();
  private readonly started = performance.now();
  private readonly deadline: number;
  private readonly settleDeadline: number;
  private readonly timer: ReturnType<typeof setTimeout>;
  private readonly cancelled: Promise<never>;
  private readonly cleanups = new Set<Promise<unknown>>();
  private readonly releaseCli: () => void;
  private cleanupFailed = false;
  private readonly incompletePreparation = new Set<string>();
  stage = 'source';

  constructor(options: ConversionLimits = {}, inherited = false) {
    const limits = inherited ? options as Required<ConversionLimits> : conversionLimits(options);
    this.deadline = this.started + limits.totalMs;
    this.settleDeadline = this.started + limits.settleMs;
    this.cancelled = new Promise((_, reject) => {
      this.signal.addEventListener('abort', () => reject(this.signal.reason), { once: true });
    });
    void this.cancelled.catch(() => {});
    this.timer = setTimeout(() => this.abort(new ConversionTimeoutError(this.stage)), limits.totalMs);
    this.releaseCli = trackCliResource(() => this.abort(new Error('HTML conversion cancelled')));
  }

  static fromEnvironment(): ConversionTask {
    const total = process.env.PAGX_TASK_REMAINING_MS;
    const settle = process.env.PAGX_TASK_SETTLE_REMAINING_MS;
    if (total === undefined && settle === undefined) return new ConversionTask();
    const totalMs = Number(total), settleMs = Number(settle);
    if (!Number.isFinite(totalMs) || !Number.isFinite(settleMs) || totalMs <= 0 || settleMs < 0 || settleMs > totalMs) {
      throw new Error('Invalid inherited conversion deadline');
    }
    return new ConversionTask({ totalMs, settleMs }, true);
  }

  get signal(): AbortSignal { return this.controller.signal; }
  get settleExpired(): boolean { return performance.now() >= this.settleDeadline; }
  get readiness(): ConversionReadiness {
    return this.incompletePreparation.size
      ? { status: 'partial', reason: 'prepare-timeout', stages: [...this.incompletePreparation] }
      : { status: 'ready', stages: [] };
  }

  markPreparationTimeout(stage: string): void { this.incompletePreparation.add(stage); }

  /** Optional readiness only. Explicit waits, actions, capture and conversion use the hard deadline. */
  async prepare(stage: string, maxMs: number, work: (timeoutMs: number) => Promise<boolean>): Promise<boolean> {
    const timeoutMs = this.settleRemaining(maxMs);
    if (!timeoutMs) { this.markPreparationTimeout(stage); return false; }
    let timer: ReturnType<typeof setTimeout> | undefined;
    try {
      const pending = work(timeoutMs);
      this.trackCleanup(pending.catch(() => {}));
      const ready = await Promise.race([
        pending,
        new Promise<false>(resolve => { timer = setTimeout(() => resolve(false), timeoutMs); }),
      ]);
      this.checkpoint();
      if (!ready && this.settleExpired) this.markPreparationTimeout(stage);
      return ready;
    } finally { if (timer) clearTimeout(timer); }
  }

  checkpoint(stage?: string): void {
    if (!this.signal.aborted && performance.now() >= this.deadline) this.abort(new ConversionTimeoutError(this.stage));
    this.signal.throwIfAborted();
    if (stage) this.stage = stage;
  }

  remaining(maxMs = Infinity): number {
    this.checkpoint();
    return Math.max(1, Math.ceil(Math.min(maxMs, this.deadline - performance.now())));
  }

  settleRemaining(maxMs = Infinity): number {
    this.checkpoint();
    return Math.max(0, Math.ceil(Math.min(maxMs, this.settleDeadline - performance.now())));
  }

  childEnvironment(): NodeJS.ProcessEnv {
    return { ...process.env, PAGX_TASK_REMAINING_MS: String(this.remaining()),
      PAGX_TASK_SETTLE_REMAINING_MS: String(this.settleRemaining()) };
  }

  async wait(ms: number, optional = false): Promise<void> {
    this.checkpoint();
    try { await delay(optional ? this.settleRemaining(ms) : ms, undefined, { signal: this.signal }); }
    catch (error) { this.checkpoint(); throw error; }
    this.checkpoint();
  }

  async run<T>(work: () => Promise<T>): Promise<T> {
    this.checkpoint();
    try {
      const pending = work();
      // Observe late finalizers after cancellation, without treating the business error as a cleanup failure.
      this.trackCleanup(pending.catch(() => {}));
      const result = await Promise.race([pending, this.cancelled]);
      this.checkpoint();
      return result;
    } catch (error) {
      this.signal.throwIfAborted();
      throw error;
    }
  }

  trackCleanup(pending: Promise<unknown>): void {
    this.cleanups.add(pending);
    void pending.catch(() => { this.cleanupFailed = true; }).finally(() => this.cleanups.delete(pending));
  }

  deferCleanup(cleanup: () => Promise<unknown>): void {
    this.trackCleanup(Promise.allSettled([...this.cleanups]).then(cleanup));
  }

  async own<T>(create: () => Promise<T>, dispose: (value: T) => Promise<unknown>): Promise<OwnedResource<T>> {
    this.checkpoint();
    let release!: () => void;
    this.trackCleanup(new Promise<void>(resolve => { release = resolve; }));
    let value: T;
    try { value = await create(); }
    catch (error) { release(); throw error; }
    let closing: Promise<void> | undefined;
    const close = () => closing ??= Promise.resolve().then(() => dispose(value)).then(() => {}, () => {
      // Teardown must not replace a navigation/conversion error. finish() reports
      // cleanup failure separately, after all other resources have been closed.
      this.cleanupFailed = true;
    }).finally(() => { this.signal.removeEventListener('abort', abort); release(); });
    const abort = () => { void close().catch(() => {}); };
    this.signal.addEventListener('abort', abort, { once: true });
    if (this.signal.aborted) abort();
    this.checkpoint();
    return { value, close };
  }

  abort(reason: unknown): void {
    if (!this.signal.aborted) this.controller.abort(reason);
  }

  /** An enclosing owner can retain shared files beyond the bounded shutdown grace. */
  async waitForCleanup(): Promise<boolean> {
    while (this.cleanups.size) await Promise.allSettled([...this.cleanups]);
    return !this.cleanupFailed;
  }

  async finish(graceMs = 5000): Promise<boolean> {
    clearTimeout(this.timer);
    this.releaseCli();
    this.abort(new Error('HTML conversion finished'));
    let timer: ReturnType<typeof setTimeout> | undefined;
    try {
      return await Promise.race([
        this.waitForCleanup(),
        new Promise<false>(resolve => { timer = setTimeout(() => resolve(false), graceMs); }),
      ]);
    } finally { if (timer) clearTimeout(timer); }
  }
}

export async function withConversionTask<T>(task: ConversionTask | undefined,
    work: (task: ConversionTask) => Promise<T>): Promise<T> {
  if (task) return task.run(() => work(task));
  const owner = new ConversionTask();
  try { return await owner.run(() => work(owner)); }
  catch (error) { owner.abort(error); throw error; }
  finally {
    if (!(await owner.finish())) process.stderr.write('html-snapshot: conversion cleanup did not complete\n');
  }
}
