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

import { launchBrowser, type EngineWrapper } from './browser-engine';
import { runSnapshot } from './snapshot-runner';
import { errMessage } from './common';
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
export function collectHtmlInputs(root: string): string[] {
  const out: string[] = [];
  const stack: string[] = [root];
  while (stack.length > 0) {
    const dir = stack.pop()!;
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
      : `file://${path.resolve(snapshotArgs.input)}`;
    const stderrChunks: string[] = [];
    const log = (msg: string) => { stderrChunks.push(`html-snapshot: ${msg}`); };
    try {
      const cookieParams = (snapshotArgs.cookies || []).map((c) => {
        const eq = c.indexOf('=');
        return eq > 0
          ? { name: c.slice(0, eq), value: c.slice(eq + 1) }
          : { name: c, value: '' };
      });
      const headerPairs: Array<[string, string]> = (snapshotArgs.headers || []).map((h) => {
        const colon = h.indexOf(':');
        return colon > 0
          ? [h.slice(0, colon).trim(), h.slice(colon + 1).trim()] as [string, string]
          : [h, ''] as [string, string];
      });
      const result = await runSnapshot(engineHandle, targetUrl, {
        viewportWidth: snapshotArgs.viewportWidth,
        viewportHeight: snapshotArgs.viewportHeight,
        waitMs: snapshotArgs.waitMs,
        selector: snapshotArgs.selector,
        cookies: cookieParams,
        headers: headerPairs,
        inlineIconFonts: snapshotArgs.inlineIconFonts,
        scrollReveal: snapshotArgs.scrollReveal,
        downloadFonts: snapshotArgs.downloadFonts,
        fontDir: snapshotArgs.fontDir,
        downloadImages: snapshotArgs.downloadImages,
        imageDir: snapshotArgs.imageDir,
        runtimeAnimWindowMs: snapshotArgs.runtimeAnimWindowMs,
        runtimeAnimSampleCount: snapshotArgs.runtimeAnimSampleCount,
        log,
      });
      fs.writeFileSync(snapshotArgs.output, result.html, 'utf8');
      const stderr = stderrChunks.join('\n');
      if (stderr) process.stderr.write(stderr + '\n');
      return { code: 0, durationMs: Date.now() - startedAt, stdout: '', stderr: '' };
    } catch (err) {
      const stderr = stderrChunks.concat([errMessage(err)]).join('\n');
      return { code: 1, durationMs: Date.now() - startedAt, stdout: '', stderr };
    }
  };
}

export interface RunBatchOptions {
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
  inferFlex?: boolean;
  keepSubsetHtml?: boolean;
  embedFonts?: boolean;
  downloadFonts?: boolean;
  fontDir?: string;
  downloadImages?: boolean;
  imageDir?: string;
  inlineIconFonts?: boolean;
  scrollReveal?: boolean;
  runtimeAnimWindowMs?: number;
  runtimeAnimSampleCount?: number;
  cookies?: string[];
  headers?: string[];
  viewportWidth?: number;
  viewportHeight?: number;
  waitMs?: number;
  selector?: string;
}

export interface RunBatchResult {
  processed: number;
  skipped: number;
  failed: number;
  total: number;
}

// Run the batch.
export async function runBatch(opts: RunBatchOptions): Promise<RunBatchResult> {
  const inputDir = opts.inputDir ? path.resolve(opts.inputDir) : '';
  if (!inputDir) throw new Error('runBatch: inputDir is required');
  if (!fs.existsSync(inputDir) || !fs.statSync(inputDir).isDirectory()) {
    throw new Error(`runBatch: input directory not found: ${inputDir}`);
  }
  let outputRoot = opts.outputRoot || '';
  if (outputRoot) {
    fs.mkdirSync(outputRoot, { recursive: true });
    outputRoot = path.resolve(outputRoot);
  }

  const files = collectHtmlInputs(inputDir);
  const total = files.length;
  if (total === 0) {
    process.stdout.write(`No non-subset HTML files found under: ${inputDir}\n`);
    return { processed: 0, skipped: 0, failed: 0, total: 0 };
  }
  process.stdout.write(`Found ${total} non-subset HTML files under: ${inputDir}\n`);

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
      if (opts.skipExisting && fs.existsSync(target)) {
        process.stdout.write(`[${index}/${total}] skip      ${rel}\n`);
        skipped++;
        return;
      }
      process.stdout.write(`[${index}/${total}] would process ${rel}\n`);
      processed++;
    });
    process.stdout.write(`Summary: processed=${processed} skipped=${skipped} failed=0 total=${total}\n`);
    return { processed, skipped, failed: 0, total };
  }

  // Launch Chromium once.
  const engineHandle = await launchBrowser({ engine: opts.browserEngine });
  const snapshotImpl = makeInProcessSnapshot(engineHandle);

  let processed = 0;
  let skipped = 0;
  let failed = 0;

  try {
    for (let i = 0; i < total; i++) {
      const file = files[i];
      const rel = path.relative(inputDir, file);
      const index = i + 1;
      const outDir = resolveOutputDir(file, inputDir, outputRoot);
      const stem = basenameStem(file);
      const pagxTarget = path.join(outDir, `${stem}.pagx`);

      if (opts.skipExisting && fs.existsSync(pagxTarget)) {
        process.stdout.write(`[${index}/${total}] skip      ${rel}\n`);
        skipped++;
        continue;
      }

      fs.mkdirSync(outDir, { recursive: true });
      process.stdout.write(`[${index}/${total}] process   ${rel}\n`);

      try {
        await runHtmlToPagx({
          input: file,
          outputDir: outDir,
          outputName: stem,
          pagxBin: opts.pagxBin,
          scriptDir: opts.scriptDir,
          scale: opts.scale,
          doResolve: opts.doResolve,
          doRender: opts.doRender,
          inferFlex: opts.inferFlex,
          keepSubsetHtml: opts.keepSubsetHtml,
          embedFonts: opts.embedFonts,
          downloadFonts: opts.downloadFonts,
          fontDir: opts.fontDir,
          downloadImages: opts.downloadImages,
          imageDir: opts.imageDir,
          browserEngine: opts.browserEngine,
          inlineIconFonts: opts.inlineIconFonts,
          scrollReveal: opts.scrollReveal,
          runtimeAnimWindowMs: opts.runtimeAnimWindowMs,
          runtimeAnimSampleCount: opts.runtimeAnimSampleCount,
          cookies: opts.cookies,
          headers: opts.headers,
          viewportWidth: opts.viewportWidth,
          viewportHeight: opts.viewportHeight,
          waitMs: opts.waitMs,
          selector: opts.selector,
          log: (line: string) => process.stdout.write(line + '\n'),
          snapshotImpl,
        });
        processed++;
      } catch (err) {
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

  process.stdout.write(`Summary: processed=${processed} skipped=${skipped} failed=${failed} total=${total}\n`);
  return { processed, skipped, failed, total };
}
