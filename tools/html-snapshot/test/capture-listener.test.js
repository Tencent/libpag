'use strict';

const { makeCaptureListener } = require('../dist/lib/capture-listener');

// Build a puppeteer-style response stub. `responseBytes(resp, 'puppeteer')`
// inside the listener resolves via `resp.buffer()`, so we expose that here
// and let the test override timing/error behaviour as needed.
function makeResp({
  url = 'https://x/asset.bin',
  type = 'font',
  ok = true,
  body = 'BYTES',
  bufferImpl,
} = {}) {
  return {
    request: () => ({ resourceType: () => type, url: () => url }),
    ok: () => ok,
    buffer: bufferImpl || (async () => Buffer.from(body)),
  };
}

describe('makeCaptureListener', () => {
  test('caches a successful matching response', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
    });
    await listener(makeResp({ url: 'https://x/a.woff2' }));
    expect(cache.get('https://x/a.woff2').toString()).toBe('BYTES');
  });

  test('passes the response through `project` to derive the cached value', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'image',
      engine: 'puppeteer',
      cache,
      project: (buf, resp) => ({
        buffer: buf,
        contentType: resp.contentType ? resp.contentType() : 'inferred',
      }),
    });
    const resp = makeResp({ url: 'https://x/a.png', type: 'image' });
    resp.contentType = () => 'image/png';
    await listener(resp);
    expect(cache.get('https://x/a.png')).toEqual({
      buffer: Buffer.from('BYTES'),
      contentType: 'image/png',
    });
  });

  test('skips responses with a non-matching resourceType', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
    });
    await listener(makeResp({ type: 'image' }));
    expect(cache.size).toBe(0);
  });

  test('skips data: URLs and non-OK responses', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
    });
    await listener(makeResp({ url: 'data:font/woff2;base64,AAAA' }));
    await listener(makeResp({ url: 'https://x/b.woff2', ok: false }));
    expect(cache.size).toBe(0);
  });

  test('does not re-fetch a URL already in the cache', async () => {
    const cache = new Map();
    cache.set('https://x/c', Buffer.from('OLD'));
    let calls = 0;
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
    });
    await listener({
      request: () => ({ resourceType: () => 'font', url: () => 'https://x/c' }),
      ok: () => true,
      buffer: async () => { calls++; return Buffer.from('NEW'); },
    });
    expect(calls).toBe(0);
    expect(cache.get('https://x/c').toString()).toBe('OLD');
  });

  test('logs and swallows errors thrown while reading the body', async () => {
    const logs = [];
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
      label: 'font',
      log: (m) => logs.push(m),
    });
    await listener(makeResp({
      bufferImpl: async () => { throw new Error('detached'); },
    }));
    expect(cache.size).toBe(0);
    expect(logs[0]).toMatch(/font capture skipped: detached/);
  });

  test('uses the supplied label in error logs', async () => {
    const logs = [];
    const listener = makeCaptureListener({
      resourceType: 'image',
      engine: 'puppeteer',
      cache: new Map(),
      label: 'image',
      log: (m) => logs.push(m),
    });
    await listener(makeResp({
      type: 'image',
      bufferImpl: async () => { throw new Error('boom'); },
    }));
    expect(logs[0]).toMatch(/^image capture skipped: boom$/);
  });

  test('skipEmptyBody=true drops zero-length payloads from the cache', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
      skipEmptyBody: true,
    });
    await listener({
      request: () => ({ resourceType: () => 'font', url: () => 'https://x/empty' }),
      ok: () => true,
      buffer: async () => Buffer.alloc(0),
    });
    expect(cache.size).toBe(0);
  });

  test('skipEmptyBody=false (default) caches an empty buffer', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'image',
      engine: 'puppeteer',
      cache,
    });
    await listener({
      request: () => ({ resourceType: () => 'image', url: () => 'https://x/empty' }),
      ok: () => true,
      buffer: async () => Buffer.alloc(0),
    });
    expect(cache.has('https://x/empty')).toBe(true);
    expect(cache.get('https://x/empty').length).toBe(0);
  });

  test('timeoutMs caches the body when it resolves before the timer', async () => {
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
      timeoutMs: 100,
    });
    await listener(makeResp({ url: 'https://x/fast', body: 'OK' }));
    expect(cache.get('https://x/fast').toString()).toBe('OK');
  });

  test('timeoutMs aborts a hung body read and surfaces the configured message', async () => {
    const logs = [];
    const cache = new Map();
    const listener = makeCaptureListener({
      resourceType: 'font',
      engine: 'puppeteer',
      cache,
      timeoutMs: 5,
      timeoutMessage: 'custom timeout label',
      log: (m) => logs.push(m),
    });
    // A body read that never resolves: the timer races it and rejects.
    await listener({
      request: () => ({ resourceType: () => 'font', url: () => 'https://x/hang' }),
      ok: () => true,
      buffer: () => new Promise(() => {}),
    });
    expect(cache.size).toBe(0);
    expect(logs[0]).toMatch(/custom timeout label/);
  });
});
