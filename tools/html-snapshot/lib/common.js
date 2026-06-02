// Shared, dependency-free utilities used across the html-snapshot codebase.
//
// Why this file exists: before the split, `lib/cli.js` doubled as the home
// for these helpers, which forced every low-level module that needed
// `errMessage`, `isHttpUrl`, or `SNAPSHOT_DEFAULTS` (capture-listener,
// font-download, image-download, icon-font, pipeline, snapshot-runner, …) to
// `require('./cli')`. That coupled non-CLI code to the CLI argument parser
// and pulled `lib/browser-engine.js` along as a transitive load. Moving the
// generic helpers out makes `cli.js` purely a CLI front-end and lets every
// other module depend on a leaf utility module that has no project-internal
// imports of its own.
//
// What lives here: only helpers that are truly generic and have zero
// project-internal dependencies. CLI-only helpers (FLAGS, parseCookie,
// parseHeader, parseArgs, printUsage) stay in `lib/cli.js`; HTTP-only
// validators (validateCookies, validateHeaders) live in `lib/http-utils.js`.

'use strict';

// Standard prefix used when html-snapshot logs to stderr. Lives here so the
// CLI driver, the in-process snapshot runner, and any sibling tool that
// wants to mimic the same UX print under the same banner.
const LOG_PREFIX = 'html-snapshot: ';

// Default values for the user-facing snapshot options. Defined here so the
// CLI parser, the HTTP service (server.js), and the in-process runner
// (snapshot-runner.js) read the numbers from a single source — drift between
// them used to mean "the same request produced different output depending on
// which entry point you used".
const SNAPSHOT_DEFAULTS = Object.freeze({
  viewportWidth: 1400,
  viewportHeight: 900,
  waitMs: 800,
});

// Print an error message with the standard CLI prefix and abort with the
// "argument error" exit code (2). Centralised so every fail point shares the
// same prefix and exit semantics — the prior inline `console.error` + `exit`
// pairs were close to identical and easy to drift.
function fail(msg) {
  console.error(`${LOG_PREFIX}${msg}`);
  process.exit(2);
}

// Build a `fail`-style helper that prefixes log messages with `prefix:` and
// exits with the given code (default 2). Used by sibling tools (eval/run.js,
// eval/baseline.js) that share the same arg-error UX but want their own log
// prefix so users can tell which tool reported the failure. The returned
// function never returns, so callers can use it in expression position
// without `return fail(...)` boilerplate.
function makeFail(prefix) {
  return function failWithPrefix(msg, code) {
    console.error(`${prefix}: ${msg}`);
    process.exit(code === undefined ? 2 : code);
  };
}

// Extract a one-line message from any thrown value. Centralised because the
// `err && err.message ? err.message : err` pattern was repeated at every
// catch site that wanted to log a reason without a stack trace. Treats
// non-Error throws (strings, plain objects, undefined) gracefully — falls
// back to `String(err)` so `undefined` becomes `'undefined'` rather than
// an empty trailing colon in the log.
function errMessage(err) {
  if (err && typeof err.message === 'string' && err.message) return err.message;
  return String(err);
}

// Convert a flag's argument to a number, validating it survives parsing and
// is at or above `min`. Without this, `--viewport-width foo` would silently
// send NaN to puppeteer's setViewport. Used for both strictly-positive
// (viewport dimensions) and non-negative (wait-ms) numeric flags. `opts.fail`
// optionally overrides the default `html-snapshot:`-prefixed reporter so
// sibling tools can keep their own log prefix.
function parseNumber(flagName, value, opts) {
  const min = (opts && Object.prototype.hasOwnProperty.call(opts, 'min')) ? opts.min : 0;
  const max = (opts && Object.prototype.hasOwnProperty.call(opts, 'max')) ? opts.max : Infinity;
  const failFn = (opts && opts.fail) || fail;
  if (value === undefined) {
    failFn(`'${flagName}' requires a numeric argument`);
  }
  const n = Number(value);
  if (!Number.isFinite(n) || n < min || n > max) {
    let bound;
    if (max !== Infinity) bound = `between ${min} and ${max}`;
    else if (min > 0) bound = `>= ${min}`;
    else bound = 'non-negative';
    failFn(`'${flagName}' expects a ${bound} number, got '${value}'`);
  }
  return n;
}

// Match `http://` and `https://` only. We deliberately reject other schemes
// (file:, data:, ftp:, ...) so that a typo like `htttp://...` or a stray local
// path with a colon never silently hits the network.
function isHttpUrl(s) {
  return /^https?:\/\//i.test(s);
}

module.exports = {
  LOG_PREFIX,
  SNAPSHOT_DEFAULTS,
  fail,
  makeFail,
  errMessage,
  parseNumber,
  isHttpUrl,
};
