// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.

import { responseBytes, type BrowserResponse, type EngineName } from './browser-engine';

export interface StateResource {
  buffer: Buffer;
  contentType: string;
  type: string;
}

/** Observe before navigation. Each checkpoint gets immutable response bytes, including
 * authenticated assets and assets fetched only after an interaction. A later response to
 * the same URL replaces the earlier version; completion order cannot resurrect old bytes.
 */
export class StateResources {
  private entries = new Map<string, StateResource>();
  private versions = new Map<string, number>();
  private pending = new Set<Promise<void>>();
  constructor(private engine: EngineName, private log?: (message: string) => void) {}

  onResponse = (response: BrowserResponse): void => {
    const request = response.request();
    const type = request.resourceType();
    if (!['image', 'font', 'stylesheet'].includes(type) || !response.ok()) return;
    const url = request.url();
    if (url.startsWith('data:')) return;
    const version = (this.versions.get(url) || 0) + 1;
    this.versions.set(url, version);
    // Invalidate first: a failed new response must not reuse the previous state.
    this.entries.delete(url);
    let timer: ReturnType<typeof setTimeout> | undefined;
    const read = Promise.race([responseBytes(response, this.engine), new Promise<never>((_, reject) => {
      timer = setTimeout(() => reject(new Error('response body timed out')), 10000);
    })]).then(buffer => {
      if (buffer.length && this.versions.get(url) === version) this.entries.set(url, {
        buffer, type, contentType: response.headers()['content-type'] ||
          (type === 'stylesheet' ? 'text/css' : type === 'font' ? 'font/woff2' : 'application/octet-stream'),
      });
    }).catch(err => { this.log?.(`Resource capture failed (${type}): ${(err as Error).message}`); })
      .finally(() => { clearTimeout(timer); this.pending.delete(read); });
    this.pending.add(read);
  };

  async snapshot(timeoutMs: number): Promise<Map<string, StateResource>> {
    let timer: ReturnType<typeof setTimeout> | undefined;
    try {
      await Promise.race([(async () => {
        while (this.pending.size) await Promise.all(Array.from(this.pending));
      })(), new Promise<never>((_, reject) => {
        timer = setTimeout(() => reject(new Error('Checkpoint resources timed out')), timeoutMs);
      })]);
      return new Map(this.entries);
    } finally { clearTimeout(timer); }
  }
}
