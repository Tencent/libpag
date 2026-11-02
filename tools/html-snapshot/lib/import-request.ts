// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as fs from 'fs';
import * as path from 'path';
import { collectBatchInputs, runBatch } from './batch';
import { resolveEngine, checkBrowserRuntime } from './browser-engine';
import { parseCookie, parseHeader } from './cli';
import { withCliLifecycle } from './cli-lifecycle';
import { generateContactSheet, validatePreviewOptions } from './contact-sheet';
import { parseScenario, selectCheckpoint, validateScenarioCaptureOptions } from './interaction';
import { convertToArtifacts, isHttpUrl, runHtmlToPagx } from './pipeline';
import { runScenarioToPagx, type RunScenarioToPagxOptions } from './scenario-pipeline';

export interface ImportRequest extends RunScenarioToPagxOptions {
  version: 1;
  input: string;
  format: 'html' | 'svg';
  pagxBin: string;
  batchDir?: string;
  dryRun?: boolean;
  skipExisting?: boolean;
}

export class ImportRequestError extends Error {
  constructor(message: string, public exitCode = 2) { super(message); }
}
const invalid = (message: string): never => { throw new ImportRequestError(message); };

// This is an internal native-to-Node protocol, not another user-facing option parser.
export function readImportRequest(args: string[]): ImportRequest {
  if (args.length !== 2 || args[0] !== '--request') invalid('expected an internal import request');
  let request: ImportRequest;
  try { request = JSON.parse(args[1]) as ImportRequest; }
  catch (_) { return invalid('invalid import request JSON'); }
  if (!request || request.version !== 1 || !['html', 'svg'].includes(request.format) ||
      typeof request.input !== 'string' || typeof request.pagxBin !== 'string') {
    invalid('unsupported import request');
  }
  return request;
}

function directoryDestination(directory: string): void {
  let parent = path.resolve(directory);
  while (!fs.existsSync(parent) && path.dirname(parent) !== parent) parent = path.dirname(parent);
  if (!fs.statSync(parent).isDirectory()) invalid(`output parent is not a directory: ${parent}`);
}

function canonical(file: string): string {
  const resolved = fs.existsSync(file) ? fs.realpathSync(file) : path.resolve(file);
  return process.platform === 'win32' || process.platform === 'darwin'
    ? resolved.normalize('NFC').toLowerCase() : resolved;
}

/** Determine all destinations before starting a browser or writing an artifact. */
export function validateImportRequest(opts: ImportRequest): ImportRequest {
  try {
    validatePreviewOptions(opts);
    if (opts.scenario) validateScenarioCaptureOptions(opts);
    if (opts.format === 'html' && !opts.htmlNoSnapshot) opts.browserEngine = resolveEngine(opts.browserEngine || undefined);
    const url = isHttpUrl(opts.input);
    if (opts.batchDir) {
      if (!fs.statSync(opts.batchDir, { throwIfNoEntry: false })?.isDirectory()) {
        throw new ImportRequestError(`input directory not found: ${opts.batchDir}`, 1);
      }
      directoryDestination(opts.outputDir!);
      collectBatchInputs(opts.batchDir, opts.outputDir);
      return opts;
    }
    if (!url && !fs.statSync(opts.input, { throwIfNoEntry: false })?.isFile()) {
      throw new ImportRequestError(`input not found: ${opts.input}`, 1);
    }
    for (const value of opts.cookies || []) parseCookie('--html-cookie', value, invalid);
    for (const value of opts.headers || []) parseHeader('--html-header', value, invalid);
    let names: string[] = [''];
    if (opts.scenario) {
      const scenario = parseScenario(JSON.parse(fs.readFileSync(opts.scenario, 'utf8')));
      names = opts.checkpoint ? [selectCheckpoint(scenario, opts.checkpoint)]
        : scenario.steps.filter(step => step.type === 'capture').map(step => step.name!);
      if (opts.browserEngine !== 'playwright' && scenario.steps.some(step => step.target?.role)) {
        invalid('Role locators require --html-browser-engine playwright');
      }
      if (names.length > 1 && (opts.outputFile || !opts.outputDir)) {
        invalid('multiple checkpoints require --output-dir; --output accepts one result');
      }
    }
    if (!opts.outputFile && !opts.outputDir) {
      if (url) invalid('URL input requires an explicit output');
      const base = path.parse(opts.input).name;
      opts.outputFile = path.join(path.dirname(opts.input), `${base}${names[0] ? '.' + names[0] : ''}.pagx`);
    }
    if (opts.outputFile) {
      opts.outputFile = path.resolve(opts.outputFile);
      opts.outputDir = path.dirname(opts.outputFile);
      opts.outputName = path.parse(opts.outputFile).name;
    } else {
      opts.outputDir = path.resolve(opts.outputDir!);
      opts.outputName ||= url ? '' : path.parse(opts.input).name;
    }
    const base = opts.outputName!;
    if (!base || base === '.' || base === '..' || /[\\/\0]/.test(base)) invalid('invalid --output-name');
    directoryDestination(opts.outputDir);
    if (opts.downloadFonts) directoryDestination(opts.fontDir || path.join(opts.outputDir, `${base}.fonts`));
    if (opts.downloadImages) directoryDestination(opts.imageDir || path.join(opts.outputDir, `${base}.images`));
    const targets: string[] = [];
    for (const name of names) {
      const stem = opts.outputFile ? base : `${base}${name ? '.' + name : ''}`;
      targets.push(opts.outputFile || path.join(opts.outputDir, `${stem}.pagx`));
      if (opts.doRender) targets.push(path.join(opts.outputDir, `${stem}.png`));
      if (opts.keepSubsetHtml) targets.push(path.join(opts.outputDir, `${stem}.subset.html`));
    }
    if (opts.scenario) targets.push(path.join(opts.outputDir, `${base}.captures.json`));
    if (opts.preview) directoryDestination(path.join(opts.outputDir, `${base}.preview`));
    const seen = new Set<string>();
    for (const target of targets) {
      const key = canonical(target);
      if (seen.has(key)) invalid(`output name collision: ${target}`);
      seen.add(key);
      if ((!url && key === canonical(opts.input)) || (opts.scenario && key === canonical(opts.scenario))) {
        invalid(`output would overwrite an input: ${target}`);
      }
      if (fs.statSync(target, { throwIfNoEntry: false })?.isDirectory()) invalid(`output must be a file: ${target}`);
    }
    return opts;
  } catch (error) {
    if (error instanceof ImportRequestError) throw error;
    throw new ImportRequestError(error instanceof Error ? error.message : String(error));
  }
}

export function importNeedsBrowser(opts: ImportRequest): boolean {
  return opts.format === 'html' && !opts.htmlNoSnapshot && !opts.dryRun;
}

export function preflightImport(opts: ImportRequest): void {
  if (opts.dryRun) return;
  try {
    if (!fs.statSync(opts.pagxBin).isFile()) throw new Error('not a file');
    fs.accessSync(opts.pagxBin, process.platform === 'win32' ? fs.constants.F_OK : fs.constants.X_OK);
  } catch (_) { throw new ImportRequestError(`native pagx binary is unavailable: ${opts.pagxBin}`, 1); }
}

// Low-level snapshot/server APIs keep their own vocabulary; diagnostics at this boundary use
// only the public import names. No aliases or old CLI arguments are accepted.
export function importErrorMessage(error: unknown): string {
  let message = error instanceof Error ? error.message : String(error);
  for (const flag of ['scenario', 'checkpoint', 'browser-engine', 'capture-animations', 'scroll-reveal',
    'viewport-width', 'viewport-height', 'wait-ms', 'selector', 'download-fonts', 'embed-fonts',
    'font-dir', 'download-images', 'image-dir', 'cookie', 'header', 'batch']) {
    message = message.replace(new RegExp(`(?<![\\w-])--${flag}(?![\\w-])`, 'g'), `--html-${flag}`);
  }
  return message;
}

export async function runImportRequest(opts: ImportRequest, scriptDir: string): Promise<number> {
  validateImportRequest(opts);
  preflightImport(opts);
  if (importNeedsBrowser(opts)) checkBrowserRuntime(opts.browserEngine);
  const log = (line: string) => process.stderr.write(line + '\n');
  return withCliLifecycle(async () => {
    if (opts.batchDir) {
      const result = await runBatch({ ...opts, inputDir: opts.batchDir, outputRoot: opts.outputDir, scriptDir, log });
      return result.failed ? 1 : 0;
    }
    if (opts.scenario) {
      await runScenarioToPagx({ ...opts, scriptDir, log });
      return 0;
    }
    const options = { ...opts, scriptDir, log };
    const result = opts.format === 'html' && !opts.htmlNoSnapshot
      ? await runHtmlToPagx(options) : await convertToArtifacts(options, opts.input);
    if (opts.preview) {
      if (!result.png) throw new Error('preview requires a rendered PNG');
      const base = path.parse(result.pagx).name;
      const preview = await generateContactSheet([{ name: base, png: result.png }], {
        outputDir: path.join(path.dirname(result.pagx), `${base}.preview`),
        columns: opts.previewColumns, pagxBin: opts.pagxBin, log,
      });
      for (const page of preview.pages) log(`[done] ${page.file}`);
    }
    return 0;
  });
}
