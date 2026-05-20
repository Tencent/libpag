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
];

const FLAG_BY_NAME = new Map();
for (const flag of FLAGS) {
  for (const name of flag.names) FLAG_BY_NAME.set(name, flag);
}

function parseArgs(argv) {
  const opts = {
    input: '',
    output: '',
    viewportWidth: 1400,
    viewportHeight: 900,
    waitMs: 800,
    selector: '',
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
  opts.input = path.resolve(positional[0]);
  if (!opts.output) {
    const dir = path.dirname(opts.input);
    const base = path.basename(opts.input, path.extname(opts.input));
    opts.output = path.join(dir, `${base}.subset.html`);
  } else {
    opts.output = path.resolve(opts.output);
  }
  return opts;
}

function printUsage() {
  console.log(`Usage: html-snapshot <input.html> [-o <out.html>] [options]

Render <input.html> in a headless browser and emit a flat, absolute-positioned
snapshot suitable for 'pagx import --format html'.

Options:
  -o, --output <file>        Output path (default: <input>.subset.html)
  --viewport-width <px>      Headless viewport width (default 1400)
  --viewport-height <px>     Headless viewport height (default 900)
  --wait-ms <ms>             Extra settle delay after networkidle (default 800)
  --selector <css>           Wait for this selector before snapshotting`);
}

module.exports = { parseArgs, printUsage };
