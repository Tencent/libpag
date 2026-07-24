'use strict';

const {
  SUPPORTED_ENGINES,
  DEFAULT_ENGINE,
  resolveEngine,
  unwrap,
  isWrapper,
  newPage,
  setViewport,
  mapWaitUntil,
  addCookies,
  responseBytes,
  addInitScript,
  formatLaunchHint,
  launchBrowser,
} = require('../dist/lib/browser-engine');

describe('resolveEngine', () => {
  const savedEnv = process.env.HTML_SNAPSHOT_BROWSER;
  afterEach(() => {
    if (savedEnv === undefined) delete process.env.HTML_SNAPSHOT_BROWSER;
    else process.env.HTML_SNAPSHOT_BROWSER = savedEnv;
  });

  test('defaults to puppeteer when nothing is set', () => {
    delete process.env.HTML_SNAPSHOT_BROWSER;
    expect(resolveEngine(undefined)).toBe(DEFAULT_ENGINE);
    expect(resolveEngine(undefined)).toBe('puppeteer');
  });

  test('normalises case', () => {
    expect(resolveEngine('PlayWright')).toBe('playwright');
  });

  test('honours the env var when no explicit name is passed', () => {
    process.env.HTML_SNAPSHOT_BROWSER = 'playwright';
    expect(resolveEngine(undefined)).toBe('playwright');
  });

  test('explicit argument wins over the env var', () => {
    process.env.HTML_SNAPSHOT_BROWSER = 'playwright';
    expect(resolveEngine('puppeteer')).toBe('puppeteer');
  });

  test('throws on an unsupported engine', () => {
    expect(() => resolveEngine('webkit')).toThrow(/unsupported browser engine/);
  });

  test('SUPPORTED_ENGINES lists both drivers', () => {
    expect(SUPPORTED_ENGINES).toEqual(['puppeteer', 'playwright']);
  });
});

describe('isWrapper / unwrap', () => {
  test('isWrapper recognises the { browser, engine } shape', () => {
    expect(isWrapper({ browser: {}, engine: 'puppeteer' })).toBe(true);
    expect(isWrapper({ browser: {} })).toBe(false);
    expect(isWrapper(null)).toBe(false);
    expect(isWrapper({})).toBe(false);
  });

  test('unwrap returns the wrapper components untouched', () => {
    const browser = { id: 1 };
    expect(unwrap({ browser, engine: 'playwright' }))
      .toEqual({ browser, engine: 'playwright' });
  });

  test('unwrap defaults a bare browser handle to puppeteer', () => {
    const browser = { id: 2 };
    expect(unwrap(browser)).toEqual({ browser, engine: 'puppeteer' });
  });
});

describe('mapWaitUntil', () => {
  test('maps networkidle to the engine flavour', () => {
    expect(mapWaitUntil('puppeteer', 'networkidle')).toBe('networkidle0');
    expect(mapWaitUntil('playwright', 'networkidle')).toBe('networkidle');
  });

  test('maps the legacy networkidle0 token per engine', () => {
    expect(mapWaitUntil('puppeteer', 'networkidle0')).toBe('networkidle0');
    expect(mapWaitUntil('playwright', 'networkidle0')).toBe('networkidle');
  });

  test('defaults a falsy token to networkidle', () => {
    expect(mapWaitUntil('puppeteer', '')).toBe('networkidle0');
    expect(mapWaitUntil('playwright', undefined)).toBe('networkidle');
  });

  test('passes other tokens through unchanged', () => {
    expect(mapWaitUntil('puppeteer', 'load')).toBe('load');
    expect(mapWaitUntil('playwright', 'domcontentloaded')).toBe('domcontentloaded');
  });
});

describe('newPage', () => {
  test('puppeteer: calls setViewport with the device scale factor', async () => {
    const calls = [];
    const page = { setViewport: (vp) => { calls.push(vp); } };
    const browser = { newPage: async () => page };
    const out = await newPage({ browser, engine: 'puppeteer' }, {
      viewport: { width: 800, height: 600 },
    });
    expect(out).toBe(page);
    expect(calls[0]).toEqual({ width: 800, height: 600, deviceScaleFactor: 1 });
  });

  test('playwright: calls setViewportSize without a DSF', async () => {
    const calls = [];
    const page = { setViewportSize: (vp) => { calls.push(vp); } };
    const browser = { newPage: async () => page };
    await newPage({ browser, engine: 'playwright' }, {
      viewport: { width: 1024, height: 768 },
    });
    expect(calls[0]).toEqual({ width: 1024, height: 768 });
  });

  test('playwright: rejects a non-default device scale factor', async () => {
    const page = { setViewportSize: () => {} };
    const browser = { newPage: async () => page };
    await expect(newPage({ browser, engine: 'playwright' }, {
      viewport: { width: 800, height: 600, deviceScaleFactor: 2 },
    })).rejects.toThrow(/deviceScaleFactor/);
  });

  test('no viewport opt skips the setter', async () => {
    let called = false;
    const page = { setViewport: () => { called = true; } };
    const browser = { newPage: async () => page };
    await newPage({ browser, engine: 'puppeteer' }, {});
    expect(called).toBe(false);
  });
});

describe('setViewport', () => {
  test('puppeteer uses setViewport with DSF', async () => {
    const calls = [];
    const page = { setViewport: (vp) => calls.push(vp) };
    await setViewport(page, 'puppeteer', { width: 100, height: 200 });
    expect(calls[0]).toEqual({ width: 100, height: 200, deviceScaleFactor: 1 });
  });

  test('playwright uses setViewportSize', async () => {
    const calls = [];
    const page = { setViewportSize: (vp) => calls.push(vp) };
    await setViewport(page, 'playwright', { width: 100, height: 200 });
    expect(calls[0]).toEqual({ width: 100, height: 200 });
  });

  test('no-op when viewport is falsy', async () => {
    await expect(setViewport({}, 'puppeteer', null)).resolves.toBeUndefined();
  });
});

describe('addCookies', () => {
  test('puppeteer spreads cookies into setCookie', async () => {
    const received = [];
    const page = { setCookie: (...cookies) => received.push(...cookies) };
    await addCookies(page, 'puppeteer', [{ name: 'a', value: '1', url: 'https://x' }]);
    expect(received).toEqual([{ name: 'a', value: '1', url: 'https://x' }]);
  });

  test('playwright passes cookies to context().addCookies', async () => {
    let received = null;
    const ctx = { addCookies: (cookies) => { received = cookies; } };
    const page = { context: () => ctx };
    await addCookies(page, 'playwright', [{ name: 'a', value: '1', url: 'https://x' }]);
    expect(received).toEqual([{ name: 'a', value: '1', url: 'https://x' }]);
  });

  test('no-op for an empty cookie list', async () => {
    let touched = false;
    const page = { setCookie: () => { touched = true; } };
    await addCookies(page, 'puppeteer', []);
    expect(touched).toBe(false);
  });
});

describe('responseBytes', () => {
  test('puppeteer reads response.buffer()', async () => {
    const resp = { buffer: async () => Buffer.from('pup') };
    expect((await responseBytes(resp, 'puppeteer')).toString()).toBe('pup');
  });

  test('playwright reads response.body()', async () => {
    const resp = { body: async () => Buffer.from('play') };
    expect((await responseBytes(resp, 'playwright')).toString()).toBe('play');
  });
});

describe('addInitScript', () => {
  test('puppeteer uses evaluateOnNewDocument', async () => {
    const calls = [];
    const page = { evaluateOnNewDocument: (s) => calls.push(s) };
    await addInitScript(page, 'puppeteer', 'window.x=1');
    expect(calls).toEqual(['window.x=1']);
  });

  test('playwright wraps the source in a content object', async () => {
    const calls = [];
    const page = { addInitScript: (arg) => calls.push(arg) };
    await addInitScript(page, 'playwright', 'window.x=1');
    expect(calls).toEqual([{ content: 'window.x=1' }]);
  });
});

describe('formatLaunchHint', () => {
  test('returns null for unrelated errors', () => {
    expect(formatLaunchHint(new Error('something else'), 'puppeteer')).toBeNull();
    expect(formatLaunchHint('plain string error', 'puppeteer')).toBeNull();
  });

  test('returns puppeteer-flavoured install lines for the missing-binary message', () => {
    const out = formatLaunchHint(new Error('Could not find Chrome (ver. xyz)'), 'puppeteer');
    expect(Array.isArray(out)).toBe(true);
    expect(out[0]).toMatch(/failed to launch headless Chromium for engine 'puppeteer'/);
    expect(out.some((l) => /puppeteer browsers install chrome/.test(l))).toBe(true);
  });

  test('returns playwright-flavoured install lines', () => {
    const out = formatLaunchHint(new Error("Executable doesn't exist at /opt/foo"), 'playwright');
    expect(out[0]).toMatch(/engine 'playwright'/);
    expect(out.some((l) => /playwright install chromium/.test(l))).toBe(true);
  });

  test('matches "Failed to launch the browser process" verbatim', () => {
    const out = formatLaunchHint(new Error('Failed to launch the browser process'), 'puppeteer');
    expect(out).not.toBeNull();
  });

  test('handles non-Error throws by stringifying them', () => {
    expect(formatLaunchHint('Could not find Chrome', 'puppeteer')).not.toBeNull();
    expect(formatLaunchHint('benign string', 'puppeteer')).toBeNull();
  });
});

describe('launchBrowser — package loading & options', () => {
  // Each test reloads `dist/lib/browser-engine` after registering a mock for
  // the engine package, so the lazy `require()` inside loadEngine picks up
  // the stub instead of the real puppeteer / playwright install.
  function loadWithMock(pkg, mockFactory) {
    jest.resetModules();
    jest.doMock(pkg, mockFactory, { virtual: true });
    return require('../dist/lib/browser-engine');
  }

  afterEach(() => {
    jest.resetModules();
  });

  test('rejects with the install hint when the engine package fails to load', async () => {
    const mod = loadWithMock('puppeteer', () => { throw new Error('Cannot find module foo'); });
    await expect(mod.launchBrowser({ engine: 'puppeteer' }))
      .rejects.toThrow(/failed to load 'puppeteer' for browser engine 'puppeteer'/);
  });

  test('passes default headless + args to puppeteer.launch', async () => {
    const launches = [];
    const mod = loadWithMock('puppeteer', () => ({
      launch: async (opts) => { launches.push(opts); return { closed: false }; },
    }));
    const handle = await mod.launchBrowser({ engine: 'puppeteer' });
    expect(handle.engine).toBe('puppeteer');
    expect(launches).toHaveLength(1);
    expect(launches[0].headless).toBe(true);
    expect(launches[0].args).toEqual(['--no-sandbox', '--font-render-hinting=none', '--allow-file-access-from-files']);
    // executablePath plumbing is engine-specific; puppeteer reads its env var
    // at launch time, so this code path must NOT inject one.
    expect('executablePath' in launches[0]).toBe(false);
  });

  test('honours headless: false and a custom args array', async () => {
    const launches = [];
    const mod = loadWithMock('puppeteer', () => ({
      launch: async (opts) => { launches.push(opts); return {}; },
    }));
    await mod.launchBrowser({ engine: 'puppeteer', headless: false, args: ['--remote-debugging-port=9222'] });
    expect(launches[0].headless).toBe(false);
    expect(launches[0].args).toEqual(['--remote-debugging-port=9222']);
  });

  test('playwright: forwards executablePath when supplied', async () => {
    const launches = [];
    const mod = loadWithMock('playwright', () => ({
      chromium: { launch: async (opts) => { launches.push(opts); return {}; } },
    }));
    await mod.launchBrowser({ engine: 'playwright', executablePath: '/opt/chromium' });
    expect(launches[0].executablePath).toBe('/opt/chromium');
  });

  test('playwright: falls back to PLAYWRIGHT_EXECUTABLE_PATH', async () => {
    const launches = [];
    const mod = loadWithMock('playwright', () => ({
      chromium: { launch: async (opts) => { launches.push(opts); return {}; } },
    }));
    const saved = process.env.PLAYWRIGHT_EXECUTABLE_PATH;
    process.env.PLAYWRIGHT_EXECUTABLE_PATH = '/from-env/chromium';
    try {
      await mod.launchBrowser({ engine: 'playwright' });
      expect(launches[0].executablePath).toBe('/from-env/chromium');
    } finally {
      if (saved === undefined) delete process.env.PLAYWRIGHT_EXECUTABLE_PATH;
      else process.env.PLAYWRIGHT_EXECUTABLE_PATH = saved;
    }
  });

  test('playwright: rejects when the package omits chromium', async () => {
    const mod = loadWithMock('playwright', () => ({}));
    await expect(mod.launchBrowser({ engine: 'playwright' }))
      .rejects.toThrow(/'playwright' module does not expose chromium/);
  });
});
