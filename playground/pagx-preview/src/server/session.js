/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import path from 'path';
import chokidar from 'chokidar';

// Delay between raw chokidar events and the reload emission. Absorbs the burst that editors
// produce when saving (temp file + rename + attr touch) into a single reload.
const RELOAD_DEBOUNCE_MS = 80;

/**
 * PreviewSession tracks one PAGX file plus its external resources (images, embedded fonts, etc.)
 * and turns filesystem changes into reload events for subscribers.
 *
 * The entry file is watched from construction. Resource files reported by the browser via
 * updateResources() are added incrementally; stale entries are unwatched on the next update.
 */
export class PreviewSession {
  constructor(id, entryFile) {
    this.id = id;
    this.entryFile = path.resolve(entryFile);
    this.entryDir = path.dirname(this.entryFile);
    this.resources = new Set();
    this.listeners = new Set();
    this.reloadTimer = null;
    // Cached document summary uploaded by the client after a successful load. Used by the MCP
    // get_document tool to answer AI queries without round-tripping through the client. Stale
    // data is acceptable — the client overwrites it on every load.
    this.documentSummary = null;
    // Count of consecutive get_document calls that found no summary. When a host does not render
    // the inline widget, the summary never arrives; this lets the MCP tool detect that case and
    // fall back to the browser-openable webview after a few misses. Reset to 0 on a successful
    // get_document (i.e. once the client has uploaded a summary).
    this.documentQueryFailures = 0;
    this.watcher = chokidar.watch(this.entryFile, {
      ignoreInitial: true,
      awaitWriteFinish: { stabilityThreshold: 40, pollInterval: 20 },
    });
    this.watcher.on('all', (event, file) => this.onFsEvent(event, file));
  }

  /** Resolves a browser-supplied relative path to an absolute path under entryDir, or null if
   *  the path escapes the entry directory (defense against `../` traversal). */
  resolveResource(relativePath) {
    if (typeof relativePath !== 'string' || relativePath.length === 0) return null;
    if (relativePath.includes('\0')) return null;
    const absolute = path.resolve(this.entryDir, relativePath);
    const rel = path.relative(this.entryDir, absolute);
    if (rel.startsWith('..') || path.isAbsolute(rel)) return null;
    return absolute;
  }

  /** Replaces the resource watch list with the browser-reported set. */
  updateResources(paths) {
    const nextAbs = new Set();
    for (const p of paths) {
      const abs = this.resolveResource(p);
      if (abs) nextAbs.add(abs);
    }
    const toAdd = [];
    const toRemove = [];
    for (const abs of nextAbs) {
      if (!this.resources.has(abs)) toAdd.push(abs);
    }
    for (const abs of this.resources) {
      if (!nextAbs.has(abs)) toRemove.push(abs);
    }
    if (toAdd.length > 0) this.watcher.add(toAdd);
    if (toRemove.length > 0) this.watcher.unwatch(toRemove);
    this.resources = nextAbs;
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  hasSubscribers() {
    return this.listeners.size > 0;
  }

  /** Stores a document summary uploaded by the client after load. See documentSummary comment. */
  setDocumentSummary(summary) {
    this.documentSummary = summary;
    this.documentQueryFailures = 0;
  }

  onFsEvent(event, file) {
    if (event !== 'add' && event !== 'change' && event !== 'unlink') return;
    if (this.reloadTimer !== null) return;
    this.reloadTimer = setTimeout(() => {
      this.reloadTimer = null;
      this.emit({ type: 'reload', file, event });
    }, RELOAD_DEBOUNCE_MS);
  }

  emit(payload) {
    for (const listener of this.listeners) {
      try {
        listener(payload);
      } catch (err) {
        console.error('pagx preview: subscriber threw', err);
      }
    }
  }

  async close() {
    if (this.reloadTimer !== null) {
      clearTimeout(this.reloadTimer);
      this.reloadTimer = null;
    }
    this.listeners.clear();
    await this.watcher.close();
  }
}
