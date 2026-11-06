// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import type { IncomingMessage, ServerResponse } from 'node:http';
import { Readable, Transform } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { ZipFile } from 'yazl';
import { ConversionTask } from './conversion-task';
import { HttpError, setBoolParam } from './http-utils';
import { runArchiveImport, preflightArchiveImport, type ArchiveImportOptions } from './archive-import';
import { websiteOrigins } from './website-proxy';
import { validatePreviewOptions } from './contact-sheet';

export function parseArchiveQuery(url: string): Partial<ArchiveImportOptions> {
  const params = new URL(url, 'http://localhost').searchParams;
  const known = new Set(['root', 'format', 'viewportWidth', 'viewportHeight', 'waitMs', 'selector',
    'inlineIconFonts', 'captureAnimations', 'scrollReveal', 'render', 'preview', 'previewColumns']);
  for (const key of params.keys()) {
    if (!known.has(key) || params.getAll(key).length !== 1) throw new HttpError(400, `Unknown or repeated archive option: ${key}`);
  }
  const format = params.get('format') || 'pagx';
  if (!['pagx', 'both'].includes(format)) throw new HttpError(400, 'Archive format must be pagx or both');
  const values: Record<string, unknown> = {};
  for (const key of ['inlineIconFonts', 'captureAnimations', 'scrollReveal', 'render', 'preview']) setBoolParam(params, values, key);
  for (const key of ['viewportWidth', 'viewportHeight', 'waitMs', 'previewColumns']) {
    if (!params.has(key)) continue;
    const raw = params.get(key)!, number = Number(raw);
    if (!raw.trim() || !Number.isSafeInteger(number) || number < (key === 'waitMs' ? 0 : 1) || number > 2147483647) {
      throw new HttpError(400, `${key} must be a valid integer`);
    }
    values[key] = number;
  }
  const options = { ...values, archiveRoot: params.get('root') || '', selector: params.get('selector') || '',
    keepSubsetHtml: format === 'both', doRender: !!values.render || !!values.preview } as Partial<ArchiveImportOptions>;
  try { validatePreviewOptions(options); }
  catch (error) { throw new HttpError(400, (error as Error).message); }
  return options;
}

/** Package only final artifacts, never cache records or private native staging directories. */
export async function writeArtifactZip(directory: string, destination: string,
    options: { signal?: AbortSignal; maxBytes?: number; maxFiles?: number } = {}): Promise<void> {
  const maxBytes = options.maxBytes ?? 512 * 1024 * 1024, maxFiles = options.maxFiles ?? 20000;
  const files: Array<{ file: string; name: string; size: number }> = [];
  const pending = [directory];
  let total = 0;
  while (pending.length) {
    options.signal?.throwIfAborted();
    const current = pending.pop()!;
    for (const item of await fs.promises.readdir(current, { withFileTypes: true })) {
      if ((item.isDirectory() && item.name.startsWith('.pagx-stage-')) ||
          (item.isFile() && item.name.endsWith('.import.json'))) continue;
      const file = path.join(current, item.name);
      if (item.isDirectory()) { pending.push(file); continue; }
      if (!item.isFile()) throw new Error(`Unexpected non-file output: ${file}`);
      const { size } = await fs.promises.stat(file);
      total += size;
      if (total > maxBytes || files.length >= maxFiles) throw new HttpError(413, 'Archive outputs exceed packaging limit');
      files.push({ file, name: path.relative(directory, file).split(path.sep).join('/'), size });
    }
  }
  files.sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
  const zip = new ZipFile(), output = zip.outputStream as Readable;
  const readers = new Set<fs.ReadStream>();
  zip.on('error', error => output.destroy(error));
  let written = 0;
  const result = pipeline(output, new Transform({ transform(bytes: Buffer, _, callback) {
    written += bytes.length;
    callback(written > maxBytes ? new HttpError(413, 'Archive response exceeds packaging limit') : null, bytes);
  } }), fs.createWriteStream(destination, { flags: 'wx' }), { signal: options.signal });
  for (const file of files) zip.addReadStreamLazy(file.name, { size: file.size }, callback => {
    const stream = fs.createReadStream(file.file, { signal: options.signal });
    readers.add(stream); stream.once('close', () => readers.delete(stream));
    // yazl's lazy stream API does not forward source errors to its output stream.
    stream.once('error', error => output.destroy(error));
    callback(null, stream);
  });
  zip.end();
  try { await result; }
  finally {
    await Promise.all([...readers].map(reader => new Promise<void>(resolve => {
      reader.once('close', resolve); reader.destroy();
    })));
  }
}

export interface ArchiveHttpContext {
  signal?: AbortSignal;
  pagxBin: string;
  browserEngine: string;
  scriptDir: string;
  maxBodyBytes: number;
  readBody: (req: IncomingMessage, maxBytes: number) => Promise<Buffer>;
  log?: (message: string) => void;
}

export async function handleArchiveHttp(req: IncomingMessage, res: ServerResponse, context: ArchiveHttpContext): Promise<void> {
  if (!['application/zip', 'application/x-zip-compressed'].includes((req.headers['content-type'] || '').split(';')[0].trim().toLowerCase())) {
    throw new HttpError(415, 'Archive uploads require Content-Type: application/zip');
  }
  const options = parseArchiveQuery(req.url || '');
  try { await fs.promises.access(context.pagxBin, process.platform === 'win32' ? fs.constants.F_OK : fs.constants.X_OK); }
  catch (_) { throw new HttpError(503, 'Native pagx binary is unavailable'); }
  const totalMs = Number(process.env.HTML_SNAPSHOT_ARCHIVE_TIMEOUT_MS || 600000);
  const task = new ConversionTask({ totalMs, settleMs: Math.floor(totalMs / 2) });
  const abort = () => { if (!res.writableEnded) task.abort(new Error('Archive client disconnected')); };
  const cancel = () => task.abort(context.signal!.reason);
  if (context.signal?.aborted) cancel(); else context.signal?.addEventListener('abort', cancel, { once: true });
  req.once('aborted', abort); res.once('close', abort);
  let directory: string | undefined;
  try {
    await task.run(async () => {
      task.checkpoint('archive-upload');
      const stopUpload = () => { if (!req.complete) req.destroy(); };
      task.signal.addEventListener('abort', stopUpload, { once: true });
      let body: Buffer;
      try { body = await context.readBody(req, context.maxBodyBytes); }
      finally { task.signal.removeEventListener('abort', stopUpload); }
      task.checkpoint('archive-prepare');
      directory = await fs.promises.mkdtemp(path.join(os.tmpdir(), 'pagx-archive-http-'));
      const input = path.join(directory, 'input.zip'), outputDir = path.join(directory, 'outputs');
      await fs.promises.writeFile(input, body);
      const opts: ArchiveImportOptions = { ...options, input, outputDir, scriptDir: context.scriptDir,
        pagxBin: context.pagxBin, browserEngine: context.browserEngine, signal: task.signal, cleanupTask: task, log: context.log,
        allowOrigins: websiteOrigins((process.env.HTML_SNAPSHOT_ARCHIVE_ALLOW_ORIGINS || '').split(',').filter(Boolean)) };
      try { await preflightArchiveImport(opts); }
      catch (error) { task.checkpoint(); throw new HttpError(400, (error as Error).message); }
      task.checkpoint('archive-convert');
      const converted = await runArchiveImport(opts);
      task.checkpoint('archive-package');
      const result = path.join(directory, 'result.zip');
      await writeArtifactZip(outputDir, result, { signal: task.signal });
      const stat = await fs.promises.stat(result);
      task.checkpoint('publish');
      res.writeHead(200, { 'Content-Type': 'application/zip', 'Content-Length': stat.size,
        'Content-Disposition': 'attachment; filename="imported-webpages.zip"',
        'X-Snapshot-Status': converted.manifest.status, 'X-Snapshot-Pages': converted.total });
      await pipeline(fs.createReadStream(result), res, { signal: task.signal });
    });
  } finally {
    req.off('aborted', abort); res.off('close', abort);
    context.signal?.removeEventListener('abort', cancel);
    task.deferCleanup(async () => { if (directory) await fs.promises.rm(directory, { recursive: true, force: true }); });
    if (!(await task.finish())) context.log?.('archive cleanup did not complete');
  }
}
