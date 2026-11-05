// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as fs from 'node:fs';
import * as path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { DOMParser, XMLSerializer } from '@xmldom/xmldom';
import type { ConversionTask } from './conversion-task';

export function validatePagx(xml: string): ReturnType<DOMParser['parseFromString']> {
  const errors: string[] = [];
  const doc = new DOMParser({ onError: (level, message) => { errors.push(`${level}: ${message}`); } })
    .parseFromString(xml, 'application/xml');
  const root = doc.documentElement;
  if (errors.length || !root || root.tagName !== 'pagx' || doc.doctype ||
      !['width', 'height'].every(name => Number.isFinite(Number(root.getAttribute(name))) && Number(root.getAttribute(name)) > 0)) {
    throw new Error('Native import produced invalid PAGX');
  }
  return doc;
}

interface Entry { staged: string; target: string; immutable: boolean }

/** Private work files; publication contains no asynchronous gap between checking and renaming. */
export class ArtifactPublication {
  readonly directory: string;
  private readonly entries = new Map<string, Entry>();
  private disposed = false;
  private preserveBackups = false;
  constructor(readonly task: ConversionTask, parent: string) {
    task.checkpoint();
    fs.mkdirSync(parent, { recursive: true });
    this.directory = fs.mkdtempSync(path.join(parent, '.pagx-stage-'));
  }

  add(staged: string, target: string, immutable = false): void {
    this.task.checkpoint();
    this.entries.set(path.resolve(target), { staged, target: path.resolve(target), immutable });
  }

  source(target: string): string { return this.entries.get(path.resolve(target))?.staged || target; }

  /** Keep external resources immutable, so replacing a document never damages an older one. */
  async prepareImages(pagxFile: string, finalPagx: string): Promise<void> {
    const original = await fs.promises.readFile(pagxFile, 'utf8');
    const doc = validatePagx(original);
    let changed = false;
    for (const image of Array.from(doc.getElementsByTagName('Image'))) {
      this.task.checkpoint('resources');
      const source = image.getAttribute('source') || '';
      if (!source || source.startsWith('@') || /^(?:data:|https?:)/i.test(source)) continue;
      const file = source.startsWith('file:') ? fileURLToPath(source) : path.resolve(path.dirname(pagxFile), source);
      const bytes = await fs.promises.readFile(file);
      const hash = createHash('sha256').update(bytes).digest('hex');
      const extension = /^\.[a-z0-9]{1,8}$/i.test(path.extname(file)) ? path.extname(file).toLowerCase() : '.bin';
      const relative = `.pagx-assets/${hash}${extension}`;
      const staged = path.join(path.dirname(pagxFile), relative);
      await fs.promises.mkdir(path.dirname(staged), { recursive: true });
      await fs.promises.writeFile(staged, bytes);
      this.add(staged, path.join(path.dirname(finalPagx), relative), true);
      image.setAttribute('source', relative);
      changed = true;
    }
    // Leave documents with inline images byte-identical, including whitespace and metadata.
    if (changed) await fs.promises.writeFile(pagxFile, new XMLSerializer().serializeToString(doc));
    this.task.checkpoint();
  }

  commit(): void {
    this.task.checkpoint('publish');
    const replaced: Array<{ target: string; backup?: string }> = [];
    try {
      // Resources first, followed by previews/subsets, then the primary PAGX registered last.
      const priority = (entry: Entry) => entry.immutable ? 0 : path.extname(entry.target) === '.pagx' ? 2 : 1;
      const entries = [...this.entries.values()].sort((a, b) => priority(a) - priority(b));
      for (const [index, entry] of entries.entries()) {
        this.task.checkpoint();
        fs.mkdirSync(path.dirname(entry.target), { recursive: true });
        if (entry.immutable && fs.existsSync(entry.target)) {
          if (!fs.readFileSync(entry.target).equals(fs.readFileSync(entry.staged))) {
            throw new Error(`Resource cache content mismatch: ${entry.target}`);
          }
          continue;
        }
        let backup: string | undefined;
        if (!entry.immutable && fs.existsSync(entry.target)) {
          backup = path.join(this.directory, `backup-${index}`);
          fs.copyFileSync(entry.target, backup, fs.constants.COPYFILE_FICLONE);
        }
        this.task.checkpoint();
        fs.renameSync(entry.staged, entry.target);
        if (!entry.immutable) replaced.push({ target: entry.target, backup });
      }
      this.task.checkpoint();
    } catch (error) {
      const failures: unknown[] = [];
      for (const entry of replaced.reverse()) {
        try {
          if (entry.backup) fs.renameSync(entry.backup, entry.target);
          else fs.unlinkSync(entry.target);
        } catch (failure) { failures.push(failure); }
      }
      if (failures.length) {
        this.preserveBackups = true;
        throw new AggregateError([error, ...failures], `Publication rollback was incomplete; backups retained in ${this.directory}`);
      }
      throw error;
    }
  }

  dispose(): void {
    if (this.disposed || this.preserveBackups) return;
    this.disposed = true;
    // Includes late child exits. Immutable shared resources are deliberately retained.
    this.task.deferCleanup(() => fs.promises.rm(this.directory, { recursive: true, force: true }));
  }
}
