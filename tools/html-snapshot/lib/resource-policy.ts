// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import type { ConversionTask } from './conversion-task';

/** A response that arrives after a partial capture is sealed must not change its resource set. */
export class CaptureStore<T> extends Map<string, T> {
  sealed = false;
  readonly pending = new Set<Promise<void>>();
  override set(url: string, value: T): this {
    if (!this.sealed) super.set(url, value);
    return this;
  }
  setLocal(url: string, value: T): this { return super.set(url, value); }
  observe(work: Promise<void>): void {
    this.pending.add(work);
    void work.catch(() => {}).finally(() => this.pending.delete(work));
  }
  async drain(task: ConversionTask, stage: string): Promise<void> {
    if (this.pending.size) await task.prepare(stage, Infinity, async () => {
      await Promise.allSettled([...this.pending]);
      return true;
    });
    if (task.readiness.status === 'partial') this.sealed = true;
  }
}

// Installed before the virtual animation clock. Only converter helpers call this API;
// the source application's fetch/timers remain untouched.
export function installResourcePolicy(): void {
  const nativeFetch = window.fetch.bind(window);
  const now = performance.now.bind(performance);
  const later = window.setTimeout.bind(window), cancel = window.clearTimeout.bind(window);
  const requests = new Map<string, Promise<{ bytes: ArrayBuffer; status: number; headers: [string, string][] }>>();
  const controllers = new Set<AbortController>();
  let deadline = Infinity, sealed = false, timedOut = false;
  const seal = () => { sealed = true; for (const controller of controllers) controller.abort(); };
  const remaining = () => Math.max(0, deadline - now());
  window.__pagxResources = {
    configure(ms: number) { deadline = Math.min(deadline, now() + ms); },
    seal,
    get timedOut() { return timedOut; },
    async fetch(url: string, init?: RequestInit): Promise<Response> {
      if (!/^https?:/i.test(url)) return nativeFetch(url, init);
      // URL plus request semantics: CSS uses CORS, images use the browser default.
      const key = JSON.stringify([url, init?.mode, init?.credentials]);
      let pending = requests.get(key);
      if (!pending) {
        if (sealed) throw new Error('Resource capture is sealed');
        if (!remaining()) { timedOut = true; seal(); throw new Error('Resource preparation timed out'); }
        pending = (async () => {
          const controller = new AbortController();
          controllers.add(controller);
          const timer = Number.isFinite(deadline) ? later(() => {
            timedOut = true; seal();
          }, remaining()) : undefined;
          try {
            const response = await nativeFetch(url, { ...init, signal: controller.signal });
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            const bytes = await response.arrayBuffer();
            if (controller.signal.aborted) throw new Error('Resource capture was cancelled');
            return { bytes, status: response.status, headers: Array.from(response.headers.entries()) };
          } finally { if (timer !== undefined) cancel(timer); controllers.delete(controller); }
        })();
        // Keep failures too: repeated inline passes must not retry a stalled/failed resource.
        requests.set(key, pending);
      }
      const result = await pending;
      return new Response(result.bytes, { status: result.status, headers: result.headers });
    },
    async imageWidth(src: string): Promise<number> {
      if (/^https?:/i.test(src) && sealed) return 0;
      const probe = new Image();
      let timer: number | undefined;
      try {
        probe.src = src;
        if (!/^https?:/i.test(src)) return await probe.decode().then(() => probe.naturalWidth, () => 0);
        return await Promise.race([
          probe.decode().then(() => probe.naturalWidth, () => 0),
          new Promise<number>(resolve => {
            if (Number.isFinite(deadline)) timer = later(() => {
              timedOut = true; seal(); probe.src = ''; resolve(0);
            }, remaining());
          }),
        ]);
      } finally { if (timer !== undefined) cancel(timer); }
    },
  };
}

declare global {
  interface Window {
    __pagxResources?: {
      configure(ms: number): void;
      seal(): void;
      readonly timedOut: boolean;
      fetch(url: string, init?: RequestInit): Promise<Response>;
      imageWidth(src: string): Promise<number>;
    };
  }
}

export const RESOURCE_POLICY_INIT_SCRIPT = `(${installResourcePolicy.toString()})()`;
