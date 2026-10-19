// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';

// Shared frontend for the repository's html2pagx and the native pagx capture bridge.
// Parsing/validation never installs or starts a browser. The bundled launcher also
// uses these functions before preparing its lazily installed browser runtime.
const fs = require('fs');
const path = require('path');
const { defaultPagxBin, runHtmlToPagx, isHttpUrl } = require('./dist/lib/pipeline');
const { runBatch } = require('./dist/lib/batch');
const { runScenarioToPagx } = require('./dist/lib/scenario-pipeline');
const { validateContactSheetColumns } = require('./dist/lib/contact-sheet');
const { SUPPORTED_ENGINES, DEFAULT_ENGINE, checkBrowserRuntime } = require('./dist/lib/browser-engine');
const { parseNumber } = require('./dist/lib/common');
const { parseCookie, parseHeader } = require('./dist/lib/cli');
const { parseScenario, selectCheckpoint } = require('./dist/lib/interaction');
const { withCliLifecycle } = require('./dist/lib/cli-lifecycle');

class CaptureCliError extends Error {
  constructor(message, code = 2) { super(message); this.code = code; }
}
function failArg(message) { throw new CaptureCliError(message); }

const USAGE = `html2pagx — one-shot pipeline for converting a JS/React-driven HTML page into a
rendered PAGX. Wraps four steps:

  1. snapshot.js     <input>.html        → <input>.subset.html
  2. pagx import     <input>.subset.html → <input>.pagx
  3. pagx resolve    <input>.pagx
  4. pagx render     <input>.pagx        → <input>.png

Usage:
  html2pagx <input.html | http(s)://url> [options]
  html2pagx --batch <input-dir> [options]

Options (unknown options are errors):
  --input <file|url>        Alternative to the positional input
  -o, --output-dir <dir>     Write outputs to <dir> (default: same dir as input)
  --output-name <name>       Base name for outputs (required when input is a URL,
                             rejected in --batch mode)
  --batch <dir>              Recursively scan <dir> for .html/.htm (excluding
                             *.subset.html / *.subset.htm) and run the pipeline
                             on each file with one shared headless browser
  --output-root <dir>        Mirror batch outputs under <dir> instead of next
                             to inputs (--batch only)
  --skip-existing            In --batch mode, skip files whose target .pagx
                             already exists
  --dry-run                  In --batch mode, list the files that would be
                             processed without launching a browser or pagx
  --pagx-bin <path>          Path to the pagx CLI (default: $PAGX_BIN or
                             ../../cmake-build-debug/pagx relative to this script)
  --scale <N>                Render scale factor (default: 1)
  --no-render                Stop after pagx resolve; do not produce PNG
  --no-resolve               Stop after pagx import; do not run resolve/render
  --no-subset-html           Do not write <input>.subset.html (use a temp file
                             that is removed after import); default keeps it
  --no-inline-icon-fonts     Forwarded to snapshot.js: disable webfont-glyph
                             → inline SVG conversion (default: enabled)
  --capture-animations       Forwarded to snapshot.js: capture the page's
                             animations (CSS @keyframes, WAAPI, GSAP, anime.js)
                             into the subset so the .pagx replays the motion.
                             Default: disabled (a single static frame).
  --scenario <file.json>     Replay one interaction session; export <base>.<checkpoint>
                             files and <base>.captures.json for all capture steps
  --checkpoint <name>        With --scenario, export only the selected checkpoint
  --preview                  With --scenario, assemble rendered PNGs into
                             <base>.preview/sheet-NNN.png (default: disabled)
  --preview-columns <N>      With --preview, use up to N columns (1–32;
                             default: automatic, 1–6). Requires resolve/render.
  --scroll-reveal            Forwarded to snapshot.js: walk the page top-to-
                             bottom before snapshotting so scroll-triggered
                             reveal animations fire and lazy media loads
                             (default: disabled)
  --download-fonts           Download the web fonts the page uses and register
                             them as render fallbacks so text in uninstalled
                             web fonts renders correctly. The fonts are not
                             embedded into the .pagx. Default: disabled.
  --embed-fonts              On top of --download-fonts, embed the downloaded
                             faces into the .pagx (pagx embed) so the
                             document is self-contained and its glyph metrics
                             match the snapshot on any host. Implies
                             --download-fonts. Default: disabled.
  --font-dir <dir>           Where downloaded fonts are written
                             (default: <output>.fonts/)
  --download-images          Download the external images the page uses and
                             reference them by local file path instead of
                             inlining them as base64 data URIs. Keeps the
                             .pagx small for image-heavy pages; the images are
                             read from disk at render time. Default: disabled.
  --image-dir <dir>          Where downloaded images are written
                             (default: <output>.images/)
  --pagx-images <mode>       How image resources are stored in the .pagx:
                             'external' (default; write image files next to the
                             .pagx and keep their relative path) or 'embed'
                             (inline as base64 data URIs for a single self-
                             contained file)
  --cookie <name=value>      Forwarded to snapshot.js (URL inputs only; repeatable)
  --header <Key: Value>      Forwarded to snapshot.js (URL inputs only; repeatable)
  --browser-engine <name>    Headless browser driver: 'puppeteer' (default)
                             or 'playwright'. Forwarded to snapshot.js.
                             Honours $HTML_SNAPSHOT_BROWSER.
  -h, --help                 Show this help`;

function parseCaptureArgs(argv) {
  const opts = {
    input: '',
    outputDir: '',
    outputName: '',
    pagxBin: process.env.PAGX_BIN || defaultPagxBin(),
    scale: 1,
    doResolve: true,
    doRender: true,
    keepSubsetHtml: true,
    inlineIconFonts: true,
    captureAnimations: false,
    scenario: '',
    checkpoint: '',
    preview: false,
    previewColumns: undefined,
    scrollReveal: false,
    downloadFonts: false,
    embedFonts: false,
    fontDir: '',
    downloadImages: false,
    imageDir: '',
    pagxImages: 'external',
    cookies: [],
    headers: [],
    viewportWidth: undefined,
    viewportHeight: undefined,
    waitMs: undefined,
    selector: '',
    // Batch-mode flags. `batchDir` doubling as the mode switch keeps the
    // single-file CLI intact when the flag is absent.
    batchDir: '',
    outputRoot: '',
    skipExisting: false,
    dryRun: false,
    // Browser engine: defaults to puppeteer; --browser-engine wins over the
    // env var which wins over this default. Captured separately from the
    // flag pass-through so the preflight check below can read it.
    browserEngine: process.env.HTML_SNAPSHOT_BROWSER || DEFAULT_ENGINE,
  };
  const requireValue = (flag, value) => {
    if (value === undefined || value === '' || value.startsWith('--')) failArg(`'${flag}' requires a value`);
    return value;
  };
  // CLI-style numeric parser shared with snapshot.js / server.js: routes
  // failures through `failArg` so html2pagx prints its own usage hint
  // alongside the diagnostic, instead of cli.js's bare exit-code-2 fallback.
  const parseFlagNumber = (flag, value, opts) =>
    parseNumber(flag, value, { ...(opts || {}), fail: failArg });
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    switch (a) {
      case '-h':
      case '--help':
        return null;
      case '--input':
        if (opts.input) failArg('only one input may be specified');
        opts.input = requireValue(a, argv[++i]);
        break;
      case '-o':
      case '--output-dir':
        opts.outputDir = requireValue(a, argv[++i]);
        break;
      case '--output-name':
        opts.outputName = requireValue(a, argv[++i]);
        break;
      case '--pagx-bin':
        opts.pagxBin = requireValue(a, argv[++i]);
        break;
      case '--scale': {
        const raw = requireValue(a, argv[++i]);
        const v = Number(raw);
        // Allow fractional scales (0.5, 1.5, 2x), but reject 0/negatives — the
        // CLI parseNumber's `>= ${min}` template would print an unreadable
        // `>= 5e-324` here, so keep this check inline.
        if (!Number.isFinite(v) || v <= 0) failArg(`--scale expects a positive number, got '${raw}'`);
        opts.scale = v;
        break;
      }
      case '--no-render':
        opts.doRender = false;
        break;
      case '--no-resolve':
        opts.doResolve = false;
        opts.doRender = false;
        break;
      case '--no-subset-html':
        opts.keepSubsetHtml = false;
        break;
      case '--no-inline-icon-fonts':
        opts.inlineIconFonts = false;
        break;
      case '--capture-animations':
        opts.captureAnimations = true;
        break;
      case '--scenario':
        opts.scenario = requireValue(a, argv[++i]);
        break;
      case '--checkpoint':
        opts.checkpoint = requireValue(a, argv[++i]);
        break;
      case '--preview':
        opts.preview = true;
        break;
      case '--preview-columns':
        opts.previewColumns = Number(requireValue(a, argv[++i]));
        try { validateContactSheetColumns(opts.previewColumns); }
        catch (err) { failArg(err.message); }
        break;
      case '--scroll-reveal':
        opts.scrollReveal = true;
        break;
      case '--download-fonts':
        opts.downloadFonts = true;
        break;
      case '--embed-fonts':
        // Embedding the faces requires them to be downloaded first, so
        // --embed-fonts implies --download-fonts.
        opts.embedFonts = true;
        opts.downloadFonts = true;
        break;
      case '--font-dir':
        opts.fontDir = requireValue(a, argv[++i]);
        break;
      case '--download-images':
        opts.downloadImages = true;
        break;
      case '--image-dir':
        opts.imageDir = requireValue(a, argv[++i]);
        break;
      case '--pagx-images': {
        const mode = requireValue(a, argv[++i]);
        if (mode !== 'external' && mode !== 'embed') {
          failArg(`--pagx-images expects 'external' or 'embed', got '${mode}'`);
        }
        opts.pagxImages = mode;
        break;
      }
      case '--cookie':
        opts.cookies.push(requireValue(a, argv[++i]));
        break;
      case '--header':
        opts.headers.push(requireValue(a, argv[++i]));
        break;
      case '--viewport-width':
        opts.viewportWidth = parseFlagNumber(a, requireValue(a, argv[++i]), { min: 1 });
        break;
      case '--viewport-height':
        opts.viewportHeight = parseFlagNumber(a, requireValue(a, argv[++i]), { min: 1 });
        break;
      case '--wait-ms':
        opts.waitMs = parseFlagNumber(a, requireValue(a, argv[++i]), { min: 0 });
        break;
      case '--selector':
        opts.selector = requireValue(a, argv[++i]);
        break;
      case '--browser-engine':
        opts.browserEngine = requireValue(a, argv[++i]);
        break;
      case '--batch':
        opts.batchDir = requireValue(a, argv[++i]);
        break;
      case '--output-root':
        opts.outputRoot = requireValue(a, argv[++i]);
        break;
      case '--skip-existing':
        opts.skipExisting = true;
        break;
      case '--dry-run':
        opts.dryRun = true;
        break;
      case '--':
        // Everything past `--` would have been forwarded to snapshot.js by
        // the old bash wrapper. None of the supported flags are positional,
        // so in practice the marker just absorbs trailing args. Preserve
        // that behaviour by treating remaining tokens as positional input
        // candidates.
        i++;
        while (i < argv.length) {
          if (!opts.input) opts.input = argv[i];
          else failArg(`unexpected positional argument '${argv[i]}'`);
          i++;
        }
        break;
      default:
        if (a.startsWith('-')) failArg(`unknown option '${a}'`);
        if (!opts.input) opts.input = a;
        else failArg(`unexpected positional argument '${a}'`);
        break;
    }
  }
  if (opts.checkpoint && !opts.scenario) failArg('--checkpoint requires --scenario');
  if (opts.previewColumns !== undefined && !opts.preview) failArg('--preview-columns requires --preview');
  if (opts.preview && (!opts.scenario || opts.batchDir)) failArg('--preview requires --scenario and is not supported with --batch');
  if (opts.preview && (!opts.doRender || !opts.doResolve)) {
    failArg('--preview cannot be combined with --no-render or --no-resolve');
  }
  if (opts.scenario) {
    for (const flag of ['--batch', '--capture-animations', '--scroll-reveal', '--viewport-width',
      '--viewport-height', '--wait-ms', '--selector']) {
      if (argv.includes(flag)) failArg(`${flag} cannot be combined with --scenario; configure the scenario instead`);
    }
  }
  if (opts.batchDir) {
    // Batch mode owns the input slot — accepting a positional `input` here
    // would let the user accidentally pass both an HTML file and a directory.
    if (opts.input) {
      failArg(`--batch is incompatible with positional input '${opts.input}'`);
    }
    if (opts.outputName) {
      failArg('--batch is incompatible with --output-name (each file derives its own name)');
    }
    if (opts.outputDir) {
      failArg('--batch uses --output-root, not --output-dir');
    }
  } else {
    if (opts.outputRoot) failArg('--output-root only applies in --batch mode');
    if (opts.skipExisting) failArg('--skip-existing only applies in --batch mode');
    if (opts.dryRun) failArg('--dry-run only applies in --batch mode');
    if (!opts.input) {
      failArg('missing input HTML file');
    }
  }
  if (!SUPPORTED_ENGINES.includes(opts.browserEngine)) failArg(`unsupported --browser-engine '${opts.browserEngine}'`);
  if (opts.outputName && (opts.outputName === '.' || opts.outputName === '..' || /[\\/\0]/.test(opts.outputName))) {
    failArg('--output-name must be a filename, not a path');
  }
  return opts;
}


function captureUsage(toolName = 'html2pagx') {
  const usage = USAGE.replaceAll('html2pagx', toolName);
  return toolName === 'pagx capture'
    ? usage.replace('default: $PAGX_BIN or\n                             ../../cmake-build-debug/pagx relative to this script',
      'default: the invoking native pagx binary')
    : usage;
}

function validateCaptureOptions(opts) {
  if (opts.batchDir) {
    if (!fs.existsSync(opts.batchDir) || !fs.statSync(opts.batchDir).isDirectory()) {
      throw new CaptureCliError(`input directory not found: ${opts.batchDir}`, 1);
    }
  } else if (isHttpUrl(opts.input)) {
    if (!opts.outputName) failArg('--output-name <name> is required when input is a URL');
  } else if (!fs.existsSync(opts.input) || !fs.statSync(opts.input).isFile()) {
    throw new CaptureCliError(`input not found: ${opts.input}`, 1);
  }
  if ((opts.cookies.length || opts.headers.length) && !isHttpUrl(opts.input)) {
    failArg('cookies/headers require a URL input');
  }
  for (const value of opts.cookies) parseCookie('--cookie', value, failArg);
  for (const value of opts.headers) parseHeader('--header', value, failArg);
  if (opts.scenario) {
    let scenario;
    try { scenario = parseScenario(JSON.parse(fs.readFileSync(opts.scenario, 'utf8'))); }
    catch (err) { failArg(`invalid --scenario: ${err.message}`); }
    if (opts.checkpoint) {
      try { selectCheckpoint(scenario, opts.checkpoint); }
      catch (err) { failArg(err.message); }
    }
    if (opts.browserEngine !== 'playwright' && scenario.steps.some(step => step.target?.role)) {
      failArg('Role locators require --browser-engine playwright');
    }
  }
}

function preflightPagxBin(opts) {
  try {
    if (!fs.statSync(opts.pagxBin).isFile()) throw new Error('not a file');
    fs.accessSync(opts.pagxBin, process.platform === 'win32' ? fs.constants.F_OK : fs.constants.X_OK);
  } catch (_) {
    throw new CaptureCliError(`pagx binary not found or not executable: ${opts.pagxBin}\n`
      + 'Pass --pagx-bin <path> or build the native CLI.', 1);
  }
}

async function runCaptureCli(argv, { toolName = 'html2pagx', scriptDir = __dirname } = {}) {
  try {
    const opts = parseCaptureArgs(argv);
    if (!opts) { process.stdout.write(captureUsage(toolName) + '\n'); return 0; }
    validateCaptureOptions(opts);
    if (!opts.dryRun) {
      preflightPagxBin(opts);
      checkBrowserRuntime(opts.browserEngine);
    }
    const log = line => (toolName === 'html2pagx' ? process.stdout : process.stderr).write(line + '\n');
    return await withCliLifecycle(async () => {
      if (opts.batchDir) {
        const result = await runBatch({ ...opts, inputDir: opts.batchDir, scriptDir, log });
        return result.failed ? 1 : 0;
      }
      const run = opts.scenario ? runScenarioToPagx : runHtmlToPagx;
      await run({ ...opts, scriptDir, log });
      return 0;
    });
  } catch (err) {
    process.stderr.write(`${toolName}: ${err.message}\n`);
    if (err instanceof CaptureCliError && err.code === 2) process.stderr.write('Run with --help for usage.\n');
    return err instanceof CaptureCliError ? err.code : (err.exitCode || 1);
  }
}

module.exports = { CaptureCliError, parseCaptureArgs, validateCaptureOptions, preflightPagxBin, captureUsage, runCaptureCli };
