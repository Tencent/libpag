// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { StateResources } = require('../dist/lib/state-resources');
const resource = ({ url = 'https://example.test/resource', type = 'image', ok = true,
  headers = {}, bytes = Buffer.from('bytes') } = {}) => ({
  request: () => ({ resourceType: () => type, url: () => url }), ok: () => ok, headers: () => headers,
  buffer: jest.fn(() => Promise.resolve(bytes)), body: jest.fn(() => Promise.resolve(bytes)),
});

test.each(['puppeteer', 'playwright'])('captures supported %s resources with explicit and default MIME types', async engine => {
  const cache = new StateResources(engine);
  for (const [type, mime] of [['image', 'application/octet-stream'], ['font', 'font/woff2'], ['stylesheet', 'text/css']]) {
    const entry = resource({ url: `https://example.test/${type}`, type });
    cache.onResponse(entry);
    const snapshot = await cache.snapshot(100);
    expect(snapshot.get(`https://example.test/${type}`)).toEqual({ type, contentType: mime, buffer: Buffer.from('bytes') });
    expect(engine === 'puppeteer' ? entry.buffer : entry.body).toHaveBeenCalledTimes(1);
    expect(engine === 'puppeteer' ? entry.body : entry.buffer).not.toHaveBeenCalled();
  }
  const entry = resource({ headers: { 'content-type': 'image/webp' } });
  cache.onResponse(entry);
  expect((await cache.snapshot(100)).get('https://example.test/resource').contentType).toBe('image/webp');
});

test('ignores unsuccessful, data URL, and unrelated responses before reading their bodies', async () => {
  const cache = new StateResources('puppeteer');
  const responses = [resource({ ok: false }), resource({ url: 'data:image/png;base64,AQ==' }),
    resource({ type: 'script' }), resource({ type: 'document' }), resource({ type: 'fetch' })];
  responses.forEach(cache.onResponse);
  expect((await cache.snapshot(100)).size).toBe(0);
  responses.forEach(response => expect(response.buffer).not.toHaveBeenCalled());
});

test('failed replacements invalidate old bytes while prior checkpoint maps remain stable', async () => {
  const log = jest.fn();
  const cache = new StateResources('puppeteer', log);
  cache.onResponse(resource());
  const first = await cache.snapshot(100);
  const failed = resource();
  failed.buffer.mockRejectedValue(new Error('body unavailable'));
  cache.onResponse(failed);
  expect((await cache.snapshot(100)).size).toBe(0);
  expect(first.get('https://example.test/resource').buffer.toString()).toBe('bytes');
  expect(log).toHaveBeenCalledWith(expect.stringContaining('body unavailable'));
  cache.onResponse(resource({ bytes: Buffer.alloc(0) }));
  expect((await cache.snapshot(100)).size).toBe(0);
});

test('snapshot deadlines do not discard a body that finishes later', async () => {
  jest.useFakeTimers();
  try {
    let finish;
    const pending = resource({ bytes: new Promise(resolve => { finish = resolve; }) });
    const cache = new StateResources('puppeteer');
    cache.onResponse(pending);
    const timeout = expect(cache.snapshot(20)).rejects.toThrow('Checkpoint resources timed out');
    await jest.advanceTimersByTimeAsync(20);
    await timeout;
    finish(Buffer.from('late'));
    expect((await cache.snapshot(100)).get('https://example.test/resource').buffer.toString()).toBe('late');
    expect(jest.getTimerCount()).toBe(0);
  } finally { jest.useRealTimers(); }
});

test('a stalled body is bounded and omitted after its timeout', async () => {
  jest.useFakeTimers();
  try {
    const log = jest.fn();
    const cache = new StateResources('puppeteer', log);
    cache.onResponse(resource({ bytes: new Promise(() => {}) }));
    const snapshot = cache.snapshot(20000);
    await jest.advanceTimersByTimeAsync(10000);
    expect((await snapshot).size).toBe(0);
    expect(log).toHaveBeenCalledWith(expect.stringContaining('response body timed out'));
    expect(jest.getTimerCount()).toBe(0);
  } finally { jest.useRealTimers(); }
});
