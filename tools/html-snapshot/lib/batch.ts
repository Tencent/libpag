// Recursive batch driver for `html2pagx`. Replaces the old
// `batch-html2pagx.sh` bash wrapper. The bash wrapper forked a fresh
// `node html2pagx` per file, which meant Chromium was launched once per
// HTML — the dominant cost on small inputs.
//
// This implementation launches one browser, then loops over the matched
// files calling `runHtmlToPagx({ snapshotImpl: in-process })`. The
// in-process snapshot reuses that single browser via the same
// `lib/snapshot-runner.ts` path that `server.js` uses for its long-lived
// HTTP service.

import * as fs from 'fs';
import * as path from 'path';
import { pathToFileURL } from 'url';
import { throwIfCancelled } from './cli-lifecycle';
import { writeFileAtomic } from './atomic-write';
import { generateContactSheet, validatePreviewOptions, type ContactSheetItem, type ContactSheetResult } from './contact-sheet';

import { launchBrowser, type EngineWrapper } from './browser-engine';
import { runSnapshot } from './snapshot-runner';
import { errMessage } from './common';
import { parseCookie, parseHeader } from './cli';
import {
  PipelineStepError,
  runHtmlToPagx,
  isHttpUrl,
  type BuildSnapshotArgsOptions,
  type SpawnCaptureResult,
  type SnapshotImpl,
} from './pipeline';

// Walk `root` recursively and collect every regular file whose name ends
// with `.html` or `.htm`, dropping `.subset.html` / `.subset.htm` outputs.
export function collectHtmlInputs(root: string, excludedDirectory?: string): string[] {
  const out: string[] = [];
  const stack: string[] = [root];
  while (stack.length > 0) {
    const dir = stack.pop()!;
    if (dir === excludedDirectory) continue;
    let entries;
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch (_err) {
      continue;
    }
    for (const entry of entries) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        stack.push(full);
        continue;
      }
      if (!entry.isFile()) continue;
      const lower = entry.name.toLowerCase();
      if (lower.endsWith('.subset.html') || lower.endsWith('.subset.htm')) continue;
      if (lower.endsWith('.html') || lower.endsWith('.htm')) {
        out.push(full);
      }
    }
  }
  out.sort();
  return out;
}

// Translate an absolute input path under `inputRoot` into the directory
// where its outputs should be written.
function resolveOutputDir(file: string, inputRoot: string, outputRoot: string): string {
  const rel = path.relative(inputRoot, file);
  const relDir = path.dirname(rel);
  if (!outputRoot) {
    return path.dirname(file);
  }
  if (relDir === '.' || relDir === '') return outputRoot;
  return path.join(outputRoot, relDir);
}

// Strip both the extension and a trailing `.htm` from a basename so
// `foo.html` and `foo.htm` both yield `foo`.
function basenameStem(file: string): string {
  let stem = path.basename(file, path.extname(file));
  if (stem.endsWith('.htm')) stem = stem.slice(0, -'.htm'.length);
  return stem;
}

/** Validate batch destinations before browser installation or output creation. */
export function collectBatchInputs(root: string, outputRoot = ''): string[] {
  const inputDir = path.resolve(root);
  const output = outputRoot ? path.resolve(outputRoot) : '';
  const files = collectHtmlInputs(inputDir, output && output !== inputDir ? output : undefined);
  const destinations = new Set<string>();
  for (const file of files) {
    const target = path.join(resolveOutputDir(file, inputDir, output), `${basenameStem(file)}.pagx`);
    const key = process.platform === 'win32' || process.platform === 'darwin'
      ? target.normalize('NFC').toLowerCase() : target;
    if (destinations.has(key)) throw new Error(`batch output name collision: ${target}`);
    destinations.add(key);
  }
  return files;
}

// Build the in-process snapshot adapter. The returned function has the
// same `(snapshotArgs) => SpawnCaptureResult` shape as
// `pipeline.forkSnapshotCli`, but instead of forking `snapshot.js` it calls
// `lib/snapshot-runner.ts` against the long-lived `engineHandle`.
function makeInProcessSnapshot(engineHandle: EngineWrapper): SnapshotImpl {
  return async function inProcessSnapshot(
    snapshotArgs: BuildSnapshotArgsOptions,
  ): Promise<SpawnCaptureResult> {
    const startedAt = Date.now();
    const targetUrl = isHttpUrl(snapshotArgs.input)
      ? snapshotArgs.input
      : pathToFileURL(path.resolve(snapshotArgs.input)).href;
    const stderrChunks: string[] = [];
    const log = (msg: string) => { stderrChunks.push(`html-snapshot: ${msg}`); };
    const fail = (message: string): never => { throw new Error(message); };
    const cookieParams = (snapshotArgs.cookies || []).map(value => parseCookie('--cookie', value, fail));
    const headerPairs = (snapshotArgs.headers || []).map(value => parseHeader('--header', value, fail));
    const { engine, browser } = engineHandle;
    const context = engine === 'playwright' ? await browser.newContext() : await browser.createBrowserContext();
    try {
      const result = await runSnapshot({ engine, browser: context }, targetUrl, {
        viewportWidth: snapshotArgs.viewportWidth,
        viewportHeight: snapshotArgs.viewportHeight,
        waitMs: snapshotArgs.waitMs,
        selector: snapshotArgs.selector,
        cookies: cookieParams,
        headers: headerPairs,
        inlineIconFonts: snapshotArgs.inlineIconFonts,
        captureAnimations: snapshotArgs.captureAnimations,
        reducedMotion: !snapshotArgs.captureAnimations,
        scrollReveal: snapshotArgs.scrollReveal,
        downloadFonts: snapshotArgs.downloadFonts,
        fontDir: snapshotArgs.fontDir,
        downloadImages: snapshotArgs.downloadImages,
        imageDir: snapshotArgs.imageDir,
        log,
      });
      await writeFileAtomic(snapshotArgs.output, result.html, { reuseExisting: false });
      const stderr = stderrChunks.join('\n');
      if (stderr) process.stderr.write(stderr + '\n');
      return { code: 0, durationMs: Date.now() - startedAt, stdout: '', stderr: '',
        fontFiles: (result.fonts || []).map(font => font.path) };
    } catch (err) {
      const stderr = stderrChunks.concat([errMessage(err)]).join('\n');
      return { code: 1, durationMs: Date.now() - startedAt, stdout: '', stderr };
    } finally { await context.close(); }
  };
}

export interface RunBatchOptions {
  svgOptions?: string[];
  verbose?: boolean;
  pagxImageBaseDir?: string;
  log?: (line: string) => void;
  inputDir: string;
  scriptDir: string;
  pagxBin?: string;
  outputRoot?: string;
  skipExisting?: boolean;
  dryRun?: boolean;
  browserEngine?: string;
  scale?: number;
  doResolve?: boolean;
  doRender?: boolean;
  keepSubsetHtml?: boolean;
  embedFonts?: boolean;
  downloadFonts?: boolean;
  fontDir?: string;
  downloadImages?: boolean;
  imageDir?: string;
  pagxImages?: 'external' | 'embed';
  inlineIconFonts?: boolean;
  captureAnimations?: boolean;
  scrollReveal?: boolean;
  cookies?: string[];
  headers?: string[];
  viewportWidth?: number;
  viewportHeight?: number;
  waitMs?: number;
  selector?: string;
  preview?: boolean;
  previewColumns?: number;
}

export interface RunBatchResult {
  processed: number;
  skipped: number;
  failed: number;
  total: number;
  preview?: ContactSheetResult;
}

// A PAGX alone does not establish that a previous run produced the requested PNG, fonts
// or subset. Cache only complete results with the same source, native binary and options.
function batchSignature(opts: RunBatchOptions, input: string): string {
  const stamp = (file: string | undefined) => {
    const stat = file ? fs.statSync(file, { throwIfNoEntry: false }) : undefined;
    return stat ? [stat.size, stat.mtimeMs] : null;
  };
  const { log: _log, dryRun: _dryRun, skipExisting: _skip, preview: _preview,
    previewColumns: _columns, ...settings } = opts;
  return JSON.stringify({ input: path.resolve(input), source: stamp(input), binary: stamp(opts.pagxBin), settings });
}

type ArtifactStamp = { file: string; size: number; mtimeMs: number };
function artifactStamps(files: string[]): ArtifactStamp[] {
  return [...new Set(files)].map(file => {
    const stat = fs.statSync(file);
    if (!stat.isFile()) throw new Error(`artifact is not a file: ${file}`);
    return { file, size: stat.size, mtimeMs: stat.mtimeMs };
  });
}
function completeBatchResult(record: string, signature: string): boolean {
  try {
    const data = JSON.parse(fs.readFileSync(record, 'utf8'));
    return data.status === 'complete' && data.signature === signature && data.artifacts.length > 0 &&
      data.artifacts.every((item: ArtifactStamp) => {
        const stat = fs.statSync(item.file, { throwIfNoEntry: false });
        return stat?.isFile() && stat.size === item.size && stat.mtimeMs === item.mtimeMs;
      });
  } catch (_) { return false; }
}
function pagxResourceFiles(pagx: string): string[] {
  const xml = fs.readFileSync(pagx, 'utf8');
  const entities: Record<string, string> = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'" };
  return [...xml.matchAll(/\bsource="([^"]+)"/g)].map(match => match[1]
    .replace(/&(amp|lt|gt|quot|apos);/g, (_, name: string) => entities[name]))
    .filter(source => source && !/^(data:|https?:)/i.test(source))
    .map(source => path.resolve(path.dirname(pagx), source));
}

// Run the batch.
export async function runBatch(opts: RunBatchOptions): Promise<RunBatchResult> {
  throwIfCancelled();
  validatePreviewOptions(opts);
  const log = opts.log || ((line: string) => process.stdout.write(line + '\n'));
  const inputDir = opts.inputDir ? path.resolve(opts.inputDir) : '';
  if (!inputDir) throw new Error('runBatch: inputDir is required');
  if (!fs.existsSync(inputDir) || !fs.statSync(inputDir).isDirectory()) {
    throw new Error(`runBatch: input directory not found: ${inputDir}`);
  }
  let outputRoot = opts.outputRoot || '';
  if (outputRoot) outputRoot = path.resolve(outputRoot);

  const files = collectBatchInputs(inputDir, outputRoot);
  const total = files.length;
  if (total === 0) {
    log(`No non-subset HTML files found under: ${inputDir}`);
    return { processed: 0, skipped: 0, failed: 0, total: 0 };
  }
  log(`Found ${total} non-subset HTML files under: ${inputDir}`);

  // Dry run never opens the browser.
  if (opts.dryRun) {
    let processed = 0;
    let skipped = 0;
    files.forEach((file, idx) => {
      const rel = path.relative(inputDir, file);
      const outDir = resolveOutputDir(file, inputDir, outputRoot);
      const stem = basenameStem(file);
      const target = path.join(outDir, `${stem}.pagx`);
      const index = idx + 1;
      if (opts.skipExisting && completeBatchResult(path.join(outDir, `${stem}.import.json`), batchSignature(opts, file))) {
        log(`[${index}/${total}] skip      ${rel}`);
        skipped++;
        return;
      }
      log(`[${index}/${total}] would process ${rel}`);
      processed++;
    });
    log(`Summary: processed=${processed} skipped=${skipped} failed=0 total=${total}`);
    return { processed, skipped, failed: 0, total };
  }

  // Launch Chromium once.
  const engineHandle = await launchBrowser({ engine: opts.browserEngine });
  const snapshotImpl = makeInProcessSnapshot(engineHandle);

  let processed = 0;
  let skipped = 0;
  let failed = 0;
  const previewItems: ContactSheetItem[] = [];

  try {
    for (let i = 0; i < total; i++) {
      throwIfCancelled();
      const file = files[i];
      const rel = path.relative(inputDir, file);
      const index = i + 1;
      const outDir = resolveOutputDir(file, inputDir, outputRoot);
      const stem = basenameStem(file);
      const record = path.join(outDir, `${stem}.import.json`);
      const signature = batchSignature(opts, file);

      if (opts.skipExisting && completeBatchResult(record, signature)) {
        log(`[${index}/${total}] skip      ${rel}`);
        skipped++;
        continue;
      }

      fs.mkdirSync(outDir, { recursive: true });
      log(`[${index}/${total}] process   ${rel}`);

      try {
        await writeFileAtomic(record, JSON.stringify({ version: 1, status: 'running', signature }), { reuseExisting: false });
        const result = await runHtmlToPagx({
          input: file,
          outputDir: outDir,
          outputName: stem,
          pagxBin: opts.pagxBin,
          scriptDir: opts.scriptDir,
          scale: opts.scale,
          doResolve: opts.doResolve,
          doRender: opts.doRender,
          keepSubsetHtml: opts.keepSubsetHtml,
          embedFonts: opts.embedFonts,
          downloadFonts: opts.downloadFonts,
          fontDir: opts.fontDir,
          downloadImages: opts.downloadImages,
          imageDir: opts.imageDir,
          pagxImages: opts.pagxImages,
          pagxImageBaseDir: opts.pagxImageBaseDir,
          svgOptions: opts.svgOptions,
          verbose: opts.verbose,
          browserEngine: opts.browserEngine,
          inlineIconFonts: opts.inlineIconFonts,
          captureAnimations: opts.captureAnimations,
          scrollReveal: opts.scrollReveal,
          cookies: opts.cookies,
          headers: opts.headers,
          viewportWidth: opts.viewportWidth,
          viewportHeight: opts.viewportHeight,
          waitMs: opts.waitMs,
          selector: opts.selector,
          log,
          snapshotImpl,
        });
        if (opts.preview) {
          if (!result.png) throw new Error(`preview requires a rendered PNG for ${rel}`);
          previewItems.push({ name: rel.split(path.sep).join('/'), png: result.png });
        }
        const files = [result.pagx, ...result.fonts, ...pagxResourceFiles(result.pagx)];
        if (result.png) files.push(result.png);
        if (opts.keepSubsetHtml !== false) files.push(result.subsetHtml);
        await writeFileAtomic(record, JSON.stringify({ version: 1, status: 'complete', signature,
          artifacts: artifactStamps(files) }), { reuseExisting: false });
        processed++;
      } catch (err) {
        await writeFileAtomic(record, JSON.stringify({ version: 1, status: 'failed', signature,
          error: errMessage(err) }), { reuseExisting: false });
        throwIfCancelled();
        failed++;
        const msg = err instanceof PipelineStepError
          ? `${err.step} failed`
          : errMessage(err);
        process.stderr.write(`  failed: ${rel}: ${msg}\n`);
      }
    }
  } finally {
    try {
      await engineHandle.browser.close();
    } catch (closeErr) {
      process.stderr.write(`browser close error: ${errMessage(closeErr)}\n`);
    }
  }

  throwIfCancelled();
  let preview: ContactSheetResult | undefined;
  if (opts.preview) {
    preview = previewItems.length ? await generateContactSheet(previewItems, {
      outputDir: path.join(outputRoot || inputDir, 'preview'), columns: opts.previewColumns,
      pagxBin: opts.pagxBin, log,
    }) : { pages: [] };
    for (const page of preview.pages) log(`[done] ${page.file}`);
  }
  log(`Summary: processed=${processed} skipped=${skipped} failed=${failed} total=${total}`);
  return { processed, skipped, failed, total, ...(preview ? { preview } : {}) };
}
