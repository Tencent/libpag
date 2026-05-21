// CLI argument parsing for html-snapshot. Exports a `parseArgs(argv)` that
// mirrors `process.argv`-shaped input (so callers can pass `process.argv`
// directly and we slice past the `node` + script entries internally) and a
// `printUsage()` that writes the help text to stdout.

'use strict';

const path = require('path');

// Convert a flag's argument to a positive number; abort with a clear error
// message when the value is missing, NaN, or non-positive. Without this,
// `--viewport-width foo` would silently send NaN to puppeteer's setViewport.
function parsePositiveNumber(flagName, value) {
  if (value === undefined) {
    console.error(`html-snapshot: '${flagName}' requires a numeric argument`);
    process.exit(2);
  }
  const n = Number(value);
  if (!Number.isFinite(n) || n <= 0) {
    console.error(`html-snapshot: '${flagName}' expects a positive number, got '${value}'`);
    process.exit(2);
  }
  return n;
}

function parseNonNegativeNumber(flagName, value) {
  if (value === undefined) {
    console.error(`html-snapshot: '${flagName}' requires a numeric argument`);
    process.exit(2);
  }
  const n = Number(value);
  if (!Number.isFinite(n) || n < 0) {
    console.error(`html-snapshot: '${flagName}' expects a non-negative number, got '${value}'`);
    process.exit(2);
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
    console.error(`html-snapshot: '${flagName}' requires a 'name=value' argument`);
    process.exit(2);
  }
  const eq = value.indexOf('=');
  if (eq <= 0) {
    console.error(`html-snapshot: '${flagName}' expects 'name=value', got '${value}'`);
    process.exit(2);
  }
  return { name: value.slice(0, eq), value: value.slice(eq + 1) };
}

// Parse `Key: Value` (or `Key:Value`) into a [key, value] pair. The colon
// after the key is the separator; any further colons are part of the value
// (so `Authorization: Bearer xyz:abc` works as expected).
function parseHeader(flagName, value) {
  if (value === undefined) {
    console.error(`html-snapshot: '${flagName}' requires a 'Key: Value' argument`);
    process.exit(2);
  }
  const colon = value.indexOf(':');
  if (colon <= 0) {
    console.error(`html-snapshot: '${flagName}' expects 'Key: Value', got '${value}'`);
    process.exit(2);
  }
  const key = value.slice(0, colon).trim();
  const val = value.slice(colon + 1).trim();
  if (!key) {
    console.error(`html-snapshot: '${flagName}' has empty header name in '${value}'`);
    process.exit(2);
  }
  return [key, val];
}

// Long-option flag table. Each entry lists the names the user can type and a
// setter that mutates `opts` with the next argv token. `-h` / `--help` and
// positional handling stay inline in `parseArgs` since they affect control
// flow rather than just options.
const FLAGS = [
  { names: ['-o', '--output'],     set: (o, v) => { o.output = v; } },
  { names: ['--viewport-width'],   set: (o, v) => { o.viewportWidth = parsePositiveNumber('--viewport-width', v); } },
  { names: ['--viewport-height'],  set: (o, v) => { o.viewportHeight = parsePositiveNumber('--viewport-height', v); } },
  { names: ['--wait-ms'],          set: (o, v) => { o.waitMs = parseNonNegativeNumber('--wait-ms', v); } },
  { names: ['--selector'],         set: (o, v) => { o.selector = v; } },
  { names: ['--cookie'],           set: (o, v) => { o.cookies.push(parseCookie('--cookie', v)); } },
  { names: ['--header'],           set: (o, v) => { o.headers.push(parseHeader('--header', v)); } },
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
      flag.set(opts, argv[++i]);
      continue;
    }
    if (a.startsWith('-')) {
      console.error(`html-snapshot: unknown option '${a}'`);
      process.exit(2);
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
      console.error(`html-snapshot: -o/--output is required when input is a URL`);
      process.exit(2);
    }
    if (opts.output) opts.output = path.resolve(opts.output);
  } else {
    if (opts.cookies.length || opts.headers.length) {
      console.error(`html-snapshot: --cookie / --header are only supported with URL inputs`);
      process.exit(2);
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
                             repeatable)`);
}

module.exports = { parseArgs, printUsage, isHttpUrl };
