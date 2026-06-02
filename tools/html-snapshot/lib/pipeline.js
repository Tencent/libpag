// Shared pipeline orchestration for the html → PAGX flow.
//
// Two entry points consume this module:
//   - `tools/html-snapshot/html2pagx` — the user-facing single-file CLI
//     (run snapshot.js → pagx import → resolve → optional font embed → render
//     for one HTML document or URL).
//   - `tools/html-snapshot/eval/run.js` — the corpus evaluator (same pipeline
//     per case, plus baseline rendering, pixel/flex metrics, shared font/image
//     caches, and concurrency).
//
// Before this module existed, both entry points reimplemented the same five
// shell-outs side by side; eval/run.js even had `Mirrors html2pagx` comments
// scattered through it. The duplication forced every behaviour change into
// two places. This file consolidates those shell-outs so the shared pipeline
// has exactly one implementation, and each entry point is left only with the
// code that is genuinely its own (eval-specific reports + caches + pool;
// html2pagx-specific argument parsing + preflight + progress prints).
//
// Function shape:
//   - Per-step helpers (`runSnapshot`, `runPagxImportToFile`, `runPagxResolve`,
//     `runPagxFontEmbed`, `runPagxRender`) spawn their child process, capture
//     stderr (optionally writing it to a file the eval needs for later
//     analysis), and resolve to `{ code, durationMs, stderr }`. They never
//     throw on a non-zero exit — eval/run.js wants soft failure (record
//     `row.error = 'X-failed'`), html2pagx wants hard failure (exit 1 with a
//     prefixed message). Returning a structured result lets each caller pick
//     its own contract.
//   - `runHtmlToPagx` is the html2pagx-side end-to-end orchestrator: it
//     applies the per-step helpers in order, derives default output paths,
//     handles the optional resolve/render/embed gates, and throws
//     `PipelineStepError` when a step exits non-zero. eval/run.js does its
//     own gating (skipExisting, per-case concurrency cursor) and therefore
//     calls the per-step helpers directly rather than the orchestrator.

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

const { defaultPagxBin } = require('./pagx-runner');

// `pagx import --format html` emits one warning per absolute-positioned text
// leaf — the importer normalises them silently in the resulting PAGX, but
// the warnings still spam stderr at one line per element. The bash wrapper
// (`html2pagx`) used to filter them out before printing pagx's stderr; we do
// the same here so a 200-element page doesn't produce 200 lines of noise.
// Kept as a standalone function so server.js and eval can share the same
// filter rule without each maintaining its own.
function filterKnownWarnings(stderr) {
  if (!stderr) return '';
  return stderr
    .split('\n')
    .filter((line) => !line.includes('warning: html: position: absolute on text leaf'))
    .join('\n')
    .trim();
}

// Tagged error class thrown by `runHtmlToPagx` when one of the pipeline steps
// exits non-zero. Carries the step label, exit code, and stderr so the
// html2pagx CLI can print a clean `html2pagx: <step> failed` message and
// surface the underlying tool's diagnostics.
class PipelineStepError extends Error {
  constructor(message, { step, code, stderr } = {}) {
    super(message);
    this.name = 'PipelineStepError';
    this.step = step || '';
    this.code = code === undefined ? null : code;
    this.stderr = stderr || '';
  }
}

// Default per-child timeout. eval/run.js used 180s for years; matching it here
// keeps a stuck pagx invocation from hanging the parent forever without
// changing the eval's observed behaviour. The orchestrator forwards this to
// every step; callers that want a different ceiling pass `timeoutMs`.
const DEFAULT_STEP_TIMEOUT_MS = 180000;

// Internal: spawn a child, capture stdout/stderr, optionally tee stderr to a
// file (eval needs it on disk for compare.countImporterWarnings), enforce a
// timeout, and resolve with `{ code, durationMs, stdout, stderr }`. Never
// rejects on a child failure — the resolve shape is the per-step contract.
function spawnCapture(cmd, args, opts = {}) {
  const stderrPath = opts.stderrPath || '';
  const timeoutMs = typeof opts.timeoutMs === 'number' ? opts.timeoutMs : DEFAULT_STEP_TIMEOUT_MS;
  return new Promise((resolve) => {
    const startedAt = Date.now();
    let child;
    try {
      child = spawn(cmd, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    } catch (err) {
      const stderr = `\n${err.message || String(err)}`;
      if (stderrPath) {
        try { fs.writeFileSync(stderrPath, stderr, 'utf8'); } catch (_e) { /* ignore */ }
      }
      resolve({ code: null, durationMs: Date.now() - startedAt, stdout: '', stderr });
      return;
    }
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (d) => { stdout += d.toString(); });
    child.stderr.on('data', (d) => { stderr += d.toString(); });
    let killedByTimeout = false;
    const timer = setTimeout(() => {
      killedByTimeout = true;
      try { child.kill('SIGKILL'); } catch (_e) { /* ignore */ }
    }, timeoutMs);
    child.on('error', (err) => { stderr += `\n${err.message || String(err)}`; });
    child.on('close', (code) => {
      clearTimeout(timer);
      if (killedByTimeout) stderr += `\n[pipeline] killed by timeout (${timeoutMs}ms)`;
      if (stderrPath) {
        try { fs.writeFileSync(stderrPath, stderr, 'utf8'); } catch (_e) { /* ignore */ }
      }
      resolve({ code, durationMs: Date.now() - startedAt, stdout, stderr });
    });
  });
}

// Build the `node snapshot.js <input> -o <output> [...]` argv. Centralised so
// runSnapshot and any future consumer agree on flag names. `extraArgs` is an
// escape hatch for callers that need to pass through a flag this helper has
// not learned about yet.
function buildSnapshotArgs(opts) {
  const args = [path.join(opts.scriptDir, 'snapshot.js'), opts.input, '-o', opts.output];
  if (opts.browserEngine) args.push('--browser-engine', opts.browserEngine);
  if (opts.inlineIconFonts === false) args.push('--no-inline-icon-fonts');
  if (typeof opts.viewportWidth === 'number') args.push('--viewport-width', String(opts.viewportWidth));
  if (typeof opts.viewportHeight === 'number') args.push('--viewport-height', String(opts.viewportHeight));
  if (typeof opts.waitMs === 'number') args.push('--wait-ms', String(opts.waitMs));
  if (opts.selector) args.push('--selector', opts.selector);
  if (Array.isArray(opts.cookies)) {
    for (const c of opts.cookies) args.push('--cookie', c);
  }
  if (Array.isArray(opts.headers)) {
    for (const h of opts.headers) args.push('--header', h);
  }
  if (opts.downloadFonts) {
    args.push('--download-fonts');
    if (opts.fontDir) args.push('--font-dir', opts.fontDir);
    if (opts.fontManifest) args.push('--font-manifest', opts.fontManifest);
  }
  if (opts.downloadImages) {
    args.push('--download-images');
    if (opts.imageDir) args.push('--image-dir', opts.imageDir);
  }
  if (Array.isArray(opts.extraArgs)) args.push(...opts.extraArgs);
  return args;
}

// Step 1 — render <input> into a flat absolute-positioned subset HTML.
//
// Required: `input` (file path or http(s) URL), `output` (subset HTML path),
// `scriptDir` (directory containing snapshot.js).
// Optional: every snapshot-side flag (browserEngine, inlineIconFonts,
// viewport*, waitMs, selector, cookies, headers, downloadFonts/fontDir/
// fontManifest, downloadImages/imageDir). `stderrPath` tees stderr to disk;
// `nodeBin` overrides the Node interpreter (defaults to `process.execPath` so
// child Node matches the parent — matters when the parent is run via `npx`,
// `nvm`, or a non-default install).
async function runSnapshot(opts = {}) {
  if (!opts.input) throw new Error('runSnapshot: input is required');
  if (!opts.output) throw new Error('runSnapshot: output is required');
  if (!opts.scriptDir) throw new Error('runSnapshot: scriptDir is required');
  const nodeBin = opts.nodeBin || process.execPath;
  const args = buildSnapshotArgs(opts);
  return spawnCapture(nodeBin, args, {
    stderrPath: opts.stderrPath,
    timeoutMs: opts.timeoutMs,
  });
}

// Step 2 — run `pagx import --format html --input <subset> --output <pagx>`.
//
// `inferFlex` defaults to true (matches html2pagx's old behaviour: pass
// --html-infer-flex unless explicitly disabled). The result includes raw
// stderr; if `log` is provided, the filtered stderr (warnings stripped) is
// emitted via that callback so the caller can surface diagnostics without
// the per-leaf warning noise.
async function runPagxImportToFile(opts = {}) {
  const { pagxBin, subsetHtml, pagxFile, inferFlex = true, stderrPath, log, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxImportToFile: pagxBin is required');
  if (!subsetHtml) throw new Error('runPagxImportToFile: subsetHtml is required');
  if (!pagxFile) throw new Error('runPagxImportToFile: pagxFile is required');
  const args = ['import', '--input', subsetHtml, '--output', pagxFile, '--format', 'html'];
  if (inferFlex) args.push('--html-infer-flex');
  const result = await spawnCapture(pagxBin, args, { stderrPath, timeoutMs });
  if (log) {
    const filtered = filterKnownWarnings(result.stderr);
    if (filtered) log(filtered);
  }
  return result;
}

// Step 3 — `pagx resolve <pagx>`. Resolves external image/font references the
// document carries by URL into self-contained payloads where possible.
async function runPagxResolve(opts = {}) {
  const { pagxBin, pagxFile, stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxResolve: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxResolve: pagxFile is required');
  return spawnCapture(pagxBin, ['resolve', pagxFile], { stderrPath, timeoutMs });
}

// Optional Step 3.5 — `pagx font embed <pagx> --fallback <font>...`.
// Only meaningful when `fontFiles` is non-empty. Caller decides whether to
// invoke at all (gated on --embed-fonts in html2pagx and eval).
async function runPagxFontEmbed(opts = {}) {
  const { pagxBin, pagxFile, fontFiles = [], stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxFontEmbed: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxFontEmbed: pagxFile is required');
  const args = ['font', 'embed', pagxFile];
  for (const f of fontFiles) args.push('--fallback', f);
  return spawnCapture(pagxBin, args, { stderrPath, timeoutMs });
}

// Step 4 — `pagx render <pagx> --output <png> --scale <s> [--fallback <font>...]`.
async function runPagxRender(opts = {}) {
  const { pagxBin, pagxFile, output, scale = 1, fontFallbacks = [], stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxRender: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxRender: pagxFile is required');
  if (!output) throw new Error('runPagxRender: output is required');
  const args = ['render', pagxFile, '--output', output, '--scale', String(scale)];
  for (const f of fontFallbacks) args.push('--fallback', f);
  return spawnCapture(pagxBin, args, { stderrPath, timeoutMs });
}

// Glob a directory for SFNT files (TTF / OTF / TTC). Used by html2pagx after
// snapshot.js writes its captured fonts into `fontDir` — the resulting list
// is registered with `pagx render --fallback` and (optionally)
// `pagx font embed --fallback` so text styled with an uninstalled web font
// resolves against the real face. Returns absolute paths sorted by name for
// deterministic ordering.
function collectFontFallbacks(opts = {}) {
  const fontDir = opts.fontDir;
  if (!fontDir) return [];
  let entries;
  try {
    entries = fs.readdirSync(fontDir);
  } catch (_err) {
    return [];
  }
  const out = entries
    .filter((n) => /\.(ttf|otf|ttc)$/i.test(n))
    .map((n) => path.join(fontDir, n));
  out.sort();
  return out;
}

// Read a per-case font manifest produced by `snapshot.js --font-manifest`
// (one absolute path per line). Used by eval/run.js to register only the
// fonts a particular case actually uses, when fonts share a single content-
// addressed cache directory across the whole corpus. Skips missing entries
// silently — a stale manifest line should not abort the run.
function readFontManifest(opts = {}) {
  const manifestPath = opts.manifestPath;
  if (!manifestPath) return [];
  let text;
  try {
    text = fs.readFileSync(manifestPath, 'utf8');
  } catch (_err) {
    return [];
  }
  return text
    .split('\n')
    .map((l) => l.trim())
    .filter((l) => l && fs.existsSync(l));
}

// Detect an http(s) URL in the same way the snapshot CLI does. Centralised so
// runHtmlToPagx and html2pagx agree on the rule (and to avoid importing
// `cli.js` here, which would pull the snapshot-side flag table for no reason).
function isHttpUrl(s) {
  return typeof s === 'string' && /^https?:\/\//i.test(s);
}

// Default callers' `log` to a no-op when omitted; runHtmlToPagx prints
// `[N/M] step ...` progress lines through this hook so html2pagx can stream
// them to stdout while embedded callers stay quiet.
function noopLog() {}

// End-to-end orchestrator. Runs snapshot → import → optional resolve →
// optional font embed → optional render. Throws `PipelineStepError` on the
// first non-zero step so the html2pagx CLI can print a uniform
// `html2pagx: <step> failed` message and exit 1.
//
// Output paths follow html2pagx's old convention:
//   subset HTML → <outputDir>/<base>.subset.html
//   pagx        → <outputDir>/<base>.pagx
//   png         → <outputDir>/<base>.png
//   fonts dir   → <outputDir>/<base>.fonts/  (when --download-fonts and
//                 fontDir is unset)
//   images dir  → <outputDir>/<base>.images/ (when --download-images and
//                 imageDir is unset)
//
// `keepSubsetHtml` controls whether the subset HTML survives on disk after
// import; when false the file is written to a self-cleaning temp dir.
//
// `snapshotImpl` (optional) replaces the default fork-`node snapshot.js` step
// with a caller-provided async function of the same `(snapshotArgs) => { code,
// durationMs, stderr }` shape. Batch mode injects an in-process variant that
// reuses one long-lived browser across many files; everyone else relies on the
// default fork-per-file path.
async function runHtmlToPagx(opts = {}) {
  const log = typeof opts.log === 'function' ? opts.log : noopLog;
  const pagxBin = opts.pagxBin || defaultPagxBin();
  const scriptDir = opts.scriptDir;
  if (!scriptDir) throw new Error('runHtmlToPagx: scriptDir is required');

  const inputIsUrl = isHttpUrl(opts.input);
  let outputDir = opts.outputDir || '';
  let baseName = opts.outputName || '';

  if (inputIsUrl) {
    if (!baseName) {
      throw new Error('runHtmlToPagx: outputName is required for URL input');
    }
    if (!outputDir) outputDir = process.cwd();
  } else {
    if (!opts.input) throw new Error('runHtmlToPagx: input is required');
    if (!fs.existsSync(opts.input)) {
      throw new Error(`runHtmlToPagx: input not found: ${opts.input}`);
    }
    const inputAbs = path.resolve(opts.input);
    if (!outputDir) outputDir = path.dirname(inputAbs);
    if (!baseName) {
      let stem = path.basename(inputAbs, path.extname(inputAbs));
      if (stem.endsWith('.htm')) stem = stem.slice(0, -'.htm'.length);
      baseName = stem;
    }
  }
  fs.mkdirSync(outputDir, { recursive: true });
  outputDir = path.resolve(outputDir);

  // When the user opts out of keeping the subset HTML on disk, redirect it to
  // a self-cleaning temp dir; pagx import still needs a file to read.
  let subsetHtml;
  let tempDir = '';
  if (opts.keepSubsetHtml === false) {
    tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'html2pagx-'));
    subsetHtml = path.join(tempDir, `${baseName}.subset.html`);
  } else {
    subsetHtml = path.join(outputDir, `${baseName}.subset.html`);
  }
  const pagxFile = path.join(outputDir, `${baseName}.pagx`);
  const pngFile = path.join(outputDir, `${baseName}.png`);

  const doResolve = opts.doResolve !== false;
  const doRender = opts.doRender !== false && doResolve;
  const embedFonts = !!opts.embedFonts;
  const downloadFonts = !!opts.downloadFonts || embedFonts;
  const downloadImages = !!opts.downloadImages;

  let fontDir = opts.fontDir || '';
  if (downloadFonts && !fontDir) {
    fontDir = path.join(outputDir, `${baseName}.fonts`);
  }
  let imageDir = opts.imageDir || '';
  if (downloadImages && !imageDir) {
    imageDir = path.join(outputDir, `${baseName}.images`);
  }

  // Step counter for human-readable progress prints. Matches the old bash
  // wrapper's `[N/M] step ...` format that lib/batch.js and CI logs depend
  // on for line-wise grepping.
  let totalSteps = 2;
  if (doResolve) totalSteps = 3;
  if (doRender) totalSteps = 4;

  // Step 1 implementation is injectable. Default = fork `node snapshot.js` per
  // file (the historical CLI behaviour). Batch mode (`lib/batch.js`) injects an
  // in-process variant that calls `lib/snapshot-runner.js` against a single
  // long-lived browser, saving ~1.5s of Chromium startup per file.
  const snapshotImpl = typeof opts.snapshotImpl === 'function' ? opts.snapshotImpl : runSnapshot;

  try {
    log(`[1/${totalSteps}] snapshot  ${opts.input}`);
    const snapshotResult = await snapshotImpl({
      input: opts.input,
      output: subsetHtml,
      scriptDir,
      browserEngine: opts.browserEngine,
      inlineIconFonts: opts.inlineIconFonts,
      cookies: opts.cookies,
      headers: opts.headers,
      viewportWidth: opts.viewportWidth,
      viewportHeight: opts.viewportHeight,
      waitMs: opts.waitMs,
      selector: opts.selector,
      downloadFonts,
      fontDir,
      downloadImages,
      imageDir,
      extraArgs: opts.snapshotExtraArgs,
    });
    if (snapshotResult.code !== 0) {
      process.stderr.write(snapshotResult.stderr);
      throw new PipelineStepError('snapshot failed', {
        step: 'snapshot',
        code: snapshotResult.code,
        stderr: snapshotResult.stderr,
      });
    } else if (snapshotResult.stderr) {
      process.stderr.write(snapshotResult.stderr);
    }

    let fonts = [];
    if (downloadFonts) {
      fonts = collectFontFallbacks({ fontDir });
      if (fonts.length === 0) {
        log(`html2pagx: --download-fonts: no web fonts were captured in ${fontDir}`);
      }
    }

    log(`[2/${totalSteps}] import    ${subsetHtml}`);
    const importResult = await runPagxImportToFile({
      pagxBin,
      subsetHtml,
      pagxFile,
      inferFlex: opts.inferFlex !== false,
      log: (line) => process.stderr.write(line + '\n'),
    });
    if (importResult.code !== 0 || !fs.existsSync(pagxFile)) {
      throw new PipelineStepError('pagx import failed', {
        step: 'pagx import',
        code: importResult.code,
        stderr: importResult.stderr,
      });
    }

    if (!doResolve) {
      log(`[done] wrote ${pagxFile} (skipped resolve+render)`);
      return { subsetHtml, pagx: pagxFile, png: null, fonts };
    }

    log(`[3/${totalSteps}] resolve   ${pagxFile}`);
    const resolveResult = await runPagxResolve({ pagxBin, pagxFile });
    if (resolveResult.code !== 0) {
      process.stderr.write(resolveResult.stderr);
      throw new PipelineStepError('pagx resolve failed', {
        step: 'pagx resolve',
        code: resolveResult.code,
        stderr: resolveResult.stderr,
      });
    } else if (resolveResult.stderr) {
      process.stderr.write(resolveResult.stderr);
    }

    if (embedFonts && fonts.length > 0) {
      log(`[font-embed] ${pagxFile} (${fonts.length} font file(s))`);
      const embedResult = await runPagxFontEmbed({ pagxBin, pagxFile, fontFiles: fonts });
      if (embedResult.code !== 0) {
        process.stderr.write(embedResult.stderr);
        throw new PipelineStepError('pagx font embed failed', {
          step: 'pagx font embed',
          code: embedResult.code,
          stderr: embedResult.stderr,
        });
      } else if (embedResult.stderr) {
        process.stderr.write(embedResult.stderr);
      }
    }

    if (!doRender) {
      log(`[done] wrote ${pagxFile} (skipped render)`);
      return { subsetHtml, pagx: pagxFile, png: null, fonts };
    }

    log(`[4/${totalSteps}] render    ${pagxFile}  ->  ${pngFile} (scale=${opts.scale || 1})`);
    const renderResult = await runPagxRender({
      pagxBin,
      pagxFile,
      output: pngFile,
      scale: opts.scale || 1,
      fontFallbacks: fonts,
    });
    if (renderResult.code !== 0) {
      process.stderr.write(renderResult.stderr);
      throw new PipelineStepError('pagx render failed', {
        step: 'pagx render',
        code: renderResult.code,
        stderr: renderResult.stderr,
      });
    } else if (renderResult.stderr) {
      process.stderr.write(renderResult.stderr);
    }

    log(`[done] ${pngFile}`);
    return { subsetHtml, pagx: pagxFile, png: pngFile, fonts };
  } finally {
    if (tempDir) {
      try { fs.rmSync(tempDir, { recursive: true, force: true }); } catch (_e) { /* ignore */ }
    }
  }
}

module.exports = {
  defaultPagxBin,
  filterKnownWarnings,
  PipelineStepError,
  spawnCapture,
  runSnapshot,
  runPagxImportToFile,
  runPagxResolve,
  runPagxFontEmbed,
  runPagxRender,
  collectFontFallbacks,
  readFontManifest,
  runHtmlToPagx,
  isHttpUrl,
};
