// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
// Private, versioned protocol for the native import workflow. No public CLI parsing,
// native import subprocesses, or final PAGX output ownership belongs here.
import * as fs from 'fs';
import * as path from 'path';
import { pathToFileURL } from 'url';
import { parseScenario, selectCheckpoint, runScenario, type Checkpoint } from './interaction';
import { runSnapshot } from './snapshot-runner';
import { launchBrowser, type EngineName } from './browser-engine';
import { throwIfCancelled } from './cli-lifecycle';
import { writeFileAtomic } from './atomic-write';
import { generateContactSheet, validateContactSheetColumns } from './contact-sheet';

export interface ImportAdapterRequest {
  version: 1;
  operation: 'inspect' | 'capture' | 'preview';
  inputs?: Array<{ id: string; input: string }>;
  workDir?: string;
  options?: {
    scenario?: string; checkpoint?: string; browserEngine?: EngineName;
    viewportWidth?: number; viewportHeight?: number; waitMs?: number; selector?: string;
    cookies?: string[]; headers?: string[]; captureAnimations?: boolean; scrollReveal?: boolean;
    inlineIconFonts?: boolean; downloadFonts?: boolean;
  };
  preview?: { items: Array<{ name: string; png: string }>; outputDir: string; columns?: number; pagxBin: string };
}

export function validateImportRequest(raw: unknown) {
  if (!raw || typeof raw !== 'object') throw new Error('import request must be an object');
  const req = raw as ImportAdapterRequest;
  if (req.version !== 1) throw new Error('unsupported import protocol version');
  if (!['inspect', 'capture', 'preview'].includes(req.operation)) throw new Error('invalid import operation');
  if (req.operation === 'preview') {
    if (!req.preview || !Array.isArray(req.preview.items) || !req.preview.items.length ||
        !req.preview.outputDir || !req.preview.pagxBin) throw new Error('invalid preview request');
    validateContactSheetColumns(req.preview.columns);
    return { req, options: {}, scenario: null, cookies: [], headers: [] };
  }
  const options = req.options || {};
  const engine = options.browserEngine || 'puppeteer';
  if (engine !== 'puppeteer' && engine !== 'playwright') throw new Error('unsupported browser engine');
  if (!Array.isArray(req.inputs) || !req.inputs.length) throw new Error('import request needs inputs');
  const ids = new Set<string>();
  for (const source of req.inputs) {
    if (!source || typeof source.id !== 'string' || !/^[a-zA-Z0-9_-]+$/.test(source.id) || ids.has(source.id))
      throw new Error('invalid or duplicate import source id');
    ids.add(source.id);
    if (typeof source.input !== 'string' || !source.input) throw new Error('invalid import input');
    if (!/^https?:\/\//.test(source.input) && !fs.statSync(source.input, { throwIfNoEntry: false })?.isFile())
      throw new Error(`input not found: ${source.input}`);
  }
  const cookies = (options.cookies || []).map(value => {
    const at = value.indexOf('=');
    if (at < 1 || /[\r\n\0]/.test(value)) throw new Error('--cookie expects name=value');
    return { name: value.slice(0, at), value: value.slice(at + 1) };
  });
  const headers: Array<[string, string]> = (options.headers || []).map(value => {
    const at = value.indexOf(':');
    if (at < 1 || !/^[!#$%&'*+.^_`|~\w-]+$/.test(value.slice(0, at)) || /[\r\n\0]/.test(value))
      throw new Error('--header expects Key: Value');
    return [value.slice(0, at), value.slice(at + 1).trim()];
  });
  if ((cookies.length || headers.length) && req.inputs.some(source => !/^https?:\/\//.test(source.input)))
    throw new Error('cookies/headers require URL inputs');
  const scenario = options.scenario ? parseScenario(JSON.parse(fs.readFileSync(options.scenario, 'utf8'))) : null;
  if (options.checkpoint && !scenario) throw new Error('checkpoint requires a scenario');
  if (scenario) {
    if (req.inputs.length !== 1) throw new Error('a scenario requires exactly one input');
    if (options.checkpoint) selectCheckpoint(scenario, options.checkpoint);
    if (engine !== 'playwright' && scenario.steps.some(step => step.target?.role))
      throw new Error('Role locators require --browser-engine playwright');
  }
  if (req.operation === 'capture' && (!req.workDir || !path.isAbsolute(req.workDir)))
    throw new Error('capture requires an absolute work directory');
  return { req, options, scenario, cookies, headers };
}

interface PreparedSource {
  id: string; checkpoint: string; snapshot: string; url: string; baseDir: string;
  width: number; height: number; warnings: string[]; fonts: string[];
}

export async function runImportAdapter(raw: unknown, responseFile: string): Promise<number> {
  const { req, options, scenario, cookies, headers } = validateImportRequest(raw);
  const write = (value: unknown) => writeFileAtomic(responseFile, JSON.stringify(value, null, 2) + '\n', { reuseExisting: false });
  if (req.operation === 'inspect') {
    await write({ version: 1, status: 'complete', checkpoints: scenario?.steps.filter(s => s.type === 'capture').map(s => s.name) || [] });
    return 0;
  }
  if (req.operation === 'preview') {
    const preview = req.preview!;
    const result = await generateContactSheet(preview.items, { ...preview,
      log: line => process.stderr.write(`pagx import: ${line}\n`) });
    await write({ version: 1, status: 'complete', pages: result.pages });
    return 0;
  }
  const response: { version: number; status: string; outputs: PreparedSource[]; errors: Array<{ id: string; error: string }> } =
    { version: 1, status: 'running', outputs: [], errors: [] };
  await write(response);
  const engine = await launchBrowser({ engine: options.browserEngine });
  try {
    for (const source of req.inputs!) {
      throwIfCancelled();
      const sourceDir = path.join(req.workDir!, source.id);
      await fs.promises.mkdir(sourceDir, { recursive: true });
      const url = /^https?:\/\//.test(source.input) ? source.input : pathToFileURL(path.resolve(source.input)).href;
      const warnings: string[] = [];
      const log = (message: string) => {
        process.stderr.write(`pagx import: ${message}\n`);
        if (/warn|failed|unsupported/i.test(message)) warnings.push(message);
      };
      const save = async (result: { html: string; width: number; height: number; fonts?: Array<{ path: string }>;
        name?: string; url?: string; warnings?: string[] }) => {
        throwIfCancelled();
        const snapshot = path.join(sourceDir, `${result.name || 'document'}.html`);
        await writeFileAtomic(snapshot, result.html, { reuseExisting: false });
        response.outputs.push({ id: source.id, checkpoint: result.name || '', snapshot,
          url: result.url || url, width: result.width, height: result.height,
          baseDir: /^https?:\/\//.test(source.input) ? '' : path.dirname(path.resolve(source.input)),
          warnings: [...warnings, ...(result.warnings || [])], fonts: (result.fonts || []).map(f => f.path) });
        await write(response);
      };
      const runOptions = { ...options, cookies, headers, log, fontDir: path.join(sourceDir, 'fonts') };
      try {
        if (scenario) {
          await runScenario(engine, url, scenario, { ...runOptions, onCheckpoint: (result: Checkpoint) => save(result) });
        } else {
          // Puppeteer's default newPage shares cookies/storage. Isolate every source,
          // including ordinary HTML batch jobs, while reusing the browser process.
          const context = engine.engine === 'playwright' ? await engine.browser.newContext() : await engine.browser.createBrowserContext();
          try {
            await save(await runSnapshot({ engine: engine.engine, browser: context }, url,
              { ...runOptions, reducedMotion: !options.captureAnimations }));
          } finally { await context.close(); }
        }
      } catch (error) {
        throwIfCancelled();
        response.errors.push({ id: source.id, error: error instanceof Error ? error.message : String(error) });
        await write(response);
      }
    }
    response.status = response.errors.length ? 'failed' : 'complete';
    await write(response);
    return response.errors.length ? 1 : 0;
  } catch (error) {
    response.status = 'failed';
    response.errors.push({ id: '', error: error instanceof Error ? error.message : String(error) });
    await write(response);
    throw error;
  } finally { await engine.browser.close(); }
}
