#!/usr/bin/env node
/**
 * html-snapshot HTTP service.
 *
 * Wraps the snapshot pipeline (the same one `snapshot.js` runs from the CLI)
 * behind a tiny HTTP server. Send an HTML string in the request body; receive
 * either the processed, flat, absolute-positioned HTML, or the converted
 * PAGX document, back — the same artefacts that
 * `node snapshot.js page.html` and `pagx import --format html` produce on
 * disk.
 *
 * Lifecycle:
 *   • One headless browser is launched at startup and kept alive for the
 *     lifetime of the process. Each request opens a fresh page, runs the
 *     pipeline, and closes the page — no temp tabs accumulate even under
 *     sustained load. Concurrent requests are served in parallel against
 *     the same browser.
 *   • PAGX conversion (when requested) shells out to the `pagx` binary
 *     once per request. The binary is located via --pagx-bin / $PAGX_BIN;
 *     server boot does not require pagx to exist (only `format=pagx` /
 *     `format=both` requests do).
 *
 * Endpoints:
 *   POST /snapshot
 *     Request body:
 *       - Content-Type: application/json  → { html | url: string,
 *                                              format?: "html"|"pagx"|"both",
 *                                              options?: { ... } }
 *                                            Provide exactly one of `html` or `url`.
 *       - Any other Content-Type           → the entire body is treated as the
 *                                            HTML payload (format defaults to
 *                                            "html"; use the JSON envelope or
 *                                            the `?format=` query param to
 *                                            request PAGX).
 *     Response:
 *       - format=html (default)            → text/html with the processed HTML
 *                                            (plus X-Snapshot-Width /
 *                                            X-Snapshot-Height /
 *                                            X-Snapshot-Format headers).
 *       - format=pagx                      → application/xml with the PAGX
 *                                            document (same headers).
 *       - format=both                      → JSON only ({ html, pagx, width,
 *                                            height }). Returns 406 unless the
 *                                            client sends Accept: application/json.
 *       - Accept: application/json (any
 *         single-format request)           → { html|pagx, width, height }.
 *     The `options` object accepts: viewportWidth, viewportHeight, waitMs,
 *     selector, inlineIconFonts, inferFlex, plus cookies / headers for URL
 *     inputs (all optional). `inferFlex` only applies to PAGX output.
 *
 *   GET /snapshot?url=<http(s)-url>&format=html|pagx|both&...options
 *     Convenience route for "fetch this page and snapshot it" — no body
 *     required. Same response semantics as POST. Supported query params:
 *       url (required), format, viewportWidth, viewportHeight, waitMs,
 *       selector, inlineIconFonts, inferFlex.
 *
 *   GET /health
 *     200 { status: "ok", engine, pagxBin, pagxBinExists, activeRequests }
 *     once the browser is up.
 *
 * Usage:
 *   node server.js [--port 8787] [--host 127.0.0.1]
 *                  [--browser-engine puppeteer|playwright]
 *                  [--pagx-bin <path>]
 *                  [--max-body-mb 32]
 */

'use strict';

const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');

const { launchBrowser, resolveEngine, SUPPORTED_ENGINES } = require('./lib/browser-engine');
const { runSnapshot } = require('./lib/snapshot-runner');
const { runPagxImport, defaultPagxBin, PagxImportError } = require('./lib/pagx-runner');
const { errMessage, isHttpUrl, makeFail, parseNumber, validateCookies, validateHeaders, SNAPSHOT_DEFAULTS } = require('./lib/cli');
const {
  HttpError,
  safeQueryParam,
  setNumberParam,
  setStringParam,
  setBoolParam,
  wantsJsonResponse,
} = require('./lib/http-utils');

// Output formats the server understands. `html` is the original (and default)
// behaviour: return the flat snapshot HTML straight from the browser.
// `pagx` runs the snapshot through `pagx import --format html` and returns
// the resulting XML. `both` returns both in a single JSON envelope (only
// available to JSON callers; text responses can carry one format).
const SUPPORTED_FORMATS = ['html', 'pagx', 'both'];
const DEFAULT_FORMAT = 'html';

const LOG_PREFIX = 'html-snapshot-server: ';
const fail = makeFail('html-snapshot-server');

function log(msg) {
  // Stderr keeps server diagnostics out of stdout, so wrappers that pipe
  // a single request (e.g. via `curl --output -`) can keep stdout clean.
  console.error(`${LOG_PREFIX}${msg}`);
}

function printUsage() {
  console.log(`Usage: node server.js [options]

Run an HTTP service that wraps the html-snapshot pipeline. The browser is
launched once at startup and reused across requests.

Options:
  --port <n>               Listen port (default 8787; env: PORT)
  --host <addr>            Listen address (default 127.0.0.1; env: HOST)
  --browser-engine <name>  ${SUPPORTED_ENGINES.join(' | ')} (default puppeteer;
                           env: HTML_SNAPSHOT_BROWSER)
  --pagx-bin <path>        Path to the pagx CLI used for format=pagx|both
                           (default: \$PAGX_BIN or
                           <repo-root>/cmake-build-debug/pagx)
  --max-body-mb <n>        Maximum request body size in MB (default 32;
                           env: MAX_BODY_MB)
  -h, --help               Show this help

Endpoints:
  POST /snapshot         — body is raw HTML, or JSON { html|url, format?,
                           options? }. \`format\` is one of html (default),
                           pagx, or both. text/html or application/xml is
                           returned for single-format requests; format=both
                           requires Accept: application/json.
  GET  /snapshot?url=…   — fetch the URL and run the same pipeline (no body).
                           Optional query params: format, viewportWidth,
                           viewportHeight, waitMs, selector, inlineIconFonts,
                           inferFlex.
  GET  /health           — liveness probe.`);
}

function parseServerArgs(argv) {
  const opts = {
    port: Number(process.env.PORT) || 8787,
    host: process.env.HOST || '127.0.0.1',
    browserEngine: resolveEngine(process.env.HTML_SNAPSHOT_BROWSER),
    maxBodyBytes: (Number(process.env.MAX_BODY_MB) || 32) * 1024 * 1024,
    // `pagxBin` is resolved eagerly so logs show the absolute path the
    // server will spawn. The binary itself is not required at boot — only
    // requests that ask for `format=pagx`/`format=both` actually need it.
    pagxBin: defaultPagxBin(),
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') {
      printUsage();
      process.exit(0);
    } else if (a === '--port') {
      opts.port = parseNumber('--port', argv[++i], { min: 1, max: 65535, fail });
    } else if (a === '--host') {
      opts.host = argv[++i];
    } else if (a === '--browser-engine') {
      try {
        opts.browserEngine = resolveEngine(argv[++i]);
      } catch (err) {
        fail(errMessage(err));
      }
    } else if (a === '--pagx-bin') {
      const value = argv[++i];
      if (!value) fail('--pagx-bin requires a path argument');
      opts.pagxBin = path.resolve(value);
    } else if (a === '--max-body-mb') {
      const mb = parseNumber('--max-body-mb', argv[++i], { min: 1, fail });
      opts.maxBodyBytes = mb * 1024 * 1024;
    } else {
      // server's UX has long printed the usage block on unknown options to
      // help operators correct typos. Keep that behaviour — fail() alone
      // would only emit the one-line error.
      printUsage();
      fail(`unknown option: ${a}`);
    }
  }
  return opts;
}

// Read the full request body, rejecting once the cumulative size exceeds
// `maxBytes`. Node's `http` doesn't enforce body limits by default, so a
// runaway POST would otherwise pin RAM until the browser pipeline blew up
// further downstream.
//
// When the limit is hit, we *drain* the rest of the body instead of
// destroying the request. Destroying mid-flight leaves the client with a
// "Connection reset by peer" — readers see no status line at all. Draining
// keeps the connection alive long enough to flush a proper 413 from
// handleSnapshot. The cost is a few wasted bytes on the socket; the
// upside is a clean error contract over the wire.
function readBody(req, maxBytes) {
  return new Promise((resolve, reject) => {
    // Honor an advisory Content-Length first so a 100 GB POST doesn't
    // have to dribble the whole payload before we say no.
    const declared = Number(req.headers['content-length']);
    if (Number.isFinite(declared) && declared > maxBytes) {
      // Still consume the body so the keep-alive socket can be reused
      // (otherwise some clients hang waiting for FIN).
      req.resume();
      req.on('end', () => reject(new HttpError(413, `request body exceeds ${maxBytes} bytes`)));
      req.on('error', reject);
      return;
    }
    let total = 0;
    let overflow = false;
    const chunks = [];
    req.on('data', (chunk) => {
      if (overflow) return;
      total += chunk.length;
      if (total > maxBytes) {
        overflow = true;
        // Free the buffered chunks immediately — we already know we're
        // not returning them.
        chunks.length = 0;
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => {
      if (overflow) {
        reject(new HttpError(413, `request body exceeds ${maxBytes} bytes`));
      } else {
        resolve(Buffer.concat(chunks));
      }
    });
    req.on('error', reject);
  });
}

function jsonResponse(res, status, payload) {
  res.statusCode = status;
  res.setHeader('Content-Type', 'application/json; charset=utf-8');
  res.end(JSON.stringify(payload));
}

function jsonError(res, status, message) {
  jsonResponse(res, status, { error: message });
}

// Validate and normalise the user-supplied `format` selector. Accepts a
// string (single value, or comma-separated list — handy in query strings),
// an array of strings (e.g. ["html", "pagx"], the multi-format shorthand),
// or undefined (defaults to html). Throws an `HttpError 400` on anything we
// don't recognise so the client gets a clear contract violation instead of
// a silent fallback.
//
// Single values may be any of the supported formats including 'both'. Lists
// (whether array-typed or comma-separated string) only accept 'html'/'pagx'
// as elements; the literal 'both' as a list element makes no semantic sense
// and is rejected.
function normaliseFormat(rawFormat) {
  if (rawFormat === undefined || rawFormat === null) return DEFAULT_FORMAT;

  let parts;
  let isList;
  if (typeof rawFormat === 'string') {
    const trimmed = rawFormat.trim().toLowerCase();
    if (!trimmed) return DEFAULT_FORMAT;
    isList = trimmed.includes(',');
    parts = isList
      ? trimmed.split(',').map((p) => p.trim()).filter(Boolean)
      : [trimmed];
  } else if (Array.isArray(rawFormat)) {
    isList = true;
    parts = rawFormat
      .filter((p) => typeof p === 'string')
      .map((p) => p.trim().toLowerCase())
      .filter(Boolean);
  } else {
    throw new HttpError(400, 'format must be a string or array of strings');
  }

  if (parts.length === 0) return DEFAULT_FORMAT;

  if (isList) {
    const set = new Set(parts);
    for (const v of set) {
      if (v !== 'html' && v !== 'pagx') {
        throw new HttpError(400, `unsupported format value '${v}' (expected html|pagx)`);
      }
    }
    return set.size === 1 ? [...set][0] : 'both';
  }

  const single = parts[0];
  if (!SUPPORTED_FORMATS.includes(single)) {
    throw new HttpError(
      400,
      `unsupported format '${single}' (expected ${SUPPORTED_FORMATS.join('|')})`,
    );
  }
  return single;
}

// Materialise the incoming HTML on disk so the existing pipeline (which
// drives the browser via `page.goto(url)`) can navigate to it as a file://
// URL. Each call returns a `{ file, cleanup }` pair scoped to a unique
// temp directory; the caller MUST invoke `cleanup()` once the snapshot is
// done so we don't leak temp dirs under `$TMPDIR`. We deliberately avoid
// `page.setContent()` here because it would force us to fork the
// page-loader's `goto + waitForFunction + waitMs` chain.
function writeHtmlToTempFile(html) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-server-'));
  const file = path.join(dir, 'input.html');
  fs.writeFileSync(file, html, 'utf8');
  return {
    file,
    cleanup() {
      try {
        fs.rmSync(dir, { recursive: true, force: true });
      } catch (_) {
        // Best-effort. A stale temp dir is annoying but not fatal —
        // the OS cleans `$TMPDIR` on reboot.
      }
    },
  };
}

// Parse a POST body into a `{ html | url, format, options }` source
// descriptor. JSON bodies may carry either an `html` string or a `url`
// string (exactly one), plus an optional top-level `format` selector and
// `options` object; any other Content-Type is treated as a raw HTML payload
// (format defaults to html — the URL fallback isn't useful here, and PAGX
// can be requested via the ?format=pagx query string instead).
function parsePostSource(req, rawBuf) {
  const ct = (req.headers['content-type'] || '').split(';')[0].trim().toLowerCase();
  const rawText = rawBuf.toString('utf8');
  // Allow `?format=pagx` on the URL even when posting raw HTML — handy for
  // `curl --data-binary @page.html '/snapshot?format=pagx'`-style clients
  // that don't want to construct a JSON envelope. Query takes effect only
  // when the JSON body itself does not specify a format.
  const queryFormat = safeQueryParam(req, 'format');

  if (ct === 'application/json') {
    let parsed;
    try {
      parsed = JSON.parse(rawText);
    } catch (err) {
      throw new HttpError(400, `invalid JSON body: ${errMessage(err)}`);
    }
    if (!parsed || typeof parsed !== 'object') {
      throw new HttpError(400, 'json body must be an object');
    }
    const hasHtml = typeof parsed.html === 'string';
    const hasUrl = typeof parsed.url === 'string';
    if (hasHtml && hasUrl) {
      throw new HttpError(400, 'json body must contain either "html" or "url", not both');
    }
    if (!hasHtml && !hasUrl) {
      throw new HttpError(400, 'json body must contain a string "html" or "url" field');
    }
    const options = (parsed.options && typeof parsed.options === 'object') ? parsed.options : {};
    const format = normaliseFormat(parsed.format !== undefined ? parsed.format : queryFormat);
    if (hasUrl) {
      if (!isHttpUrl(parsed.url)) {
        throw new HttpError(400, 'url must start with http:// or https://');
      }
      return { url: parsed.url, options, format };
    }
    if (!parsed.html.trim()) {
      throw new HttpError(400, 'empty "html" field');
    }
    return { html: parsed.html, options, format };
  }
  if (!rawText || !rawText.trim()) {
    throw new HttpError(400, 'empty html body');
  }
  return { html: rawText, options: {}, format: normaliseFormat(queryFormat) };
}

// Parse a GET query string into the same `{ url, options }` source
// descriptor that `parsePostSource` produces. The GET route is the
// "fetch and snapshot" convenience path: it requires a `url` parameter
// and accepts the same scalar options the JSON body exposes (cookies /
// headers are deliberately omitted — repeatable structured params get
// awkward in query strings; use POST when you need them).
function parseGetSource(req) {
  let parsedUrl;
  try {
    // `req.url` is always a relative URL; the base is only there so the
    // `URL` constructor can parse it.
    parsedUrl = new URL(req.url, 'http://localhost');
  } catch (err) {
    throw new HttpError(400, `malformed request URL: ${errMessage(err)}`);
  }
  const params = parsedUrl.searchParams;
  const url = params.get('url');
  if (!url) {
    throw new HttpError(400, 'missing required query param: url');
  }
  if (!isHttpUrl(url)) {
    throw new HttpError(400, 'url must start with http:// or https://');
  }
  // Re-shape the query string into the same scalar object that
  // `resolveOptions` consumes from JSON bodies. We coerce here so
  // resolveOptions can stay JSON-shaped (booleans / numbers, not strings).
  // Validation of numeric ranges is deferred to `resolveOptions`, which
  // silently drops out-of-range values and falls back to its defaults.
  const queryOptions = {};
  setNumberParam(params, queryOptions, 'viewportWidth');
  setNumberParam(params, queryOptions, 'viewportHeight');
  setNumberParam(params, queryOptions, 'waitMs');
  setStringParam(params, queryOptions, 'selector');
  setBoolParam(params, queryOptions, 'inlineIconFonts');
  setBoolParam(params, queryOptions, 'inferFlex');
  const format = normaliseFormat(params.get('format'));
  return { url, options: queryOptions, format };
}

// Whitelist + coerce the small set of pipeline options we expose over HTTP.
// Anything else in `reqOptions` is silently dropped — this matches the CLI
// (which only forwards documented flags) and prevents callers from
// accidentally toggling internal knobs (e.g. browser engine, init scripts).
//
// `cookies` and `headers` are only honoured for URL-source requests; the
// snapshot pipeline ignores them for file:// URLs and forwarding them
// would only confuse the caller. The flag is plumbed via `forUrl`.
//
// `inferFlex` is special: it does not affect the snapshot pipeline itself,
// only the downstream `pagx import` step (`--html-infer-flex`). The
// resolved value is therefore returned out-of-band so the caller can pass
// it to runPagxImport without tangling it into runSnapshot's option set.
function resolveOptions(reqOptions, forUrl) {
  const out = {};
  const r = reqOptions || {};
  if (Number.isFinite(r.viewportWidth) && r.viewportWidth > 0) out.viewportWidth = r.viewportWidth;
  if (Number.isFinite(r.viewportHeight) && r.viewportHeight > 0) out.viewportHeight = r.viewportHeight;
  if (Number.isFinite(r.waitMs) && r.waitMs >= 0) out.waitMs = r.waitMs;
  if (typeof r.selector === 'string') out.selector = r.selector;
  if (typeof r.inlineIconFonts === 'boolean') out.inlineIconFonts = r.inlineIconFonts;
  if (forUrl) {
    // cookies: [{ name, value }] only — anything else is dropped (validators
    // shared with the CLI live in lib/cli.js).
    const cookies = validateCookies(r.cookies);
    if (cookies.length > 0) out.cookies = cookies;
    // headers: accept [[key, value], …] (matches CLI output) or { Key: Value }
    // (convenient for JSON callers); both normalise to [[k, v], …] inside
    // validateHeaders.
    const headers = validateHeaders(r.headers);
    if (headers.length > 0) out.headers = headers;
  }
  return out;
}

// Pull the PAGX-specific knobs out of the request's `options` object. Kept
// separate from `resolveOptions` because they don't belong in the snapshot
// pipeline's argument set — they only matter for the `pagx import` step.
function resolvePagxOptions(reqOptions) {
  const r = reqOptions || {};
  // `inferFlex` defaults to true (matching html2pagx). Only flip it when the
  // caller passes an explicit boolean false; a non-boolean value is ignored.
  const inferFlex = r.inferFlex === false ? false : true;
  return { inferFlex };
}

// Common pipeline path for both POST and GET. `source` is the descriptor
// returned by parsePostSource / parseGetSource; it carries exactly one of
// `html` or `url`, plus a normalised `format` selector. We resolve options,
// set up a temp file when needed, run the snapshot, optionally invoke
// `pagx import`, and shape the response — all wrapped in a single
// try/finally so the temp dir always gets cleaned up.
async function handleSnapshot(req, res, ctx, source) {
  let tmp = null;
  try {
    const isUrlSource = typeof source.url === 'string';
    const runOpts = resolveOptions(source.options, isUrlSource);
    const pagxOpts = resolvePagxOptions(source.options);
    const format = source.format || DEFAULT_FORMAT;
    const wantsPagx = format === 'pagx' || format === 'both';
    const wantsHtml = format === 'html' || format === 'both';

    // Multi-format requests must be JSON — text responses can carry only
    // one body. Surface a 406 up front rather than picking one format and
    // surprising the client.
    const acceptsJson = wantsJsonResponse(req);
    if (format === 'both' && !acceptsJson) {
      throw new HttpError(
        406,
        'format=both requires Accept: application/json (text responses can carry only one format)',
      );
    }

    let targetUrl;
    let inputDesc;
    if (isUrlSource) {
      targetUrl = source.url;
      inputDesc = `url=${source.url}`;
    } else {
      tmp = writeHtmlToTempFile(source.html);
      targetUrl = `file://${tmp.file}`;
      inputDesc = `in=${source.html.length}B`;
    }
    const result = await runSnapshot(ctx.engineHandle, targetUrl, runOpts);

    let pagx = null;
    if (wantsPagx) {
      try {
        pagx = await runPagxImport({
          pagxBin: ctx.opts.pagxBin,
          html: result.html,
          inferFlex: pagxOpts.inferFlex,
          log: (msg) => log(`pagx: ${msg}`),
        });
      } catch (err) {
        // The snapshot itself succeeded; we only failed downstream. 502
        // (bad gateway) is the closest fit — pagx is the upstream that
        // misbehaved, and the caller can tell snapshot-vs-pagx failures
        // apart by the status code without parsing the message body.
        const detail = err instanceof PagxImportError && err.stderr
          ? `${err.message}: ${err.stderr.trim().split('\n').slice(-3).join(' / ')}`
          : errMessage(err);
        throw new HttpError(502, `pagx import failed: ${detail}`);
      }
    }

    log(
      `${req.method} /snapshot ok ${result.width}x${result.height} `
      + `${inputDesc} format=${format} html=${wantsHtml ? result.html.length : 0}B `
      + `pagx=${pagx ? pagx.length : 0}B ${Date.now() - source.startedAt}ms`,
    );

    res.setHeader('X-Snapshot-Width', String(result.width));
    res.setHeader('X-Snapshot-Height', String(result.height));
    res.setHeader('X-Snapshot-Format', format);

    if (acceptsJson) {
      const body = { width: result.width, height: result.height };
      if (wantsHtml) body.html = result.html;
      if (wantsPagx) body.pagx = pagx;
      jsonResponse(res, 200, body);
      return;
    }

    // Single-format text response. format is guaranteed to be 'html' or
    // 'pagx' here (the 'both' case bailed out above with a 406).
    res.statusCode = 200;
    if (format === 'pagx') {
      // PAGX is a UTF-8 XML document; application/xml is the closest
      // standard MIME type. Servers behind the curl pipeline-into-file
      // case still get a clean text payload they can pipe directly to
      // `pagx resolve` if they save it to disk.
      res.setHeader('Content-Type', 'application/xml; charset=utf-8');
      res.end(pagx);
    } else {
      res.setHeader('Content-Type', 'text/html; charset=utf-8');
      res.end(result.html);
    }
  } finally {
    if (tmp) tmp.cleanup();
  }
}

// Common error-mapping shell shared by both routes. Captures `startedAt`
// for the source descriptor so handleSnapshot can log elapsed time.
async function runRoute(req, res, ctx, sourceFactory) {
  ctx.activeRequests += 1;
  const startedAt = Date.now();
  try {
    const source = await sourceFactory();
    source.startedAt = startedAt;
    await handleSnapshot(req, res, ctx, source);
  } catch (err) {
    const status = err instanceof HttpError ? err.status : 500;
    const message = errMessage(err);
    log(`${req.method} ${(req.url || '').split('?')[0]} ${status} ${message}`);
    if (!res.headersSent) {
      jsonError(res, status, message);
    } else {
      // Headers already flushed (shouldn't happen for our handlers,
      // but guard against future changes that stream the body). Close
      // the socket so the client sees a truncated response rather
      // than a silent hang.
      res.destroy();
    }
  } finally {
    ctx.activeRequests -= 1;
  }
}

// Probe an executable file path. Returns true only if the path exists AND
// is executable for the current user; any error (ENOENT, EACCES, …)
// collapses to false so callers can render one diagnostic line instead of a
// stack trace. Lives in server.js because it's only used at boot.
function isExecutable(filepath) {
  try {
    fs.accessSync(filepath, fs.constants.X_OK);
    return true;
  } catch (_) {
    return false;
  }
}

// Build a fallback error handler that maps an unexpected request-handler
// throw to a clean 500 (or a socket destroy if headers already flushed).
// Used as the trailing `.catch` on `runRoute`'s returned promise — in
// practice runRoute should never let a throw escape, but the catch keeps an
// unexpected error from killing the process.
function makeUnexpectedHandler(res) {
  return (err) => {
    const message = errMessage(err);
    log(`unhandled handler error: ${message}`);
    if (!res.headersSent) {
      jsonError(res, 500, message);
    } else {
      res.destroy();
    }
  };
}

function handleHealth(req, res, ctx) {
  jsonResponse(res, 200, {
    status: 'ok',
    engine: ctx.engineHandle.engine,
    pagxBin: ctx.opts.pagxBin,
    pagxBinExists: ctx.pagxBinExists,
    activeRequests: ctx.activeRequests,
  });
}

function handlePostSnapshot(req, res, ctx) {
  // Both /snapshot routes funnel through runRoute, which owns the try/catch
  // and 4xx/5xx mapping. Fire-and-forget — runRoute manages its own response.
  runRoute(req, res, ctx, async () => {
    const raw = await readBody(req, ctx.opts.maxBodyBytes);
    return parsePostSource(req, raw);
  }).catch(makeUnexpectedHandler(res));
}

function handleGetSnapshot(req, res, ctx) {
  // GET takes no body — the source is fully derived from the query string.
  // Wrapped in an async factory so parse errors hit runRoute's error mapper
  // instead of becoming a synchronous throw.
  runRoute(req, res, ctx, async () => parseGetSource(req)).catch(makeUnexpectedHandler(res));
}

// Declarative route table. Adding a new endpoint is a one-line append here
// instead of an extra `if` branch in the request callback. Three entries is
// small enough that a linear scan is fine; the cost is dwarfed by the
// per-request work downstream.
const ROUTES = [
  { method: 'GET', path: '/health', handle: handleHealth },
  { method: 'POST', path: '/snapshot', handle: handlePostSnapshot },
  { method: 'GET', path: '/snapshot', handle: handleGetSnapshot },
];

function dispatch(req, res, ctx) {
  // Strip the query string so future parameters can land on the URL without
  // breaking the route match.
  const urlPath = (req.url || '').split('?')[0];
  for (const route of ROUTES) {
    if (route.method === req.method && route.path === urlPath) {
      route.handle(req, res, ctx);
      return;
    }
  }
  jsonError(res, 404, `not found: ${req.method} ${urlPath}`);
}

async function startServer(opts) {
  log(`launching ${opts.browserEngine}…`);
  const engineHandle = await launchBrowser({ engine: opts.browserEngine });
  log(`browser ready (engine=${engineHandle.engine})`);

  // Probe the pagx binary up front so the operator gets one boot-time log
  // line instead of discovering on the first PAGX request that the binary
  // isn't built. We don't fail boot if it's missing — HTML-only callers
  // shouldn't be blocked by an absent pagx — but the diagnostic surfaces
  // the situation early.
  const pagxBinExists = isExecutable(opts.pagxBin);
  if (pagxBinExists) {
    log(`pagx binary found at ${opts.pagxBin}`);
  } else {
    log(
      `pagx binary not executable at ${opts.pagxBin} `
      + `(format=pagx|both requests will fail until it is built; `
      + `pass --pagx-bin <path> or set $PAGX_BIN)`,
    );
  }

  const ctx = { engineHandle, opts, activeRequests: 0, pagxBinExists };

  const server = http.createServer((req, res) => dispatch(req, res, ctx));

  server.listen(opts.port, opts.host, () => {
    log(`listening on http://${opts.host}:${opts.port}`);
  });

  // Coordinated shutdown: stop accepting new connections, drain in-flight
  // requests, then close the browser. Long-running snapshots get up to ~10s
  // to finish before the safety-net timer forces an exit so a stuck page
  // can't pin the service open forever.
  const closeServer = () => new Promise((resolve) => {
    server.close((err) => {
      if (err) log(`http server close error: ${errMessage(err)}`);
      else log('http server closed');
      resolve();
    });
  });
  let shuttingDown = false;
  const shutdown = async (signal) => {
    if (shuttingDown) return;
    shuttingDown = true;
    log(`received ${signal}, shutting down…`);
    const forceExitTimer = setTimeout(() => {
      log('forced exit after 10s shutdown timeout');
      process.exit(1);
    }, 10000);
    forceExitTimer.unref();
    // server.close doesn't resolve until every keep-alive socket closes,
    // so the safety-net timer above is what frees us if a client hangs on
    // its end of the connection.
    await closeServer();
    try {
      await engineHandle.browser.close();
      log('browser closed');
    } catch (closeErr) {
      log(`browser close error: ${errMessage(closeErr)}`);
    }
    process.exit(0);
  };
  process.on('SIGINT', () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
}

if (require.main === module) {
  const opts = parseServerArgs(process.argv);
  startServer(opts).catch((err) => {
    console.error(err && err.stack ? err.stack : err);
    process.exit(1);
  });
}

module.exports = { startServer, parseServerArgs };
