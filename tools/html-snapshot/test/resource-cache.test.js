'use strict';

const { ResourceCache } = require('../dist/lib/resource-cache');
const {
  makeResourceCacheRouteHandler,
  makeResourceCacheListener,
} = require('../dist/lib/snapshot-runner');

function res(bytes, contentType = 'text/css') {
  return { status: 200, contentType, body: Buffer.alloc(bytes, 1) };
}

describe('ResourceCache', () => {
  describe('get/set', () => {
    test('miss returns undefined, hit returns the stored resource', () => {
      const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
      expect(cache.get('http://x/a.css')).toBeUndefined();

      const entry = res(100);
      cache.set('http://x/a.css', entry);

      expect(cache.get('http://x/a.css')).toBe(entry);
      expect(cache.has('http://x/a.css')).toBe(true);
      expect(cache.size).toBe(1);
      expect(cache.bytes).toBe(100);
    });

    test('re-writing the same URL overwrites and re-totals the bytes', () => {
      const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
      cache.set('http://x/a.css', res(100));
      cache.set('http://x/a.css', res(250));

      expect(cache.size).toBe(1);
      expect(cache.bytes).toBe(250);
      expect(cache.get('http://x/a.css').body.length).toBe(250);
    });

    test('has() does not disturb LRU order', () => {
      const cache = new ResourceCache(2, 1024 * 1024, 1024 * 1024);
      cache.set('a', res(10));
      cache.set('b', res(10));
      // Peeking 'a' must NOT mark it recently used, so writing 'c' evicts 'a'.
      expect(cache.has('a')).toBe(true);
      cache.set('c', res(10));
      expect(cache.has('a')).toBe(false);
      expect(cache.has('b')).toBe(true);
      expect(cache.has('c')).toBe(true);
    });
  });

  describe('eviction', () => {
    test('drops the least-recently-used item past the entry cap', () => {
      const cache = new ResourceCache(2, 1024 * 1024, 1024 * 1024);
      cache.set('a', res(10));
      cache.set('b', res(10));
      cache.set('c', res(10));

      expect(cache.size).toBe(2);
      expect(cache.get('a')).toBeUndefined();
      expect(cache.get('b')).toBeDefined();
      expect(cache.get('c')).toBeDefined();
    });

    test('get refreshes LRU order so the touched item survives', () => {
      const cache = new ResourceCache(2, 1024 * 1024, 1024 * 1024);
      cache.set('a', res(10));
      cache.set('b', res(10));
      expect(cache.get('a')).toBeDefined();
      cache.set('c', res(10));

      expect(cache.get('a')).toBeDefined();
      expect(cache.get('b')).toBeUndefined();
      expect(cache.get('c')).toBeDefined();
    });

    test('keeps evicting until under the total byte budget', () => {
      const cache = new ResourceCache(100, 250, 1024 * 1024);
      cache.set('a', res(100));
      cache.set('b', res(100));
      cache.set('c', res(100));

      expect(cache.bytes).toBeLessThanOrEqual(250);
      expect(cache.get('a')).toBeUndefined();
      expect(cache.get('b')).toBeDefined();
      expect(cache.get('c')).toBeDefined();
    });
  });

  describe('per-entry cap', () => {
    test('an entry larger than maxEntryBytes is not cached', () => {
      const cache = new ResourceCache(10, 1024 * 1024, 50);
      cache.set('big', res(100));
      expect(cache.size).toBe(0);
      expect(cache.get('big')).toBeUndefined();
    });
  });

  describe('clear', () => {
    test('empties all entries and resets the byte counter', () => {
      const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
      cache.set('a', res(100));
      cache.set('b', res(100));
      cache.clear();

      expect(cache.size).toBe(0);
      expect(cache.bytes).toBe(0);
      expect(cache.get('a')).toBeUndefined();
    });
  });
});

describe('makeResourceCacheRouteHandler', () => {
  test('fulfils a GET hit for an http(s) URL', () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    cache.set('https://cdn/app.css', res(20, 'text/css'));
    const handle = makeResourceCacheRouteHandler(cache);

    const decision = handle({ url: 'https://cdn/app.css', method: 'GET', resourceType: 'stylesheet' });
    expect(decision.action).toBe('fulfill');
    expect(decision.fulfillment.status).toBe(200);
    expect(decision.fulfillment.contentType).toBe('text/css');
    expect(decision.fulfillment.body.length).toBe(20);
  });

  test('continues on a cache miss', () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    const handle = makeResourceCacheRouteHandler(cache);
    expect(handle({ url: 'https://cdn/missing.css', method: 'GET', resourceType: 'stylesheet' }))
      .toEqual({ action: 'continue' });
  });

  test('continues for non-GET requests even on a hit', () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    cache.set('https://cdn/app.css', res(20));
    const handle = makeResourceCacheRouteHandler(cache);
    expect(handle({ url: 'https://cdn/app.css', method: 'POST', resourceType: 'stylesheet' }))
      .toEqual({ action: 'continue' });
  });

  test('continues for non-http(s) URLs (e.g. the file:// document)', () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    const handle = makeResourceCacheRouteHandler(cache);
    expect(handle({ url: 'file:///tmp/input.html', method: 'GET', resourceType: 'document' }))
      .toEqual({ action: 'continue' });
  });
});

describe('makeResourceCacheListener', () => {
  // Minimal fake response shaped like the bits the listener reads, for both
  // engines (puppeteer buffer() / playwright body() are unified by
  // responseBytes; we pass engine 'puppeteer' so buffer() is used).
  function fakeResp({ url, method = 'GET', ok = true, contentType = 'text/css', body = Buffer.from('x') }) {
    return {
      request: () => ({ method: () => method }),
      ok: () => ok,
      url: () => url,
      headers: () => ({ 'content-type': contentType }),
      buffer: async () => body,
    };
  }

  test('caches a successful CSS GET response', async () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    const listen = makeResourceCacheListener('puppeteer', cache, null);
    await listen(fakeResp({ url: 'https://cdn/app.css', body: Buffer.from('body{}') }));

    const got = cache.get('https://cdn/app.css');
    expect(got).toBeDefined();
    expect(got.contentType).toBe('text/css');
    expect(got.body.toString()).toBe('body{}');
  });

  test('caches a font by URL extension even with a generic content-type', async () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    const listen = makeResourceCacheListener('puppeteer', cache, null);
    await listen(fakeResp({
      url: 'https://cdn/icons.woff2',
      contentType: 'application/octet-stream',
      body: Buffer.from('font'),
    }));
    expect(cache.has('https://cdn/icons.woff2')).toBe(true);
  });

  test('skips images and other non-cacheable types', async () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    const listen = makeResourceCacheListener('puppeteer', cache, null);
    await listen(fakeResp({ url: 'https://cdn/pic.png', contentType: 'image/png' }));
    expect(cache.size).toBe(0);
  });

  test('skips non-GET, non-ok, and non-http(s) responses', async () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    const listen = makeResourceCacheListener('puppeteer', cache, null);
    await listen(fakeResp({ url: 'https://cdn/a.css', method: 'POST' }));
    await listen(fakeResp({ url: 'https://cdn/b.css', ok: false }));
    await listen(fakeResp({ url: 'file:///tmp/c.css' }));
    expect(cache.size).toBe(0);
  });

  test('does not re-read the body for an already-cached URL', async () => {
    const cache = new ResourceCache(10, 1024 * 1024, 1024 * 1024);
    cache.set('https://cdn/app.css', res(5, 'text/css'));
    const listen = makeResourceCacheListener('puppeteer', cache, null);
    let read = false;
    await listen({
      request: () => ({ method: () => 'GET' }),
      ok: () => true,
      url: () => 'https://cdn/app.css',
      headers: () => ({ 'content-type': 'text/css' }),
      buffer: async () => { read = true; return Buffer.from('new'); },
    });
    expect(read).toBe(false);
    expect(cache.get('https://cdn/app.css').body.length).toBe(5);
  });
});
