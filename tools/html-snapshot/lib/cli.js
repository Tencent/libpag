// CLI argument parsing for html-snapshot. Exports a `parseArgs(argv)` that
// mirrors `process.argv`-shaped input (so callers can pass `process.argv`
// directly and we slice past the `node` + script entries internally) and a
// `printUsage()` that writes the help text to stdout.

'use strict';

const path = require('path');

const LOG_PREFIX = 'html-snapshot: ';

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
  const failFn = (opts && opts.fail) || fail;
  if (value === undefined) {
    failFn(`'${flagName}' requires a numeric argument`);
  }
  const n = Number(value);
  if (!Number.isFinite(n) || n < min) {
    const bound = min > 0 ? `>= ${min}` : 'non-negative';
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

// Parse `name=value` into a puppeteer-compatible cookie descriptor. The URL
// must be filled in by the caller (we do that once we know the page URL) so
// the cookie is scoped to the right origin.
function parseCookie(flagName, value) {
  if (value === undefined) {
    fail(`'${flagName}' requires a 'name=value' argument`);
  }
  const eq = value.indexOf('=');
  if (eq <= 0) {
    fail(`'${flagName}' expects 'name=value', got '${value}'`);
  }
  return { name: value.slice(0, eq), value: value.slice(eq + 1) };
}

// Parse `Key: Value` (or `Key:Value`) into a [key, value] pair. The colon
// after the key is the separator; any further colons are part of the value
// (so `Authorization: Bearer xyz:abc` works as expected).
function parseHeader(flagName, value) {
  if (value === undefined) {
    fail(`'${flagName}' requires a 'Key: Value' argument`);
  }
  const colon = value.indexOf(':');
  if (colon <= 0) {
    fail(`'${flagName}' expects 'Key: Value', got '${value}'`);
  }
  const key = value.slice(0, colon).trim();
  const val = value.slice(colon + 1).trim();
  if (!key) {
    fail(`'${flagName}' has empty header name in '${value}'`);
  }
  return [key, val];
}

// Long-option flag table. Each entry lists the names the user can type and a
// setter that mutates `opts` with the next argv token. `-h` / `--help` and
// positional handling stay inline in `parseArgs` since they affect control
// flow rather than just options.
// Boolean toggles that don't consume the next argv token. `takesArg: false`
// distinguishes them from the value-bearing flags above so `parseArgs` knows
// not to shift the index after the flag is matched. The helper here keeps
// the table declarative.
const FLAGS = [
  { names: ['-o', '--output'],     set: (o, v) => { o.output = v; } },
  { names: ['--viewport-width'],   set: (o, v) => { o.viewportWidth = parseNumber('--viewport-width', v, { min: 1 }); } },
  { names: ['--viewport-height'],  set: (o, v) => { o.viewportHeight = parseNumber('--viewport-height', v, { min: 1 }); } },
  { names: ['--wait-ms'],          set: (o, v) => { o.waitMs = parseNumber('--wait-ms', v, { min: 0 }); } },
  { names: ['--selector'],         set: (o, v) => { o.selector = v; } },
  { names: ['--cookie'],           set: (o, v) => { o.cookies.push(parseCookie('--cookie', v)); } },
  { names: ['--header'],           set: (o, v) => { o.headers.push(parseHeader('--header', v)); } },
  // Disable the inline-icon-font pre-pass (default: enabled). When the pass
  // is on, every PUA `::before` glyph whose font is registered via
  // `@font-face` is replaced with a vector `<svg>` extracted from the font
  // file, so the resulting PAGX no longer depends on the icon font being
  // installed on the rendering machine. Disable to fall back to the
  // legacy font-named span path (faster, no font fetch, but the PAGX file
  // becomes non-portable).
  { names: ['--no-inline-icon-fonts'], takesArg: false, set: (o) => { o.inlineIconFonts = false; } },
];

const FLAG_BY_NAME = new Map();
for (const flag of FLAGS) {
  for (const name of flag.names) FLAG_BY_NAME.set(name, flag);
}

function parseArgs(argv) {
  const opts = {
    input: '',
    output: '',
    // When true, the HTML is written to process.stdout and the
    // "wrote ..." log line is routed to process.stderr so the two
    // streams don't get mixed. Triggered by `-o -`. Used by pagx's
    // `--html-snapshot` integration in src/cli/CommandImport.cpp so
    // the importer can read the snapshot output through a pipe
    // instead of a temp file on disk.
    outputToStdout: false,
    isUrl: false,
    viewportWidth: 1400,
    viewportHeight: 900,
    waitMs: 800,
    selector: '',
    cookies: [],
    headers: [],
    // Inline-icon-font pre-pass: convert every PUA `::before` glyph backed
    // by a webfont (Phosphor, Material Icons, Font Awesome, Lucide, …) to
    // an inline `<svg>` so the snapshot — and the PAGX downstream — no
    // longer depends on the icon font being available at render time.
    // See lib/icon-font.js for the pipeline; toggle via
    // `--no-inline-icon-fonts`.
    inlineIconFonts: true,
  };
  const positional = [];
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') {
      printUsage();
      process.exit(0);
    }
    const flag = FLAG_BY_NAME.get(a);
    if (flag) {
      // Boolean flags (`takesArg: false`) don't consume the next argv
      // token; value-bearing flags shift the index past their argument.
      if (flag.takesArg === false) {
        flag.set(opts);
      } else {
        flag.set(opts, argv[++i]);
      }
      continue;
    }
    if (a.startsWith('-')) {
      fail(`unknown option '${a}'`);
    }
    positional.push(a);
  }
  if (positional.length === 0) {
    printUsage();
    process.exit(2);
  }
  // `-o -` is the sentinel for "write HTML to stdout"; detect it here so the
  // URL/file branches below skip both their own default-output logic and the
  // path.resolve() call (which would turn `-` into an absolute path).
  if (opts.output === '-') {
    opts.outputToStdout = true;
    opts.output = '';
  }
  const inputArg = positional[0];
  opts.isUrl = isHttpUrl(inputArg);
  if (opts.isUrl) {
    // Remote pages have no filesystem location to derive a sibling name from,
    // so we require an explicit -o (or `-o -` for stdout). Letting it default
    // to a magic name in cwd would silently overwrite a previous run.
    opts.input = inputArg;
    if (!opts.output && !opts.outputToStdout) {
      fail(`-o/--output is required when input is a URL`);
    }
    if (opts.output) opts.output = path.resolve(opts.output);
  } else {
    if (opts.cookies.length || opts.headers.length) {
      fail(`--cookie / --header are only supported with URL inputs`);
    }
    opts.input = path.resolve(inputArg);
    if (opts.outputToStdout) {
      // No output file to derive; the script writes to process.stdout.
    } else if (!opts.output) {
      const dir = path.dirname(opts.input);
      const base = path.basename(opts.input, path.extname(opts.input));
      opts.output = path.join(dir, `${base}.subset.html`);
    } else {
      opts.output = path.resolve(opts.output);
    }
  }
  return opts;
}

function printUsage() {
  console.log(`Usage: html-snapshot <input.html | http(s)://url> -o <out.html> [options]

Render <input> in a headless browser and emit a flat, absolute-positioned
snapshot suitable for 'pagx import --format html'. <input> may be a local
HTML file or an http(s) URL; -o is required for URL inputs.

Options:
  -o, --output <file|->      Output path; use '-' to write the HTML to stdout
                             (required for URL inputs;
                             defaults to <input>.subset.html for files)
  --viewport-width <px>      Headless viewport width (default 1400)
  --viewport-height <px>     Headless viewport height (default 900)
  --wait-ms <ms>             Extra settle delay after networkidle (default 800)
  --selector <css>           Wait for this selector before snapshotting
  --cookie <name=value>      Set a cookie on the target URL (URL inputs only;
                             repeatable)
  --header <Key: Value>      Set an extra HTTP request header (URL inputs only;
                             repeatable)
  --no-inline-icon-fonts     Disable converting webfont icon glyphs (Phosphor,
                             Material Icons, Font Awesome, Lucide, …) to
                             inline <svg>. Default: enabled (the snapshot is
                             self-contained and renders identically on any
                             machine, regardless of which icon fonts are
                             installed).`);
}

module.exports = { parseArgs, printUsage, isHttpUrl, fail, makeFail, parseNumber, errMessage, LOG_PREFIX };
