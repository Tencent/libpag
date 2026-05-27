#!/usr/bin/env node
/**
 * html-snapshot HTTP service.
 *
 * Wraps the snapshot pipeline (the same one `snapshot.js` runs from the CLI)
 * behind a tiny HTTP server. Send an HTML string in the request body; receive
 * the processed, flat, absolute-positioned HTML back — the same output that
 * `node snapshot.js page.html` writes to disk and feeds to
 * `pagx import --format html`.
 *
 * Lifecycle:
 *   • One headless browser is launched at startup and kept alive for the
 *     lifetime of the process. Each request opens a fresh page, runs the
 *     pipeline, and closes the page — no temp tabs accumulate even under
 *     sustained load. Concurrent requests are served in parallel against
 *     the same browser.
 *
 * Endpoints:
 *   POST /snapshot
 *     Request body:
 *       - Content-Type: application/json  → { html | url: string, options?: { ... } }
 *                                            Provide exactly one of `html` or `url`.
 *       - Any other Content-Type           → the entire body is treated as the
 *                                            HTML payload.
 *     Response:
 *       - Default                          → text/html with the processed HTML
 *                                            (plus X-Snapshot-Width /
 *                                            X-Snapshot-Height headers).
 *       - Accept: application/json         → { html, width, height }.
 *     The `options` object accepts: viewportWidth, viewportHeight, waitMs,
 *     selector, inlineIconFonts, plus cookies / headers for URL inputs
 *     (all optional).
 *
 *   GET /snapshot?url=<http(s)-url>&...options
 *     Convenience route for "fetch this page and snapshot it" — no body
 *     required. Same response semantics as POST. Supported query params:
 *       url (required), viewportWidth, viewportHeight, waitMs, selector,
 *       inlineIconFonts.
 *
 *   GET /health
 *     200 { status: "ok", engine, activeRequests } once the browser is up.
 *
 * Usage:
 *   node server.js [--port 8787] [--host 127.0.0.1]
 *                  [--browser-engine puppeteer|playwright]
 *                  [--max-body-mb 32]
 */

'use strict';

const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');

const { launchBrowser, resolveEngine, SUPPORTED_ENGINES } = require('./lib/browser-engine');
const { runSnapshot } = require('./lib/snapshot-runner');
const { errMessage, isHttpUrl } = require('./lib/cli');

const LOG_PREFIX = 'html-snapshot-server: ';

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
  --max-body-mb <n>        Maximum request body size in MB (default 32;
                           env: MAX_BODY_MB)
  -h, --help               Show this help

Endpoints:
  POST /snapshot         — body is raw HTML, or JSON { html|url, options };
                           response is the processed HTML string (text/html),
                           or JSON when the client sends Accept: application/json.
  GET  /snapshot?url=…   — fetch the URL and run the same pipeline (no body).
                           Optional query params: viewportWidth, viewportHeight,
                           waitMs, selector, inlineIconFonts.
  GET  /health           — liveness probe.`);
}

function parseServerArgs(argv) {
  const opts = {
    port: Number(process.env.PORT) || 8787,
    host: process.env.HOST || '127.0.0.1',
    browserEngine: resolveEngine(process.env.HTML_SNAPSHOT_BROWSER),
    maxBodyBytes: (Number(process.env.MAX_BODY_MB) || 32) * 1024 * 1024,
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') {
      printUsage();
      process.exit(0);
    } else if (a === '--port') {
      opts.port = Number(argv[++i]);
      if (!Number.isFinite(opts.port) || opts.port <= 0 || opts.port > 65535) {
        console.error(`${LOG_PREFIX}invalid --port value`);
        process.exit(2);
      }
    } else if (a === '--host') {
      opts.host = argv[++i];
    } else if (a === '--browser-engine') {
      try {
        opts.browserEngine = resolveEngine(argv[++i]);
      } catch (err) {
        console.error(`${LOG_PREFIX}${errMessage(err)}`);
        process.exit(2);
      }
    } else if (a === '--max-body-mb') {
      const mb = Number(argv[++i]);
      if (!Number.isFinite(mb) || mb <= 0) {
        console.error(`${LOG_PREFIX}--max-body-mb must be a positive number`);
        process.exit(2);
      }
      opts.maxBodyBytes = mb * 1024 * 1024;
    } else {
      console.error(`${LOG_PREFIX}unknown option: ${a}`);
      printUsage();
      process.exit(2);
    }
  }
  return opts;
}

// Tagged error so the request handler can map a single throw into the
// right HTTP status + message without losing the cause chain.
class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
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

function wantsJsonResponse(req) {
  const accept = (req.headers['accept'] || '').toLowerCase();
  if (!accept) return false;
  // Prefer JSON only when the client explicitly asks for it and isn't also
  // listing text/html with equal or higher priority. The lightweight check
  // here covers the common cases (`Accept: application/json`,
  // `Accept: text/html, application/json;q=0.9`) without pulling in a full
  // RFC 7231 matcher.
  if (!accept.includes('application/json')) return false;
  if (accept.includes('text/html')) {
    // Both listed — defer to text/html so browsers that send the standard
    // `text/html, application/xhtml+xml, ..., */*` still get HTML.
    return false;
  }
  return true;
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

// Parse a POST body into a `{ html | url, options }` source descriptor.
// JSON bodies may carry either an `html` string or a `url` string (exactly
// one); any other Content-Type is treated as a raw HTML payload — that
// shorthand only makes sense for HTML so we don't accept URLs through it.
function parsePostSource(req, rawBuf) {
  const ct = (req.headers['content-type'] || '').split(';')[0].trim().toLowerCase();
  const rawText = rawBuf.toString('utf8');
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
    if (hasUrl) {
      if (!isHttpUrl(parsed.url)) {
        throw new HttpError(400, 'url must start with http:// or https://');
      }
      return { url: parsed.url, options };
    }
    if (!parsed.html.trim()) {
      throw new HttpError(400, 'empty "html" field');
    }
    return { html: parsed.html, options };
  }
  if (!rawText || !rawText.trim()) {
    throw new HttpError(400, 'empty html body');
  }
  return { html: rawText, options: {} };
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
  const queryOptions = {};
  const vw = params.get('viewportWidth');
  if (vw !== null) queryOptions.viewportWidth = Number(vw);
  const vh = params.get('viewportHeight');
  if (vh !== null) queryOptions.viewportHeight = Number(vh);
  const wm = params.get('waitMs');
  if (wm !== null) queryOptions.waitMs = Number(wm);
  const sel = params.get('selector');
  if (sel !== null) queryOptions.selector = sel;
  const iif = params.get('inlineIconFonts');
  if (iif !== null) {
    if (iif === 'true' || iif === '1') queryOptions.inlineIconFonts = true;
    else if (iif === 'false' || iif === '0') queryOptions.inlineIconFonts = false;
    else throw new HttpError(400, `inlineIconFonts must be true|false|1|0, got '${iif}'`);
  }
  return { url, options: queryOptions };
}

// Whitelist + coerce the small set of pipeline options we expose over HTTP.
// Anything else in `reqOptions` is silently dropped — this matches the CLI
// (which only forwards documented flags) and prevents callers from
// accidentally toggling internal knobs (e.g. browser engine, init scripts).
//
// `cookies` and `headers` are only honoured for URL-source requests; the
// snapshot pipeline ignores them for file:// URLs and forwarding them
// would only confuse the caller. The flag is plumbed via `forUrl`.
function resolveOptions(reqOptions, forUrl) {
  const out = {};
  const r = reqOptions || {};
  if (Number.isFinite(r.viewportWidth) && r.viewportWidth > 0) out.viewportWidth = r.viewportWidth;
  if (Number.isFinite(r.viewportHeight) && r.viewportHeight > 0) out.viewportHeight = r.viewportHeight;
  if (Number.isFinite(r.waitMs) && r.waitMs >= 0) out.waitMs = r.waitMs;
  if (typeof r.selector === 'string') out.selector = r.selector;
  if (typeof r.inlineIconFonts === 'boolean') out.inlineIconFonts = r.inlineIconFonts;
  if (forUrl) {
    if (Array.isArray(r.cookies)) {
      // Expected shape: [{ name, value }]. Drop anything that doesn't
      // fit the contract rather than letting puppeteer throw a
      // less-helpful error mid-pipeline.
      const valid = r.cookies.filter(
        (c) => c && typeof c.name === 'string' && typeof c.value === 'string',
      );
      if (valid.length > 0) out.cookies = valid;
    }
    if (Array.isArray(r.headers)) {
      // Array shape: [[key, value], …] matches the CLI parser output.
      const valid = r.headers.filter(
        (h) => Array.isArray(h) && h.length === 2
          && typeof h[0] === 'string' && typeof h[1] === 'string',
      );
      if (valid.length > 0) out.headers = valid;
    } else if (r.headers && typeof r.headers === 'object') {
      // Object shape: { "X-User": "alice" } — convenient for JSON
      // callers; converted to the same `[[k, v]]` form internally.
      const entries = Object.entries(r.headers).filter(
        ([k, v]) => typeof k === 'string' && typeof v === 'string',
      );
      if (entries.length > 0) out.headers = entries;
    }
  }
  return out;
}

// Common pipeline path for both POST and GET. `source` is the descriptor
// returned by parsePostSource / parseGetSource; it carries exactly one of
// `html` or `url`. We resolve options, set up a temp file when needed,
// run the snapshot, and shape the response — all wrapped in a single
// try/finally so the temp dir always gets cleaned up.
async function handleSnapshot(req, res, ctx, source) {
  let tmp = null;
  try {
    const isUrlSource = typeof source.url === 'string';
    const runOpts = resolveOptions(source.options, isUrlSource);
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
    log(
      `${req.method} /snapshot ok ${result.width}x${result.height} `
      + `${inputDesc} out=${result.html.length}B ${Date.now() - source.startedAt}ms`,
    );
    if (wantsJsonResponse(req)) {
      jsonResponse(res, 200, result);
    } else {
      res.statusCode = 200;
      res.setHeader('Content-Type', 'text/html; charset=utf-8');
      res.setHeader('X-Snapshot-Width', String(result.width));
      res.setHeader('X-Snapshot-Height', String(result.height));
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

async function startServer(opts) {
  log(`launching ${opts.browserEngine}…`);
  const engineHandle = await launchBrowser({ engine: opts.browserEngine });
  log(`browser ready (engine=${engineHandle.engine})`);

  const ctx = { engineHandle, opts, activeRequests: 0 };

  const server = http.createServer((req, res) => {
    // Strip query string for routing so future parameters can land on the
    // URL without breaking the route match.
    const urlPath = (req.url || '').split('?')[0];

    if (req.method === 'GET' && urlPath === '/health') {
      jsonResponse(res, 200, {
        status: 'ok',
        engine: engineHandle.engine,
        activeRequests: ctx.activeRequests,
      });
      return;
    }

    // Both /snapshot routes funnel through runRoute, which owns the
    // try/catch and 4xx/5xx mapping. Fire-and-forget — the handler manages
    // its own response. The outer `.catch` only fires for synchronous
    // throws outside runRoute's try block; in practice it should never
    // run, but it keeps an unexpected throw from killing the process.
    const onUnexpected = (err) => {
      log(`unhandled handler error: ${errMessage(err)}`);
      if (!res.headersSent) {
        jsonError(res, 500, errMessage(err));
      } else {
        res.destroy();
      }
    };

    if (req.method === 'POST' && urlPath === '/snapshot') {
      runRoute(req, res, ctx, async () => {
        const raw = await readBody(req, ctx.opts.maxBodyBytes);
        return parsePostSource(req, raw);
      }).catch(onUnexpected);
      return;
    }

    if (req.method === 'GET' && urlPath === '/snapshot') {
      // GET takes no body — the source is fully derived from the query
      // string. Wrapped in an async factory so parse errors hit
      // runRoute's error mapper instead of becoming a synchronous throw.
      runRoute(req, res, ctx, async () => parseGetSource(req)).catch(onUnexpected);
      return;
    }

    jsonError(res, 404, `not found: ${req.method} ${urlPath}`);
  });

  server.listen(opts.port, opts.host, () => {
    log(`listening on http://${opts.host}:${opts.port}`);
  });

  // Coordinated shutdown: stop accepting new connections, then close the
  // browser. Long-running snapshots in flight get a few seconds to finish
  // before we exit; after that, the process exits forcefully so a stuck
  // page can't pin the service open forever.
  let shuttingDown = false;
  const shutdown = async (signal) => {
    if (shuttingDown) return;
    shuttingDown = true;
    log(`received ${signal}, shutting down…`);
    server.close((err) => {
      if (err) log(`http server close error: ${errMessage(err)}`);
      else log('http server closed');
    });
    const forceExitTimer = setTimeout(() => {
      log('forced exit after 10s shutdown timeout');
      process.exit(1);
    }, 10000);
    forceExitTimer.unref();
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
