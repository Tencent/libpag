// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as fs from 'node:fs';
import * as path from 'node:path';
import { createHash } from 'node:crypto';
import { inspectZip, prepareZip, type ZipPlan, type ZipOptions } from './zip-source';
import { startWebsiteServer } from './website-server';
import { startWebsiteProxy, websiteOrigins } from './website-proxy';
import { runBatchInputs, type BatchInput, type BatchInputResult, type RunBatchOptions, type RunBatchResult } from './batch';
import { writeFileAtomic } from './atomic-write';
import { trackCliResource, throwIfCancelled } from './cli-lifecycle';
import { resolveEngine } from './browser-engine';

export interface ArchiveImportOptions extends Omit<RunBatchOptions, 'inputDir'> {
  input: string;
  archiveRoot?: string;
  allowOrigins?: string[];
  outputDir?: string;
  outputFile?: string;
  outputName?: string;
  zipOptions?: Pick<ZipOptions, 'limits'>;
}
export interface ArchivePlan { source: ZipPlan; inputs: BatchInput[]; outputRoot: string; manifest: string }
export interface ArchiveManifest {
  version: 1;
  source: { sha256: string; root: string };
  status: 'planned' | 'running' | 'complete' | 'partial' | 'failed';
  outputs: Array<BatchInputResult & { source: string } | { source: string; status: 'pending' | 'cancelled' }>;
  preview?: RunBatchResult['preview'];
  error?: string;
}

export function isZipInput(input: string): boolean { return !/^https?:/i.test(input) && /\.zip$/i.test(input); }

function pathKey(file: string): string {
  let existing = path.resolve(file);
  const suffix: string[] = [];
  while (!fs.existsSync(existing) && path.dirname(existing) !== existing) {
    suffix.unshift(path.basename(existing)); existing = path.dirname(existing);
  }
  return path.join(fs.realpathSync(existing), ...suffix).normalize('NFC').toLowerCase();
}

export function planArchiveImport(opts: ArchiveImportOptions, source: ZipPlan): ArchivePlan {
  websiteOrigins(opts.allowOrigins);
  if (opts.outputName) throw new Error('ZIP inputs derive output names from page paths; --output-name is not supported');
  if (!opts.outputFile && !opts.outputDir) throw new Error('ZIP input requires --output-dir (or --output for one page)');
  if (opts.outputFile && opts.outputDir) throw new Error('Use --output or --output-dir exclusively');
  if (opts.outputFile && source.pages.length !== 1) throw new Error('Multiple ZIP pages require --output-dir');
  const outputRoot = path.resolve(opts.outputDir || path.dirname(opts.outputFile!));
  const inputs = source.pages.map(relative => ({ input: relative, relative,
    outputDir: opts.outputFile ? outputRoot : path.join(outputRoot, path.posix.dirname(relative)),
    outputName: opts.outputFile ? path.parse(opts.outputFile).name : path.posix.basename(relative),
    ...(opts.outputFile ? { outputFile: path.resolve(opts.outputFile) } : {}) }));
  const manifest = path.join(outputRoot, opts.outputFile ? `${path.parse(opts.outputFile).name}.import-manifest.json` : 'import-manifest.json');
  const targets = [manifest];
  const directories = [outputRoot];
  for (const input of inputs) {
    targets.push(input.outputFile || path.join(input.outputDir, `${input.outputName}.pagx`));
    targets.push(path.join(input.outputDir, `${input.outputName}.import.json`));
    if (opts.doRender || opts.preview) targets.push(path.join(input.outputDir, `${input.outputName}.png`));
    if (opts.keepSubsetHtml) targets.push(path.join(input.outputDir, `${input.outputName}.subset.html`));
    directories.push(opts.fontDir || path.join(input.outputDir, `${input.outputName}.fonts`));
    if (opts.downloadImages) directories.push(opts.imageDir || path.join(input.outputDir, `${input.outputName}.images`));
    const sourceDirectories = input.relative.split('/').slice(0, -1);
    if (sourceDirectories.some(name => name.startsWith('.pagx-stage-'))) throw new Error('ZIP output conflicts with reserved staging directory');
    if (opts.preview && sourceDirectories[0]?.toLowerCase() === 'preview') throw new Error('ZIP output conflicts with reserved preview directory');
  }
  if (opts.preview) directories.push(path.join(outputRoot, 'preview'));
  for (const target of [...targets, ...directories]) {
    if (path.resolve(target).split(path.sep).some(part => Buffer.byteLength(part) > 255)) {
      throw new Error(`ZIP output path component exceeds length limit: ${target}`);
    }
  }
  const seen = new Set<string>();
  const inputStat = fs.statSync(opts.input);
  for (const target of targets) {
    const key = pathKey(target), stat = fs.statSync(target, { throwIfNoEntry: false });
    if (seen.has(key) || key === pathKey(opts.input) ||
        (stat && stat.dev === inputStat.dev && stat.ino === inputStat.ino)) throw new Error(`ZIP output collision: ${target}`);
    if (stat?.isDirectory()) throw new Error(`ZIP output must be a file: ${target}`);
    seen.add(key); directories.push(path.dirname(target));
  }
  for (let directory of directories) {
    while (true) {
      if (seen.has(pathKey(directory))) throw new Error(`ZIP output file/directory collision: ${directory}`);
      const stat = fs.statSync(directory, { throwIfNoEntry: false });
      if (stat && !stat.isDirectory()) throw new Error(`ZIP output parent must be a directory: ${directory}`);
      const parent = path.dirname(directory);
      if (parent === directory) break;
      directory = parent;
    }
  }
  return { source, inputs, outputRoot, manifest };
}

/** Async read-only preflight, also used by the npm launcher before installing a browser. */
export async function preflightArchiveImport(opts: ArchiveImportOptions): Promise<ArchivePlan> {
  const source = await inspectZip(opts.input, { ...opts.zipOptions, root: opts.archiveRoot, signal: opts.signal });
  return planArchiveImport(opts, source);
}

async function runtimeFingerprint(opts: ArchiveImportOptions): Promise<string> {
  const hash = createHash('sha256');
  const directory = path.join(opts.scriptDir, 'dist/lib');
  for (const name of (await fs.promises.readdir(directory)).filter(name => name.endsWith('.js')).sort()) {
    hash.update(name); hash.update(await fs.promises.readFile(path.join(directory, name)));
  }
  if (opts.pagxBin && fs.existsSync(opts.pagxBin)) {
    for await (const bytes of fs.createReadStream(opts.pagxBin)) { opts.signal?.throwIfAborted(); hash.update(bytes); }
  }
  const engine = resolveEngine(opts.browserEngine);
  // Reading driver metadata does not launch/install a browser; dry-run works without it.
  try {
    const driver = require(engine);
    const executable = engine === 'playwright'
      ? process.env.PLAYWRIGHT_EXECUTABLE_PATH || driver.chromium.executablePath()
      : process.env.PUPPETEER_EXECUTABLE_PATH || driver.executablePath();
    const stat = fs.statSync(executable);
    hash.update(JSON.stringify([engine, executable, stat.size, stat.mtimeMs, require(`${engine}/package.json`).version]));
  } catch (_) { hash.update('browser-unavailable'); }
  return hash.digest('hex');
}

function signatureOptions(opts: ArchiveImportOptions): unknown {
  return {
    browserEngine: resolveEngine(opts.browserEngine), scale: opts.scale, doResolve: opts.doResolve,
    doRender: opts.doRender || opts.preview, keepSubsetHtml: opts.keepSubsetHtml,
    fontReplacements: opts.fontReplacements, fontDir: opts.fontDir, imageDir: opts.imageDir,
    downloadImages: opts.downloadImages, pagxImages: opts.pagxImages, svgOptions: opts.svgOptions,
    inlineIconFonts: opts.inlineIconFonts, captureAnimations: opts.captureAnimations, scrollReveal: opts.scrollReveal,
    viewportWidth: opts.viewportWidth, viewportHeight: opts.viewportHeight, waitMs: opts.waitMs, selector: opts.selector,
    // Credentials can change the rendered page; keep their raw values out of cache records.
    request: createHash('sha256').update(JSON.stringify({ cookies: opts.cookies || [], headers: opts.headers || [] })).digest('hex'),
    environment: Object.fromEntries(Object.entries(process.env).filter(([key]) => /^(HTML_SNAPSHOT_|PAGX_FONT)/.test(key)).sort()),
  };
}

export async function runArchiveImport(opts: ArchiveImportOptions): Promise<RunBatchResult & { manifest: ArchiveManifest; manifestFile: string }> {
  const controller = new AbortController();
  const cancel = () => controller.abort(opts.signal?.reason || new Error('ZIP import cancelled'));
  const release = trackCliResource(cancel);
  if (opts.signal?.aborted) cancel(); else opts.signal?.addEventListener('abort', cancel, { once: true });
  const options = { ...opts, signal: controller.signal };
  let website: Awaited<ReturnType<typeof prepareZip>> | undefined;
  let server: Awaited<ReturnType<typeof startWebsiteServer>> | undefined;
  let proxy: Awaited<ReturnType<typeof startWebsiteProxy>> | undefined;
  let plan: ArchivePlan | undefined, manifest: ArchiveManifest | undefined;
  const writeManifest = async () => {
    if (plan && manifest && !opts.dryRun) await writeFileAtomic(plan.manifest, JSON.stringify(manifest, null, 2), { reuseExisting: false });
  };
  try {
    throwIfCancelled(); controller.signal.throwIfAborted();
    if (opts.dryRun) plan = await preflightArchiveImport(options);
    else {
      website = await prepareZip(opts.input, { ...opts.zipOptions, root: opts.archiveRoot, signal: controller.signal });
      plan = planArchiveImport(opts, website);
    }
    const fingerprint = await runtimeFingerprint(options), settings = signatureOptions(opts);
    for (const item of plan.inputs) item.signature = JSON.stringify({ version: 1, archive: plan.source.digest,
      root: plan.source.root, page: item.relative, output: item.outputFile || path.join(item.outputDir, `${item.outputName}.pagx`),
      runtime: fingerprint, settings });
    manifest = { version: 1, source: { sha256: plan.source.digest, root: plan.source.root }, status: 'running',
      outputs: plan.inputs.map(item => ({ source: item.relative, status: 'pending' })) };
    if (!opts.dryRun) {
      await fs.promises.mkdir(plan.outputRoot, { recursive: true });
      await writeManifest();
      server = await startWebsiteServer(website!);
      proxy = await startWebsiteProxy([server.origin, ...websiteOrigins(opts.allowOrigins)]);
      for (const input of plan.inputs) input.input = server.url(input.relative);
    }
    const result = await runBatchInputs({ ...opts, inputDir: '', outputRoot: plan.outputRoot,
      signal: controller.signal, website: true, embedFonts: true, downloadFonts: true,
      keepSubsetHtml: !!opts.keepSubsetHtml, doRender: !!opts.doRender || !!opts.preview,
      // Remote assets can change independently of the archive, so they cannot use archive-only caching.
      skipExisting: opts.skipExisting && !opts.allowOrigins?.length, includeSkippedPreview: true,
      launchOptions: { args: ['--font-render-hinting=none', '--disable-quic',
        '--force-webrtc-ip-handling-policy=disable_non_proxied_udp',
        `--proxy-server=${proxy?.url}`, '--proxy-bypass-list=<-loopback>',
        ...(process.env.HTML_SNAPSHOT_ZIP_NO_SANDBOX === '1' ? ['--no-sandbox'] : [])] },
      onResult: async (input, outcome) => {
        const relative = (file: string) => path.relative(plan!.outputRoot, file).split(path.sep).join('/');
        manifest!.outputs[plan!.inputs.indexOf(input)] = { ...outcome, source: input.relative,
          ...(outcome.pagx ? { pagx: relative(outcome.pagx) } : {}),
          ...(outcome.png ? { png: relative(outcome.png) } : {}),
          ...(outcome.warnings ? { warnings: outcome.warnings.map(value => value.replaceAll(server!.origin, 'zip:')) } : {}) };
        await writeManifest();
      },
    }, plan.inputs);
    manifest.status = opts.dryRun ? 'planned' : result.failed ? 'failed' : manifest.outputs.some(item => item.status === 'partial') ? 'partial' : 'complete';
    if (result.preview) manifest.preview = { pages: result.preview.pages.map(page => ({ ...page,
      file: path.relative(plan!.outputRoot, page.file).split(path.sep).join('/') })) };
    await writeManifest();
    return { ...result, manifest, manifestFile: plan.manifest };
  } catch (error) {
    if (manifest) {
      manifest.status = 'failed'; manifest.error = error instanceof Error ? error.message : String(error);
      manifest.outputs = manifest.outputs.map(item => item.status === 'pending' ? { ...item, status: 'cancelled' } : item);
      await writeManifest();
    }
    throw error;
  } finally {
    release(); opts.signal?.removeEventListener('abort', cancel);
    try { await proxy?.close(); }
    finally { try { await server?.close(); } finally { await website?.dispose(); } }
  }
}
