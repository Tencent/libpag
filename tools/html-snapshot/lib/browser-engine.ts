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
//   1. `--browser-engine <puppeteer|playwright>` CLI flag (parsed in cli.ts)
//   2. `HTML_SNAPSHOT_BROWSER=puppeteer|playwright` environment variable
//   3. `'puppeteer'` (default, keeps backward compatibility)
//
// Playwright is loaded lazily via `require()` — installing it remains opt-in
// (declared in `optionalDependencies`), so the default install footprint and
// the existing puppeteer-only workflow are unchanged.

export const SUPPORTED_ENGINES = ['puppeteer', 'playwright'] as const;
export type EngineName = (typeof SUPPORTED_ENGINES)[number];
export const DEFAULT_ENGINE: EngineName = 'puppeteer';

// Map of canonical engine name → npm package name. The package is required
// lazily so users who never set --browser-engine playwright don't have to
// install playwright.
const ENGINE_PACKAGE: Record<EngineName, string> = {
  puppeteer: 'puppeteer',
  playwright: 'playwright',
};

// Browser/Page/Response handles — typed as `any` because puppeteer and
// playwright expose different concrete classes and we only call the shared
// methods on them. Using a structural alias keeps call sites typed without
// pulling either package's full type tree into every caller.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type Browser = any;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type Page = any;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type BrowserResponse = any;

export interface EngineWrapper {
  browser: Browser;
  engine: EngineName;
}

// Normalise `name` to one of SUPPORTED_ENGINES, falling back to the env var
// then to DEFAULT_ENGINE. Throws on an unknown explicit value so the user
// notices typos at startup rather than after the first browser call fails.
export function resolveEngine(name?: string | null): EngineName {
  const raw = (name || process.env.HTML_SNAPSHOT_BROWSER || DEFAULT_ENGINE).toLowerCase();
  if (!(SUPPORTED_ENGINES as readonly string[]).includes(raw)) {
    throw new Error(
      `unsupported browser engine '${name}' (expected one of: ${SUPPORTED_ENGINES.join(', ')})`,
    );
  }
  return raw as EngineName;
}

interface EngineLauncher {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  launch(opts: Record<string, any>): Promise<Browser>;
}

// Lazy-require the engine package and return its launcher. We unify the two
// shapes here (puppeteer is its own launcher; playwright exposes
// `chromium.launch`) so the rest of the code only calls `launcher.launch(...)`.
function loadEngine(engine: EngineName): EngineLauncher {
  const pkg = ENGINE_PACKAGE[engine];
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let mod: any;
  try {
    mod = require(pkg);
  } catch (err) {
    const hint = engine === 'playwright'
      ? `Install playwright + its browser: in the libpag repo run\n`
        + `\`npm install --prefix tools/html-snapshot playwright\` then\n`
        + `\`npx --prefix tools/html-snapshot playwright install chromium\`.`
      : `Install puppeteer: in the libpag repo run \`npm install --prefix tools/html-snapshot\`.\n`
        + `If you installed @libpag/pagx via npm, the bundled launcher installs it automatically `
        + `on first use — re-run your pagx command (or unset PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL).`;
    const e = new Error(
      `failed to load '${pkg}' for browser engine '${engine}': `
      + `${err && (err as Error).message ? (err as Error).message : err}\n${hint}`,
    );
    (e as Error & { cause?: unknown }).cause = err;
    throw e;
  }
  if (engine === 'playwright') {
    if (!mod.chromium) {
      throw new Error(`'playwright' module does not expose chromium — got ${typeof mod}`);
    }
    return mod.chromium as EngineLauncher;
  }
  return mod as EngineLauncher;
}

export interface LaunchOptions {
  engine?: string | null;
  headless?: boolean;
  args?: string[];
  executablePath?: string;
}

// Launch a headless browser using the requested engine. Returns the wrapper
// `{ browser, engine }`; downstream helpers in this file accept that wrapper
// directly so callers don't have to remember to thread the engine name
// alongside the browser handle.
export async function launchBrowser(opts?: LaunchOptions): Promise<EngineWrapper> {
  const engine = resolveEngine(opts && opts.engine);
  const launcher = loadEngine(engine);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const launchOpts: Record<string, any> = {
    headless: opts && opts.headless !== undefined ? opts.headless : true,
    // `--no-sandbox` is needed inside most CI/Docker environments where
    // Chromium's sandbox can't be set up. `--font-render-hinting=none` keeps
    // glyph metrics stable across machines so the baseline and subset PNGs
    // diff cleanly. Both engines accept these flags identically.
    args: (opts && opts.args) || ['--no-sandbox', '--font-render-hinting=none'],
  };
  // Engine-specific executablePath plumbing:
  //   - puppeteer auto-reads PUPPETEER_EXECUTABLE_PATH; we don't have to
  //     forward anything.
  //   - playwright ignores env vars for the binary location, so callers
  //     who want to point it at a pre-installed Chromium (e.g. distro
  //     `/usr/bin/chromium` inside the bench Docker image, which avoids
  //     the ~200 MB Chrome-for-Testing download and matches what the
  //     puppeteer path already uses) must pass `executablePath`. We
  //     accept either an explicit `opts.executablePath` or the
  //     `PLAYWRIGHT_EXECUTABLE_PATH` env var, with the option winning.
  if (engine === 'playwright') {
    const exe = (opts && opts.executablePath)
      || process.env.PLAYWRIGHT_EXECUTABLE_PATH
      || '';
    if (exe) launchOpts.executablePath = exe;
  }
  const browser = await launcher.launch(launchOpts);
  return { browser, engine };
}

// True if the caller passed our `{ browser, engine }` wrapper rather than a
// bare puppeteer/playwright Browser instance. Used by helpers that want to
// stay backward compatible with code paths still passing a raw browser.
export function isWrapper(x: unknown): x is EngineWrapper {
  return !!(x && typeof x === 'object'
    && (x as EngineWrapper).browser
    && typeof (x as EngineWrapper).engine === 'string');
}

// Unwrap a `{ browser, engine }` wrapper into its two components, defaulting
// the engine to puppeteer when a bare browser handle is passed. Centralised
// because every helper below needs the same coercion.
export function unwrap(wrapperOrBrowser: EngineWrapper | Browser): EngineWrapper {
  if (isWrapper(wrapperOrBrowser)) {
    return { browser: wrapperOrBrowser.browser, engine: wrapperOrBrowser.engine };
  }
  return { browser: wrapperOrBrowser, engine: DEFAULT_ENGINE };
}

export interface ViewportSpec {
  width: number;
  height: number;
  deviceScaleFactor?: number;
}

export interface NewPageOptions {
  viewport?: ViewportSpec | null;
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
export async function newPage(
  wrapperOrBrowser: EngineWrapper | Browser,
  opts?: NewPageOptions,
): Promise<Page> {
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
export async function setViewport(
  page: Page,
  engine: EngineName,
  viewport: ViewportSpec | null | undefined,
): Promise<void> {
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

export type WaitUntilToken = 'load' | 'domcontentloaded' | 'networkidle' | 'networkidle0' | string;

// Translate our generic "fully settled" waitUntil token into the engine's
// flavour. Puppeteer's `'networkidle0'` (no in-flight requests for 500ms) and
// Playwright's `'networkidle'` are semantically the same; both engines
// accept `'load'` / `'domcontentloaded'` verbatim.
export function mapWaitUntil(engine: EngineName, token?: WaitUntilToken): string {
  if (!token || token === 'networkidle') {
    return engine === 'playwright' ? 'networkidle' : 'networkidle0';
  }
  if (token === 'networkidle0') {
    return engine === 'playwright' ? 'networkidle' : 'networkidle0';
  }
  return token;
}

export interface CookieParam {
  name: string;
  value: string;
  url?: string;
  domain?: string;
  path?: string;
  expires?: number;
  httpOnly?: boolean;
  secure?: boolean;
  sameSite?: string;
}

// Engine-agnostic cookie injection. Both APIs accept the same shape
// (`{ name, value, url|domain }`); the difference is just where the call
// lives — on the page for puppeteer, on the context for playwright.
// Cookies must include either `url` or `domain` so the browser knows which
// origin they belong to.
export async function addCookies(
  page: Page,
  engine: EngineName,
  cookies: CookieParam[] | null | undefined,
): Promise<void> {
  if (!cookies || cookies.length === 0) return;
  if (engine === 'playwright') {
    await page.context().addCookies(cookies);
    return;
  }
  await page.setCookie(...cookies);
}

// Read a response body as a Buffer. Puppeteer exposes `response.buffer()`;
// Playwright renamed it to `response.body()`. Both return a Node Buffer
// with the raw bytes (no charset coercion). May throw if the response
// detached (page navigated away before the body was consumed) — callers
// should wrap in try/catch and treat the failure as "skip this resource".
export async function responseBytes(
  resp: BrowserResponse,
  engine: EngineName,
): Promise<Buffer> {
  if (engine === 'playwright') {
    return resp.body();
  }
  return resp.buffer();
}

// Register a script that runs in every new document this page loads, before
// any of the page's own scripts. Used by snapshot.js to inject the snapshot
// helper bundle into `window.__pagxSnapshot` once at navigation time so the
// subsequent `page.evaluate(...)` calls only trigger the entry function
// instead of re-shipping the ~80 KB helper source over CDP per call.
//
// Puppeteer: `page.evaluateOnNewDocument(string|fn)`.
// Playwright: `page.addInitScript({ content: string })`.
export async function addInitScript(
  page: Page,
  engine: EngineName,
  script: string,
): Promise<void> {
  if (engine === 'playwright') {
    await page.addInitScript({ content: script });
    return;
  }
  await page.evaluateOnNewDocument(script);
}

// Best-effort wait for the page's network to go quiet (no in-flight requests
// for ~500ms), used *after* a `load`-gated navigation. Unlike passing
// `waitUntil: 'networkidle'` to `page.goto`, this is a separate, bounded step
// the caller can treat as non-fatal: the document has already loaded, so a
// CDN that keeps trickling requests (Google Fonts, icon webfonts, the Tailwind
// Play CDN) only costs `timeoutMs` of extra settle time instead of failing the
// whole navigation. Resolves (never rejects) — returns true if the network
// actually settled, false if it timed out or the wait wasn't supported.
//
// Puppeteer: `page.waitForNetworkIdle({ idleTime, timeout })`.
// Playwright: `page.waitForLoadState('networkidle', { timeout })`.
export async function waitForNetworkIdle(
  page: Page,
  engine: EngineName,
  timeoutMs: number,
): Promise<boolean> {
  try {
    if (engine === 'playwright') {
      await page.waitForLoadState('networkidle', { timeout: timeoutMs });
    } else {
      await page.waitForNetworkIdle({ idleTime: 500, timeout: timeoutMs });
    }
    return true;
  } catch (_) {
    return false;
  }
}

// Patterns thrown by puppeteer / playwright when the browser binary is
// missing or wasn't downloaded into the cache the engine looks at. Lives in
// one place so the diagnostic helper below stays in sync with what the
// engines actually emit.
const LAUNCH_MISSING_BROWSER_RE = /Could not find Chrome|Failed to launch the browser process|Executable doesn't exist/i;

// Inspect a `launchBrowser` rejection and return a list of stderr lines that
// tell the user how to install the missing browser binary, or `null` when
// the error is something other than "browser cache empty / wrong path".
//
// Centralised so the exact wording (and the install commands embedded in it)
// lives next to the launcher itself, instead of being copied into every
// entry point that calls `launchBrowser`. Returning lines (rather than
// printing) lets each caller route them through its own logger / log
// prefix; snapshot.js prepends `LOG_PREFIX` to each line, the HTTP server
// would prepend its own banner, etc.
export function formatLaunchHint(err: unknown, engine: EngineName): string[] | null {
  const msg = (err && typeof (err as Error).message === 'string')
    ? (err as Error).message
    : String(err);
  if (!LAUNCH_MISSING_BROWSER_RE.test(msg)) return null;
  const lines = [`failed to launch headless Chromium for engine '${engine}'. Try:`];
  if (engine === 'playwright') {
    lines.push(`  npx --prefix tools/html-snapshot playwright install chromium`);
  } else {
    // In the libpag repo the `--prefix tools/html-snapshot` form resolves the
    // bundled puppeteer; when installed via @libpag/pagx the bundled launcher
    // normally installs Chromium for you, so this is the explicit fallback.
    lines.push(`  PUPPETEER_CACHE_DIR="$HOME/.cache/puppeteer" \\`);
    lines.push(`    npx --prefix tools/html-snapshot puppeteer browsers install chrome`);
  }
  return lines;
}
