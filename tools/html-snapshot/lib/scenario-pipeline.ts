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

/** Replay once, then convert each captured state with the existing native pipeline. */
export async function runScenarioToPagx(opts: RunHtmlToPagxOptions): Promise<void> {
  if (!opts.input || !opts.scenario) throw new Error('input and scenario are required');
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
      scenario, { ...opts, cookies, headers, fontDir, imageDir });
  } finally { await engine.browser.close(); }

  const outputs = [];
  for (const result of checkpoints) {
    for (const warning of result.warnings) opts.log?.(`${result.name}: ${warning}`);
    const files = await runHtmlToPagx({
      ...opts, fontDir, imageDir, outputDir, outputName: `${base}.${result.name}`,
      snapshotImpl: async args => {
        await writeFileAtomic(args.output, result.html);
        return { code: 0, durationMs: 0, stdout: '', stderr: '' };
      },
    });
    outputs.push({ name: result.name, url: result.url, width: result.width, height: result.height,
      warnings: result.warnings, ...files, subsetHtml: opts.keepSubsetHtml === false ? null : files.subsetHtml });
  }
  const manifest = path.join(outputDir, `${base}.captures.json`);
  await writeFileAtomic(manifest, JSON.stringify({ version: 1, scenario, outputs }, null, 2) + '\n');
  opts.log?.(`[done] ${manifest}`);
}
