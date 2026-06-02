// Shared, dependency-free utilities used across the html-snapshot codebase.
//
// Why this file exists: before the split, `lib/cli.ts` doubled as the home
// for these helpers, which forced every low-level module that needed
// `errMessage`, `isHttpUrl`, or `SNAPSHOT_DEFAULTS` (capture-listener,
// font-download, image-download, icon-font, pipeline, snapshot-runner, …) to
// `require('./cli')`. That coupled non-CLI code to the CLI argument parser
// and pulled `lib/browser-engine.ts` along as a transitive load. Moving the
// generic helpers out makes `cli.ts` purely a CLI front-end and lets every
// other module depend on a leaf utility module that has no project-internal
// imports of its own.
//
// What lives here: only helpers that are truly generic and have zero
// project-internal dependencies. CLI-only helpers (FLAGS, parseCookie,
// parseHeader, parseArgs, printUsage) stay in `lib/cli.ts`; HTTP-only
// validators (validateCookies, validateHeaders) live in `lib/http-utils.ts`.

// Standard prefix used when html-snapshot logs to stderr. Lives here so the
// CLI driver, the in-process snapshot runner, and any sibling tool that
// wants to mimic the same UX print under the same banner.
export const LOG_PREFIX = 'html-snapshot: ';

export interface SnapshotDefaults {
  readonly viewportWidth: number;
  readonly viewportHeight: number;
  readonly waitMs: number;
}

// Default values for the user-facing snapshot options. Defined here so the
// CLI parser, the HTTP service (server.js), and the in-process runner
// (snapshot-runner.ts) read the numbers from a single source — drift between
// them used to mean "the same request produced different output depending on
// which entry point you used".
export const SNAPSHOT_DEFAULTS: SnapshotDefaults = Object.freeze({
  viewportWidth: 1400,
  viewportHeight: 900,
  waitMs: 800,
});

export type FailFn = (msg: string, code?: number) => never;

// Print an error message with the standard CLI prefix and abort with the
// "argument error" exit code (2). Centralised so every fail point shares the
// same prefix and exit semantics — the prior inline `console.error` + `exit`
// pairs were close to identical and easy to drift.
export function fail(msg: string): never {
  console.error(`${LOG_PREFIX}${msg}`);
  process.exit(2);
}

// Build a `fail`-style helper that prefixes log messages with `prefix:` and
// exits with the given code (default 2). Used by sibling tools (eval/run.js,
// eval/baseline.js) that share the same arg-error UX but want their own log
// prefix so users can tell which tool reported the failure. The returned
// function never returns, so callers can use it in expression position
// without `return fail(...)` boilerplate.
export function makeFail(prefix: string): FailFn {
  return function failWithPrefix(msg: string, code?: number): never {
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
export function errMessage(err: unknown): string {
  if (err && typeof (err as { message?: unknown }).message === 'string'
      && (err as { message: string }).message) {
    return (err as { message: string }).message;
  }
  return String(err);
}

export interface ParseNumberOptions {
  min?: number;
  max?: number;
  fail?: FailFn;
}

// Convert a flag's argument to a number, validating it survives parsing and
// is at or above `min`. Without this, `--viewport-width foo` would silently
// send NaN to puppeteer's setViewport. Used for both strictly-positive
// (viewport dimensions) and non-negative (wait-ms) numeric flags. `opts.fail`
// optionally overrides the default `html-snapshot:`-prefixed reporter so
// sibling tools can keep their own log prefix.
export function parseNumber(
  flagName: string,
  value: string | undefined,
  opts?: ParseNumberOptions,
): number {
  const min = (opts && Object.prototype.hasOwnProperty.call(opts, 'min')) ? (opts.min as number) : 0;
  const max = (opts && Object.prototype.hasOwnProperty.call(opts, 'max')) ? (opts.max as number) : Infinity;
  const failFn: FailFn = (opts && opts.fail) || fail;
  if (value === undefined) {
    failFn(`'${flagName}' requires a numeric argument`);
  }
  const n = Number(value);
  if (!Number.isFinite(n) || n < min || n > max) {
    let bound: string;
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
export function isHttpUrl(s: string): boolean {
  return /^https?:\/\//i.test(s);
}
