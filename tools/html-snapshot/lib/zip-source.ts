// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { createHash } from 'node:crypto';
import { once } from 'node:events';
import { Transform } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import * as yauzl from 'yauzl';

export interface ZipLimits {
  archiveBytes: number;
  totalBytes: number;
  entryBytes: number;
  entries: number;
  pages: number;
  prepareMs: number;
}

export function zipLimits(overrides: Partial<ZipLimits> = {}): ZipLimits {
  const defaults: ZipLimits = { archiveBytes: 32 * 1024 * 1024, totalBytes: 256 * 1024 * 1024,
    entryBytes: 64 * 1024 * 1024, entries: 10000, pages: 500, prepareMs: 30000 };
  for (const key of Object.keys(defaults) as Array<keyof ZipLimits>) {
    const variable = `HTML_SNAPSHOT_ZIP_${key.replace(/[A-Z]/g, c => '_' + c).toUpperCase()}`;
    defaults[key] = overrides[key] ?? Number(process.env[variable] ?? defaults[key]);
    if (!Number.isSafeInteger(defaults[key]) || defaults[key] <= 0) {
      throw new Error(`${variable} must be a positive safe integer`);
    }
  }
  if (defaults.prepareMs > 2147483647) throw new Error('ZIP prepareMs exceeds timer range');
  return defaults;
}

export interface ZipOptions { root?: string; limits?: Partial<ZipLimits>; signal?: AbortSignal }
export interface ZipPlan {
  root: string;
  pages: string[];
  files: string[];
  digest: string;
}
export interface ZipWebsite extends ZipPlan {
  directory: string;
  dispose(): Promise<void>;
}

// ZIP paths are POSIX paths, independent of the host filesystem. Reject rather than
// repairing ambiguous names: normalization must never turn two entries into one file.
export function zipPath(name: string, directory = false): string {
  const value = directory && name.endsWith('/') ? name.slice(0, -1) : name;
  if (!value || /[\\\x00-\x1f\x7f]/.test(value) || /^[A-Za-z]:/.test(value) ||
      value.split('/').some(part => !part || part === '.' || part === '..')) {
    throw new Error(`Unsafe ZIP path: ${JSON.stringify(name)}`);
  }
  // These names cannot be materialized portably and can alias another file on Windows.
  if (value.split('/').some(part => /[<>:"|?*]/.test(part) || /[. ]$/.test(part) ||
      /^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(part))) {
    throw new Error(`Non-portable ZIP path: ${JSON.stringify(name)}`);
  }
  return value;
}

function rootPath(root = ''): string {
  return !root || root === '.' ? '' : zipPath(root.replace(/\/$/, ''));
}
function metadata(name: string): boolean {
  return name.split('/').some(part => part === '__MACOSX' || part === '.DS_Store' || part.startsWith('._'));
}

async function operation<T>(options: ZipOptions, work: (limits: ZipLimits, signal: AbortSignal) => Promise<T>): Promise<T> {
  const limits = zipLimits(options.limits), controller = new AbortController();
  const abort = () => controller.abort(options.signal!.reason);
  if (options.signal?.aborted) abort();
  else options.signal?.addEventListener('abort', abort, { once: true });
  const timer = setTimeout(() => controller.abort(new Error('ZIP preparation timed out')), limits.prepareMs);
  try { controller.signal.throwIfAborted(); return await work(limits, controller.signal); }
  finally { clearTimeout(timer); options.signal?.removeEventListener('abort', abort); }
}

async function closeZip(zip: yauzl.ZipFile): Promise<void> {
  if (!zip.isOpen) return;
  const closed = once(zip, 'close');
  zip.close();
  await closed;
}

async function inspect(file: string, root: string, limits: ZipLimits, signal: AbortSignal): Promise<ZipPlan> {
  const stat = await fs.promises.stat(file);
  if (!stat.isFile() || stat.size > limits.archiveBytes) throw new Error('ZIP exceeds archive size limit or is not a file');
  const zip = await yauzl.openPromise(file, { autoClose: false, strictFileNames: true, validateEntrySizes: true });
  const names = new Map<string, { name: string; directory: boolean; explicit: boolean }>();
  const files: string[] = [];
  let count = 0, total = 0;
  try {
    for await (const entry of zip.eachEntry()) {
      signal.throwIfAborted();
      if (++count > limits.entries) throw new Error('ZIP exceeds entry count limit');
      const directory = entry.fileName.endsWith('/');
      const name = zipPath(entry.fileName, directory);
      const kind = (entry.externalFileAttributes >>> 16) & 0o170000;
      if (kind && kind !== (directory ? 0o040000 : 0o100000)) throw new Error(`ZIP special file is not supported: ${name}`);
      if ((entry.generalPurposeBitFlag & 1) !== 0) throw new Error(`Encrypted ZIP entry is not supported: ${name}`);
      if (![0, 8].includes(entry.compressionMethod)) throw new Error(`Unsupported ZIP compression: ${name}`);
      total += entry.uncompressedSize;
      if (!Number.isSafeInteger(entry.uncompressedSize) || entry.uncompressedSize < 0 ||
          entry.uncompressedSize > limits.entryBytes || !Number.isSafeInteger(total) || total > limits.totalBytes) {
        throw new Error('ZIP exceeds uncompressed size limit');
      }
      const segments = name.split('/');
      for (let i = 1; i <= segments.length; i++) {
        const current = segments.slice(0, i).join('/'), key = current.normalize('NFC').toLowerCase();
        const isDirectory = i < segments.length || directory, explicit = i === segments.length;
        const previous = names.get(key);
        if (previous && (previous.name !== current || previous.directory !== isDirectory ||
            (explicit && previous.explicit))) throw new Error(`ZIP path collision: ${name}`);
        names.set(key, { name: current, directory: isDirectory, explicit: explicit || !!previous?.explicit });
      }
      if (!directory && !metadata(name) && (!root || name.startsWith(root + '/'))) {
        files.push(root ? name.slice(root.length + 1) : name);
      }
    }
  } finally { await closeZip(zip); }
  files.sort();
  const pages = files.filter(name => /\.html?$/i.test(name));
  if (!pages.length) throw new Error(`No HTML pages found in ZIP root: ${root || '/'}`);
  if (pages.length > limits.pages) throw new Error('ZIP exceeds page count limit');
  const hash = createHash('sha256');
  let read = 0;
  for await (const bytes of fs.createReadStream(file, { signal })) {
    read += bytes.length;
    if (read > limits.archiveBytes) throw new Error('ZIP exceeds archive size limit');
    hash.update(bytes);
  }
  return { root, pages, files, digest: hash.digest('hex') };
}

/** Read-only planning: no extraction, browser initialization, or output directories. */
export async function inspectZip(file: string, options: ZipOptions = {}): Promise<ZipPlan> {
  const root = rootPath(options.root);
  return operation(options, (limits, signal) => inspect(file, root, limits, signal));
}

// Incremental CRC-32, also available on Node 18 (zlib.crc32 requires newer Node).
const crcTable = Uint32Array.from({ length: 256 }, (_, value) => {
  for (let bit = 0; bit < 8; bit++) value = value & 1 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
  return value >>> 0;
});
export function updateCrc32(bytes: Buffer, crc = 0): number {
  let value = crc ^ 0xffffffff;
  for (const byte of bytes) value = crcTable[(value ^ byte) & 255] ^ (value >>> 8);
  return (value ^ 0xffffffff) >>> 0;
}

/** Own an immutable copy, so inspection, fingerprinting and extraction use the same bytes. */
export async function prepareZip(file: string, options: ZipOptions = {}): Promise<ZipWebsite> {
  const root = rootPath(options.root);
  return operation(options, async (limits, signal) => {
    const workspace = await fs.promises.mkdtemp(path.join(os.tmpdir(), 'pagx-website-'));
    const dispose = () => fs.promises.rm(workspace, { recursive: true, force: true });
    try {
      const archive = path.join(workspace, 'source.zip');
      let copied = 0;
      await pipeline(fs.createReadStream(file), new Transform({ transform(chunk: Buffer, _, callback) {
        copied += chunk.length;
        callback(copied > limits.archiveBytes ? new Error('ZIP exceeds archive size limit') : null, chunk);
      } }), fs.createWriteStream(archive, { flags: 'wx' }), { signal });
      const plan = await inspect(archive, root, limits, signal);
      const directory = path.join(workspace, 'site');
      await fs.promises.mkdir(directory);
      const zip = await yauzl.openPromise(archive, { autoClose: false, strictFileNames: true, validateEntrySizes: true });
      let total = 0;
      try {
        for await (const entry of zip.eachEntry()) {
          signal.throwIfAborted();
          if (entry.fileName.endsWith('/')) continue;
          // Validate every payload, including entries outside the selected root, before rendering.
          const selected = !metadata(entry.fileName) && (!root || entry.fileName.startsWith(root + '/'));
          const relative = root ? entry.fileName.slice(root.length + 1) : entry.fileName;
          const destination = selected ? path.join(directory, relative) : '';
          if (selected) await fs.promises.mkdir(path.dirname(destination), { recursive: true });
          let size = 0, crc = 0;
          const check = new Transform({ transform(chunk: Buffer, _, callback) {
            size += chunk.length; total += chunk.length; crc = updateCrc32(chunk, crc);
            callback(size > limits.entryBytes || total > limits.totalBytes
              ? new Error('ZIP exceeds actual uncompressed size limit') : null, selected ? chunk : undefined);
          } });
          const input = await zip.openReadStreamPromise(entry);
          if (selected) await pipeline(input, check, fs.createWriteStream(destination, { flags: 'wx' }), { signal });
          else { check.resume(); await pipeline(input, check, { signal }); }
          if (size !== entry.uncompressedSize || crc !== entry.crc32) throw new Error(`ZIP checksum mismatch: ${entry.fileName}`);
        }
      } finally { await closeZip(zip); }
      signal.throwIfAborted();
      return { ...plan, directory, dispose };
    } catch (error) { await dispose(); throw error; }
  });
}
