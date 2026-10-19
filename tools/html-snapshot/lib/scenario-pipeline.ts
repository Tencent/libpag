// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.

import * as fs from 'fs';
import * as path from 'path';
import { pathToFileURL } from 'url';
import { launchBrowser, type EngineName } from './browser-engine';
import { parseCookie, parseHeader } from './cli';
import { parseScenario, selectCheckpoint, runScenario } from './interaction';
import { runHtmlToPagx, isHttpUrl, type RunHtmlToPagxOptions, type RunHtmlToPagxResult } from './pipeline';
import { throwIfCancelled } from './cli-lifecycle';
import { writeFileAtomic } from './atomic-write';
import { generateContactSheet, validateContactSheetColumns, type ContactSheetPage } from './contact-sheet';

export interface RunScenarioToPagxOptions extends RunHtmlToPagxOptions {
  preview?: boolean;
  previewColumns?: number;
}

type PreviewStatus =
  | { status: 'pending'; pages: [] }
  | { status: 'complete'; pages: ContactSheetPage[] }
  | { status: 'failed'; pages: []; error: string };

export interface ScenarioExport {
  version: number;
  status: 'running' | 'complete' | 'failed';
  scenario: ReturnType<typeof parseScenario>;
  outputs: (Omit<RunHtmlToPagxResult, 'subsetHtml'> & {
    name: string; url: string; width: number; height: number;
    warnings: string[]; subsetHtml: string | null;
  })[];
  preview?: PreviewStatus;
  error?: string;
}

/** Replay once, then convert each captured state with the existing native pipeline. */
export async function runScenarioToPagx(opts: RunScenarioToPagxOptions): Promise<ScenarioExport> {
  if (!opts.input || !opts.scenario) throw new Error('input and scenario are required');
  if (opts.previewColumns !== undefined && !opts.preview) throw new Error('--preview-columns requires --preview');
  validateContactSheetColumns(opts.previewColumns);
  if (opts.preview && (opts.doRender === false || opts.doResolve === false)) {
    throw new Error('--preview requires rendering; it cannot be combined with --no-render or --no-resolve');
  }
  const scenario = parseScenario(JSON.parse(fs.readFileSync(opts.scenario, 'utf8')));
  if (opts.checkpoint) selectCheckpoint(scenario, opts.checkpoint);
  const isUrl = isHttpUrl(opts.input);
  if (isUrl && !opts.outputName) throw new Error('--output-name is required for URL input');
  if (!isUrl && (opts.cookies?.length || opts.headers?.length)) throw new Error('cookies/headers require a URL input');
  const outputDir = path.resolve(opts.outputDir || (isUrl ? process.cwd() : path.dirname(opts.input)));
  const base = opts.outputName || path.basename(opts.input, path.extname(opts.input));
  const fontDir = path.resolve(opts.fontDir || path.join(outputDir, `${base}.fonts`));
  const imageDir = path.resolve(opts.imageDir || path.join(outputDir, `${base}.images`));
  // Parse credentials before creating browser resources (the CLI parsers can exit).
  const cookies = opts.cookies?.map(value => parseCookie('--cookie', value));
  const headers = opts.headers?.map(value => parseHeader('--header', value));
  await fs.promises.mkdir(outputDir, { recursive: true });
  const manifest = path.join(outputDir, `${base}.captures.json`);
  const data: ScenarioExport = { version: 1, status: 'running', scenario, outputs: [] };
  if (opts.preview) data.preview = { status: 'pending', pages: [] };
  const writeManifest = () => writeFileAtomic(manifest, JSON.stringify(data, null, 2) + '\n', { reuseExisting: false });
  // Invalidate an earlier run before launching the page or replacing any outputs.
  await writeManifest();
  try {
    const engine = await launchBrowser({ engine: opts.browserEngine as EngineName | undefined });
    let checkpoints;
    try {
      checkpoints = await runScenario(engine, isUrl ? opts.input : pathToFileURL(path.resolve(opts.input)).href,
        scenario, { ...opts, downloadFonts: opts.downloadFonts || opts.embedFonts,
          cookies, headers, fontDir, imageDir });
    } finally { await engine.browser.close(); }

    for (const result of checkpoints) {
      throwIfCancelled();
      for (const warning of result.warnings) opts.log?.(`${result.name}: ${warning}`);
      const files = await runHtmlToPagx({
        ...opts, fontDir, imageDir, outputDir, outputName: `${base}.${result.name}`,
        fontFiles: result.fonts?.map(font => font.path),
        snapshotImpl: async args => {
          await writeFileAtomic(args.output, result.html, { reuseExisting: false });
          return { code: 0, durationMs: 0, stdout: '', stderr: '' };
        },
      });
      data.outputs.push({ name: result.name, url: result.url, width: result.width, height: result.height,
        warnings: result.warnings, ...files, subsetHtml: opts.keepSubsetHtml === false ? null : files.subsetHtml });
      await writeManifest();
    }
    if (opts.preview) {
      try {
        const items = data.outputs.map(output => {
          if (!output.png) throw new Error(`checkpoint '${output.name}' has no rendered PNG`);
          return { name: output.name, png: output.png };
        });
        const result = await generateContactSheet(items, {
          outputDir: path.join(outputDir, `${base}.preview`), columns: opts.previewColumns,
          pagxBin: opts.pagxBin, log: opts.log,
        });
        data.preview = { status: 'complete', pages: result.pages.map(page => ({
          ...page, file: path.relative(path.dirname(manifest), page.file).split(path.sep).join('/'),
        })) };
        await writeManifest();
        for (const page of result.pages) opts.log?.(`[done] ${page.file}`);
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        data.preview = { status: 'failed', pages: [], error: message };
        throw new Error(`preview generation failed: ${message}`);
      }
    }
    throwIfCancelled();
    data.status = 'complete';
    await writeManifest();
    opts.log?.(`[done] ${manifest}`);
    return data;
  } catch (error) {
    try { throwIfCancelled(); } catch (cancelled) { error = cancelled; }
    const message = error instanceof Error ? error.message : String(error);
    data.status = 'failed';
    data.error = message;
    if (data.preview?.status === 'pending') data.preview = { status: 'failed', pages: [], error: message };
    try { await writeManifest(); }
    catch (manifestError) { opts.log?.(`[manifest] failed to record failure: ${String(manifestError)}`); }
    throw error;
  }
}
