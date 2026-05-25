// Browser-engine adapter — pick between Puppeteer (default) and Playwright at
// launch time, then expose a single set of helpers that paper over the small
// shape differences in their per-page APIs (viewport, cookies, networkidle).
//
// Why an adapter at all: snapshot.js, eval/baseline.js, eval/compare.js, and
// eval/run.js all drive a headless Chromium identically — open a page, set a
// viewport, optionally inject cookies/headers, navigate, wait for networkidle,
// then evaluate scripts and screenshot. Puppeteer and Playwright agree on the
// high-level shape but disagree on the exact method names and option keys, so
// without this layer every caller would carry its own `if (engine === ...)`
// fork. The adapter keeps engine knowledge in one file.
//
// Selecting the engine (precedence: highest first):
//   1. `--browser-engine <puppeteer|playwright>` CLI flag (parsed in cli.js)
//   2. `HTML_SNAPSHOT_BROWSER=puppeteer|playwright` environment variable
//   3. `'puppeteer'` (default, keeps backward compatibility)
//
// Playwright is loaded lazily via `require()` — installing it remains opt-in
// (declared in `optionalDependencies`), so the default install footprint and
// the existing puppeteer-only workflow are unchanged.

'use strict';

const SUPPORTED_ENGINES = ['puppeteer', 'playwright'];
const DEFAULT_ENGINE = 'puppeteer';

// Map of canonical engine name → npm package name. The package is required
// lazily so users who never set --browser-engine playwright don't have to
// install playwright.
const ENGINE_PACKAGE = {
  puppeteer: 'puppeteer',
  playwright: 'playwright',
};

// Normalise `name` to one of SUPPORTED_ENGINES, falling back to the env var
// then to DEFAULT_ENGINE. Throws on an unknown explicit value so the user
// notices typos at startup rather than after the first browser call fails.
function resolveEngine(name) {
  const raw = (name || process.env.HTML_SNAPSHOT_BROWSER || DEFAULT_ENGINE).toLowerCase();
  if (!SUPPORTED_ENGINES.includes(raw)) {
    throw new Error(
      `unsupported browser engine '${name}' (expected one of: ${SUPPORTED_ENGINES.join(', ')})`,
    );
  }
  return raw;
}

// Lazy-require the engine package and return its launcher. We unify the two
// shapes here (puppeteer is its own launcher; playwright exposes
// `chromium.launch`) so the rest of the code only calls `launcher.launch(...)`.
function loadEngine(engine) {
  const pkg = ENGINE_PACKAGE[engine];
  let mod;
  try {
    mod = require(pkg);
  } catch (err) {
    const hint = engine === 'playwright'
      ? `Run \`npm install --prefix tools/html-snapshot playwright\` and then\n`
        + `\`npx --prefix tools/html-snapshot playwright install chromium\` to fetch the browser binary.`
      : `Run \`npm install --prefix tools/html-snapshot\` to install puppeteer.`;
    const e = new Error(
      `failed to load '${pkg}' for browser engine '${engine}': ${err && err.message ? err.message : err}\n${hint}`,
    );
    e.cause = err;
    throw e;
  }
  if (engine === 'playwright') {
    if (!mod.chromium) {
      throw new Error(`'playwright' module does not expose chromium — got ${typeof mod}`);
    }
    return mod.chromium;
  }
  return mod;
}

// Launch a headless browser using the requested engine. Returns the wrapper
// `{ browser, engine }`; downstream helpers in this file accept that wrapper
// directly so callers don't have to remember to thread the engine name
// alongside the browser handle.
async function launchBrowser(opts) {
  const engine = resolveEngine(opts && opts.engine);
  const launcher = loadEngine(engine);
  const launchOpts = {
    headless: opts && opts.headless !== undefined ? opts.headless : true,
    // `--no-sandbox` is needed inside most CI/Docker environments where
    // Chromium's sandbox can't be set up. `--font-render-hinting=none` keeps
    // glyph metrics stable across machines so the baseline and subset PNGs
    // diff cleanly. Both engines accept these flags identically.
    args: (opts && opts.args) || ['--no-sandbox', '--font-render-hinting=none'],
  };
  const browser = await launcher.launch(launchOpts);
  return { browser, engine };
}

// True if the caller passed our `{ browser, engine }` wrapper rather than a
// bare puppeteer/playwright Browser instance. Used by helpers that want to
// stay backward compatible with code paths still passing a raw browser.
function isWrapper(x) {
  return !!(x && typeof x === 'object' && x.browser && typeof x.engine === 'string');
}

// Unwrap a `{ browser, engine }` wrapper into its two components, defaulting
// the engine to puppeteer when a bare browser handle is passed. Centralised
// because every helper below needs the same coercion.
function unwrap(wrapperOrBrowser) {
  if (isWrapper(wrapperOrBrowser)) {
    return { browser: wrapperOrBrowser.browser, engine: wrapperOrBrowser.engine };
  }
  return { browser: wrapperOrBrowser, engine: DEFAULT_ENGINE };
}

// Map our generic options to the engine-specific page constructor calls.
//
// Both engines expose `browser.newPage()`. The semantic difference is that
// Playwright implicitly wraps each `newPage` in a fresh BrowserContext and
// auto-closes that context when the page closes — so callers don't have to
// track context lifetime as long as they go through `newPage()` (rather
// than `browser.newContext().newPage()`, which leaves the context open).
//
// Viewport handling:
//   - Puppeteer: `page.setViewport({ width, height, deviceScaleFactor })`
//     accepts DSF per page, post-creation.
//   - Playwright: DSF is fixed at context-construction time. Since every
//     html-snapshot path uses `deviceScaleFactor: 1` (the default), we can
//     skip the context dance and just call `setViewportSize()` after
//     `newPage()`. If a non-default DSF is ever requested for Playwright,
//     we error out clearly rather than silently dropping it.
async function newPage(wrapperOrBrowser, opts) {
  const { browser, engine } = unwrap(wrapperOrBrowser);
  const viewport = (opts && opts.viewport) || null;
  if (engine === 'playwright') {
    if (viewport && typeof viewport.deviceScaleFactor === 'number'
        && viewport.deviceScaleFactor !== 1) {
      throw new Error(
        `playwright engine: deviceScaleFactor=${viewport.deviceScaleFactor} `
        + `requires a custom BrowserContext; only DSF=1 is supported via newPage()`,
      );
    }
    const page = await browser.newPage();
    if (viewport) {
      await page.setViewportSize({ width: viewport.width, height: viewport.height });
    }
    return page;
  }
  const page = await browser.newPage();
  if (viewport) {
    await page.setViewport({
      width: viewport.width,
      height: viewport.height,
      deviceScaleFactor: typeof viewport.deviceScaleFactor === 'number'
        ? viewport.deviceScaleFactor : 1,
    });
  }
  return page;
}

// Resize an already-opened page. Puppeteer keeps `setViewport`; Playwright
// renamed it to `setViewportSize` and split the DSF out to the context (which
// can't be changed once set). Callers that need a different DSF must request
// it via newPage's viewport opt at open time.
async function setViewport(page, engine, viewport) {
  if (!viewport) return;
  if (engine === 'playwright') {
    await page.setViewportSize({ width: viewport.width, height: viewport.height });
    return;
  }
  await page.setViewport({
    width: viewport.width,
    height: viewport.height,
    deviceScaleFactor: typeof viewport.deviceScaleFactor === 'number'
      ? viewport.deviceScaleFactor : 1,
  });
}

// Translate our generic "fully settled" waitUntil token into the engine's
// flavour. Puppeteer's `'networkidle0'` (no in-flight requests for 500ms) and
// Playwright's `'networkidle'` are semantically the same; both engines
// accept `'load'` / `'domcontentloaded'` verbatim.
function mapWaitUntil(engine, token) {
  if (!token || token === 'networkidle') {
    return engine === 'playwright' ? 'networkidle' : 'networkidle0';
  }
  if (token === 'networkidle0') {
    return engine === 'playwright' ? 'networkidle' : 'networkidle0';
  }
  return token;
}

// Engine-agnostic cookie injection. Both APIs accept the same shape
// (`{ name, value, url|domain }`); the difference is just where the call
// lives — on the page for puppeteer, on the context for playwright.
// Cookies must include either `url` or `domain` so the browser knows which
// origin they belong to.
async function addCookies(page, engine, cookies) {
  if (!cookies || cookies.length === 0) return;
  if (engine === 'playwright') {
    await page.context().addCookies(cookies);
    return;
  }
  await page.setCookie(...cookies);
}

module.exports = {
  SUPPORTED_ENGINES,
  DEFAULT_ENGINE,
  resolveEngine,
  launchBrowser,
  unwrap,
  isWrapper,
  newPage,
  setViewport,
  mapWaitUntil,
  addCookies,
};
