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

import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { spawn } from 'child_process';

import { defaultPagxBin, filterKnownWarnings } from './pagx-runner';
import { isHttpUrl } from './common';
import { ChildProcessError, type ChildProcessErrorInit } from './errors';

// Tagged error class thrown by `runHtmlToPagx` when one of the pipeline steps
// exits non-zero. Carries the step label, exit code, and stderr so the
// html2pagx CLI can print a clean `html2pagx: <step> failed` message and
// surface the underlying tool's diagnostics. Inherits `{ code, stderr }`
// field initialisation from `ChildProcessError`; only the `step` label and
// `name` are specific to this subclass.
export interface PipelineStepErrorInit extends ChildProcessErrorInit {
  step?: string;
}

export class PipelineStepError extends ChildProcessError {
  step: string;

  constructor(message: string, opts: PipelineStepErrorInit = {}) {
    super(message, opts);
    this.name = 'PipelineStepError';
    this.step = opts.step || '';
  }
}

// Default per-child timeout. eval/run.js used 180s for years; matching it here
// keeps a stuck pagx invocation from hanging the parent forever without
// changing the eval's observed behaviour. The orchestrator forwards this to
// every step; callers that want a different ceiling pass `timeoutMs`.
const DEFAULT_STEP_TIMEOUT_MS = 180000;

export interface SpawnCaptureOptions {
  stderrPath?: string;
  timeoutMs?: number;
  env?: NodeJS.ProcessEnv;
}

export interface SpawnCaptureResult {
  code: number | null;
  durationMs: number;
  stdout: string;
  stderr: string;
}

// Internal: spawn a child, capture stdout/stderr, optionally tee stderr to a
// file (eval needs it on disk for compare.countImporterWarnings), enforce a
// timeout, and resolve with `{ code, durationMs, stdout, stderr }`. Never
// rejects on a child failure — the resolve shape is the per-step contract.
export function spawnCapture(
  cmd: string,
  args: string[],
  opts: SpawnCaptureOptions = {},
): Promise<SpawnCaptureResult> {
  const stderrPath = opts.stderrPath || '';
  const timeoutMs = typeof opts.timeoutMs === 'number' ? opts.timeoutMs : DEFAULT_STEP_TIMEOUT_MS;
  return new Promise<SpawnCaptureResult>((resolve) => {
    const startedAt = Date.now();
    let child;
    try {
      child = spawn(cmd, args, {
        stdio: ['ignore', 'pipe', 'pipe'],
        env: opts.env || process.env,
      });
    } catch (err) {
      const stderr = `\n${(err as Error).message || String(err)}`;
      if (stderrPath) {
        try { fs.writeFileSync(stderrPath, stderr, 'utf8'); } catch (_e) { /* ignore */ }
      }
      resolve({ code: null, durationMs: Date.now() - startedAt, stdout: '', stderr });
      return;
    }
    let stdout = '';
    let stderr = '';
    child.stdout?.on('data', (d: Buffer) => { stdout += d.toString(); });
    child.stderr?.on('data', (d: Buffer) => { stderr += d.toString(); });
    let killedByTimeout = false;
    const timer = setTimeout(() => {
      killedByTimeout = true;
      try { child.kill('SIGKILL'); } catch (_e) { /* ignore */ }
    }, timeoutMs);
    child.on('error', (err: Error) => { stderr += `\n${err.message || String(err)}`; });
    child.on('close', (code: number | null) => {
      clearTimeout(timer);
      if (killedByTimeout) stderr += `\n[pipeline] killed by timeout (${timeoutMs}ms)`;
      if (stderrPath) {
        try { fs.writeFileSync(stderrPath, stderr, 'utf8'); } catch (_e) { /* ignore */ }
      }
      resolve({ code, durationMs: Date.now() - startedAt, stdout, stderr });
    });
  });
}

export interface BuildSnapshotArgsOptions {
  scriptDir: string;
  input: string;
  output: string;
  browserEngine?: string;
  inlineIconFonts?: boolean;
  viewportWidth?: number;
  viewportHeight?: number;
  waitMs?: number;
  selector?: string;
  cookies?: string[];
  headers?: string[];
  downloadFonts?: boolean;
  fontDir?: string;
  fontManifest?: string;
  downloadImages?: boolean;
  imageDir?: string;
  extraArgs?: string[];
}

// Build the `node snapshot.js <input> -o <output> [...]` argv. Centralised so
// forkSnapshotCli and any future consumer agree on flag names. `extraArgs` is
// an escape hatch for callers that need to pass through a flag this helper
// has not learned about yet.
function buildSnapshotArgs(opts: BuildSnapshotArgsOptions): string[] {
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

export interface ForkSnapshotCliOptions extends BuildSnapshotArgsOptions, SpawnCaptureOptions {
  nodeBin?: string;
}

// Step 1 — fork `node snapshot.js` to render <input> into a flat absolute-
// positioned subset HTML. Named `forkSnapshotCli` (rather than `runSnapshot`)
// so it never collides with `lib/snapshot-runner.ts`'s `runSnapshot`, which
// drives the snapshot pipeline in-process against an already-launched browser
// — both names previously coexisted and `lib/batch.ts` had to import them
// side-by-side, which made the difference between "fork a child" and "drive
// in-process" invisible at the call site.
export async function forkSnapshotCli(opts: Partial<ForkSnapshotCliOptions> = {}): Promise<SpawnCaptureResult> {
  if (!opts.input) throw new Error('forkSnapshotCli: input is required');
  if (!opts.output) throw new Error('forkSnapshotCli: output is required');
  if (!opts.scriptDir) throw new Error('forkSnapshotCli: scriptDir is required');
  const nodeBin = opts.nodeBin || process.execPath;
  const args = buildSnapshotArgs(opts as BuildSnapshotArgsOptions);
  return spawnCapture(nodeBin, args, {
    stderrPath: opts.stderrPath,
    timeoutMs: opts.timeoutMs,
  });
}

export interface RunPagxImportToFileOptions extends SpawnCaptureOptions {
  pagxBin?: string;
  subsetHtml?: string;
  pagxFile?: string;
  log?: (line: string) => void;
}

// Step 2 — run `pagx import --format html --input <subset> --output <pagx>`.
export async function runPagxImportToFile(
  opts: RunPagxImportToFileOptions = {},
): Promise<SpawnCaptureResult> {
  const { pagxBin, subsetHtml, pagxFile, stderrPath, log, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxImportToFile: pagxBin is required');
  if (!subsetHtml) throw new Error('runPagxImportToFile: subsetHtml is required');
  if (!pagxFile) throw new Error('runPagxImportToFile: pagxFile is required');
  // `--no-resolve`: `pagx import` resolves import directives by default now, but this pipeline
  // has its own dedicated resolve step (runPagxResolve) that owns diagnostics, progress logging,
  // and font-embed ordering. Opt out here so resolution happens exactly once, in that step.
  const args = ['import', '--input', subsetHtml, '--output', pagxFile, '--format', 'html',
    '--no-resolve'];
  // Flex inference is always on inside the importer now. The subset is already rendered, so
  // disable the importer's own snapshot pre-pass to avoid a redundant second browser render.
  const result = await spawnCapture(pagxBin, args, {
    stderrPath,
    timeoutMs,
    env: { ...process.env, PAGX_HTML_SNAPSHOT: '0' },
  });
  if (log) {
    const filtered = filterKnownWarnings(result.stderr);
    if (filtered) log(filtered);
  }
  return result;
}

export interface RunPagxResolveOptions extends SpawnCaptureOptions {
  pagxBin?: string;
  pagxFile?: string;
  // How image resources are stored in the resolved PAGX: 'external' (default; write image files
  // next to the .pagx, keeping their relative path) or 'embed' (inline as base64 data URIs).
  imageStorage?: 'external' | 'embed';
  // Directory used to resolve relative image paths (e.g. inline-SVG <image href>) to real files.
  // For subset HTML produced from a source document, this is the source document's directory.
  imageBaseDir?: string;
}

// Step 3 — `pagx resolve <pagx>`. Image resources created while resolving inline SVG are
// normalised to the requested storage mode via `--images` / `--image-base-dir`.
export async function runPagxResolve(opts: RunPagxResolveOptions = {}): Promise<SpawnCaptureResult> {
  const { pagxBin, pagxFile, imageStorage, imageBaseDir, stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxResolve: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxResolve: pagxFile is required');
  const args = ['resolve', pagxFile];
  if (imageStorage) args.push('--images', imageStorage);
  if (imageBaseDir) args.push('--image-base-dir', imageBaseDir);
  return spawnCapture(pagxBin, args, { stderrPath, timeoutMs });
}

export interface RunPagxFontEmbedOptions extends SpawnCaptureOptions {
  pagxBin?: string;
  pagxFile?: string;
  fontFiles?: string[];
}

// Optional Step 3.5 — `pagx font embed <pagx> --fallback <font>...`.
export async function runPagxFontEmbed(opts: RunPagxFontEmbedOptions = {}): Promise<SpawnCaptureResult> {
  const { pagxBin, pagxFile, fontFiles = [], stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxFontEmbed: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxFontEmbed: pagxFile is required');
  const args = ['font', 'embed', pagxFile];
  for (const f of fontFiles) args.push('--fallback', f);
  return spawnCapture(pagxBin, args, { stderrPath, timeoutMs });
}

export interface RunPagxRenderOptions extends SpawnCaptureOptions {
  pagxBin?: string;
  pagxFile?: string;
  output?: string;
  scale?: number;
  fontFallbacks?: string[];
}

// Step 4 — `pagx render <pagx> --output <png> --scale <s> [--fallback <font>...]`.
export async function runPagxRender(opts: RunPagxRenderOptions = {}): Promise<SpawnCaptureResult> {
  const { pagxBin, pagxFile, output, scale = 1, fontFallbacks = [], stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxRender: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxRender: pagxFile is required');
  if (!output) throw new Error('runPagxRender: output is required');
  const args = ['render', pagxFile, '--output', output, '--scale', String(scale)];
  for (const f of fontFallbacks) args.push('--fallback', f);
  return spawnCapture(pagxBin, args, { stderrPath, timeoutMs });
}

// Glob a directory for SFNT files (TTF / OTF / TTC).
export function collectFontFallbacks(opts: { fontDir?: string } = {}): string[] {
  const fontDir = opts.fontDir;
  if (!fontDir) return [];
  let entries: string[];
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

// Read a per-case font manifest produced by `snapshot.js --font-manifest`.
export function readFontManifest(opts: { manifestPath?: string } = {}): string[] {
  const manifestPath = opts.manifestPath;
  if (!manifestPath) return [];
  let text: string;
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

function noopLog(_msg: string): void {}

// Standard "did the step succeed?" check used by every fork-based step in
// runHtmlToPagx.
function assertStepOk(stepName: string, result: SpawnCaptureResult): void {
  if (result.code !== 0) {
    process.stderr.write(result.stderr);
    throw new PipelineStepError(`${stepName} failed`, {
      step: stepName,
      code: result.code,
      stderr: result.stderr,
    });
  }
  if (result.stderr) process.stderr.write(result.stderr);
}

export type SnapshotImpl = (args: BuildSnapshotArgsOptions) => Promise<SpawnCaptureResult>;

export interface RunHtmlToPagxOptions {
  log?: (line: string) => void;
  pagxBin?: string;
  scriptDir?: string;
  input?: string;
  outputDir?: string;
  outputName?: string;
  scale?: number;
  doResolve?: boolean;
  doRender?: boolean;
  keepSubsetHtml?: boolean;
  embedFonts?: boolean;
  downloadFonts?: boolean;
  fontDir?: string;
  downloadImages?: boolean;
  imageDir?: string;
  // How image resources are stored in the exported .pagx: 'external' (default; write image files
  // next to the .pagx, keeping their relative path) or 'embed' (inline as base64 data URIs).
  pagxImages?: 'external' | 'embed';
  // Directory relative image paths resolve against. Defaults to the input document's directory.
  pagxImageBaseDir?: string;
  browserEngine?: string;
  inlineIconFonts?: boolean;
  cookies?: string[];
  headers?: string[];
  viewportWidth?: number;
  viewportHeight?: number;
  waitMs?: number;
  selector?: string;
  snapshotImpl?: SnapshotImpl;
  snapshotExtraArgs?: string[];
}

export interface RunHtmlToPagxResult {
  subsetHtml: string;
  pagx: string;
  png: string | null;
  fonts: string[];
}

// End-to-end orchestrator. Runs snapshot → import → optional resolve →
// optional font embed → optional render. Throws `PipelineStepError` on the
// first non-zero step so the html2pagx CLI can print a uniform
// `html2pagx: <step> failed` message and exit 1.
export async function runHtmlToPagx(opts: RunHtmlToPagxOptions = {}): Promise<RunHtmlToPagxResult> {
  const log = typeof opts.log === 'function' ? opts.log : noopLog;
  const pagxBin = opts.pagxBin || defaultPagxBin();
  const scriptDir = opts.scriptDir;
  if (!scriptDir) throw new Error('runHtmlToPagx: scriptDir is required');
  if (!opts.input) throw new Error('runHtmlToPagx: input is required');

  const inputIsUrl = isHttpUrl(opts.input);
  let outputDir = opts.outputDir || '';
  let baseName = opts.outputName || '';

  if (inputIsUrl) {
    if (!baseName) {
      throw new Error('runHtmlToPagx: outputName is required for URL input');
    }
    if (!outputDir) outputDir = process.cwd();
  } else {
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
  let subsetHtml: string;
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

  let totalSteps = 2;
  if (doResolve) totalSteps = 3;
  if (doRender) totalSteps = 4;

  const snapshotImpl: SnapshotImpl = typeof opts.snapshotImpl === 'function'
    ? opts.snapshotImpl
    : (forkSnapshotCli as SnapshotImpl);

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
    assertStepOk('snapshot', snapshotResult);

    let fonts: string[] = [];
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
      log: (line: string) => process.stderr.write(line + '\n'),
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
    // Image resources (both <img> and inline-SVG <image>) exist only after resolve, so the image
    // storage mode is applied here. Relative paths in the subset resolve against the original input
    // document's directory (the subset may live in a different output dir).
    const pagxImageBaseDir = opts.pagxImageBaseDir
      || (inputIsUrl ? '' : path.dirname(path.resolve(opts.input)));
    const resolveResult = await runPagxResolve({
      pagxBin,
      pagxFile,
      imageStorage: opts.pagxImages || 'external',
      imageBaseDir: pagxImageBaseDir,
    });
    assertStepOk('pagx resolve', resolveResult);

    if (embedFonts && fonts.length > 0) {
      log(`[font-embed] ${pagxFile} (${fonts.length} font file(s))`);
      const embedResult = await runPagxFontEmbed({ pagxBin, pagxFile, fontFiles: fonts });
      assertStepOk('pagx font embed', embedResult);
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
    assertStepOk('pagx render', renderResult);

    log(`[done] ${pngFile}`);
    return { subsetHtml, pagx: pagxFile, png: pngFile, fonts };
  } finally {
    if (tempDir) {
      try { fs.rmSync(tempDir, { recursive: true, force: true }); } catch (_e) { /* ignore */ }
    }
  }
}

export { defaultPagxBin, filterKnownWarnings, isHttpUrl };
