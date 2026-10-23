// Native command and snapshot helpers for evaluation tools and preview rendering.
// The public import workflow is owned by the native CLI; this module does not orchestrate it.

import * as fs from 'fs';
import * as path from 'path';
import { spawn } from 'child_process';

import { defaultPagxBin, filterKnownWarnings } from './pagx-runner';
import { isHttpUrl } from './common';
import { throwIfCancelled, trackCliResource } from './cli-lifecycle';

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
  throwIfCancelled();
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
    let killTimer: ReturnType<typeof setTimeout> | undefined;
    const stop = () => {
      if (killTimer) return;
      child.kill('SIGTERM');
      killTimer = setTimeout(() => child.kill('SIGKILL'), 5000);
      killTimer.unref();
    };
    const release = trackCliResource(stop);
    const timer = setTimeout(() => {
      killedByTimeout = true;
      stop();
    }, timeoutMs);
    child.on('error', (err: Error) => { stderr += `\n${err.message || String(err)}`; });
    child.on('close', (code: number | null) => {
      release();
      if (killTimer) clearTimeout(killTimer);
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
  scenario?: string;
  checkpoint?: string;
  output: string;
  browserEngine?: string;
  inlineIconFonts?: boolean;
  captureAnimations?: boolean;
  scrollReveal?: boolean;
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
  if (opts.scenario) args.push('--scenario', opts.scenario);
  if (opts.checkpoint) args.push('--checkpoint', opts.checkpoint);
  if (opts.browserEngine) args.push('--browser-engine', opts.browserEngine);
  if (opts.inlineIconFonts === false) args.push('--no-inline-icon-fonts');
  if (opts.captureAnimations) args.push('--capture-animations', '--no-reduced-motion');
  if (opts.scrollReveal) args.push('--scroll-reveal');
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

// Fork `node snapshot.js` to render <input> into a flat absolute-positioned
// subset HTML. The separate runSnapshot API uses an already-launched browser.
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
    '--html-source', 'snapshot', '--images', 'embed', '--no-resolve', '--overwrite'];
  // Flex inference is always on inside the importer now. The subset is already rendered, so
  // explicitly select prepared HTML to avoid a second browser render.
  const result = await spawnCapture(pagxBin, args, {
    stderrPath,
    timeoutMs,
    env: process.env,
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

// Optional Step 3.5 — `pagx embed <pagx> --skip-images --fallback <font>...`. Images are left
// alone here because their storage mode is owned by the resolve step's --images flag.
export async function runPagxFontEmbed(opts: RunPagxFontEmbedOptions = {}): Promise<SpawnCaptureResult> {
  const { pagxBin, pagxFile, fontFiles = [], stderrPath, timeoutMs } = opts;
  if (!pagxBin) throw new Error('runPagxFontEmbed: pagxBin is required');
  if (!pagxFile) throw new Error('runPagxFontEmbed: pagxFile is required');
  const args = ['embed', pagxFile, '--skip-images'];
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

export { defaultPagxBin, filterKnownWarnings, isHttpUrl };
