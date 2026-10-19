// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.

import * as fs from 'fs';
import * as path from 'path';
import { pathToFileURL } from 'url';
import { launchBrowser, type EngineName } from './browser-engine';
import { parseCookie, parseHeader } from './cli';
import { parseScenario, runScenario } from './interaction';
import { runHtmlToPagx, isHttpUrl, type RunHtmlToPagxOptions } from './pipeline';
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

/** Replay once, then convert each captured state with the existing native pipeline. */
export async function runScenarioToPagx(opts: RunScenarioToPagxOptions): Promise<void> {
  if (!opts.input || !opts.scenario) throw new Error('input and scenario are required');
  if (opts.previewColumns !== undefined && !opts.preview) throw new Error('--preview-columns requires --preview');
  validateContactSheetColumns(opts.previewColumns);
  if (opts.preview && (opts.doRender === false || opts.doResolve === false)) {
    throw new Error('--preview requires rendering; it cannot be combined with --no-render or --no-resolve');
  }
  const scenario = parseScenario(JSON.parse(fs.readFileSync(opts.scenario, 'utf8')));
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
  const engine = await launchBrowser({ engine: opts.browserEngine as EngineName | undefined });
  let checkpoints;
  try {
    checkpoints = await runScenario(engine, isUrl ? opts.input : pathToFileURL(path.resolve(opts.input)).href,
      scenario, { ...opts, downloadFonts: opts.downloadFonts || opts.embedFonts,
        cookies, headers, fontDir, imageDir });
  } finally { await engine.browser.close(); }

  const outputs = [];
  for (const result of checkpoints) {
    for (const warning of result.warnings) opts.log?.(`${result.name}: ${warning}`);
    const files = await runHtmlToPagx({
      ...opts, fontDir, imageDir, outputDir, outputName: `${base}.${result.name}`,
      fontFiles: result.fonts?.map(font => font.path),
      snapshotImpl: async args => {
        await writeFileAtomic(args.output, result.html, { reuseExisting: false });
        return { code: 0, durationMs: 0, stdout: '', stderr: '' };
      },
    });
    outputs.push({ name: result.name, url: result.url, width: result.width, height: result.height,
      warnings: result.warnings, ...files, subsetHtml: opts.keepSubsetHtml === false ? null : files.subsetHtml });
  }
  const manifest = path.join(outputDir, `${base}.captures.json`);
  const data: { version: number; scenario: typeof scenario; outputs: typeof outputs; preview?: PreviewStatus } = {
    version: 1, scenario, outputs,
  };
  const writeManifest = () => writeFileAtomic(manifest, JSON.stringify(data, null, 2) + '\n', { reuseExisting: false });
  if (opts.preview) data.preview = { status: 'pending', pages: [] };
  await writeManifest();
  if (opts.preview) {
    try {
      const items = outputs.map(output => {
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
      try { await writeManifest(); }
      catch (manifestError) { opts.log?.(`[preview] failed to update manifest: ${String(manifestError)}`); }
      throw new Error(`preview generation failed: ${message}`);
    }
  }
  opts.log?.(`[done] ${manifest}`);
}
