// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';
import { pipeline } from 'node:stream/promises';
import { lookup } from 'mime-types';
import type { ZipWebsite } from './zip-source';

export interface WebsiteServer {
  origin: string;
  url(entry: string): string;
  close(): Promise<void>;
}

/** Serve only inspected archive entries; output/staging directories are never reachable. */
export async function startWebsiteServer(website: Pick<ZipWebsite, 'directory' | 'files'>): Promise<WebsiteServer> {
  const files = new Set(website.files), directories = new Set(['']);
  for (const file of files) {
    const segments = file.split('/');
    for (let i = 1; i < segments.length; i++) directories.add(segments.slice(0, i).join('/'));
  }
  let origin = '';
  const serve = async (req: http.IncomingMessage, res: http.ServerResponse) => {
    const fail = (code: number) => { res.writeHead(code, { 'Content-Type': 'text/plain' }); res.end(http.STATUS_CODES[code]); };
    if (req.headers.host !== new URL(origin).host) { fail(403); return; }
    if (req.method !== 'GET' && req.method !== 'HEAD') { res.setHeader('Allow', 'GET, HEAD'); fail(405); return; }
    let url: URL, relative: string;
    try {
      url = new URL(req.url!, origin);
      const segments = url.pathname.slice(1).split('/').map(segment => decodeURIComponent(segment));
      if (url.origin !== origin || segments.some(segment => /[\\/\x00]/.test(segment) || segment === '.' || segment === '..')) {
        fail(400); return;
      }
      relative = segments.join('/');
    } catch (_) { fail(400); return; }
    const directory = relative.replace(/\/$/, '');
    if (directories.has(directory)) {
      if (!url.pathname.endsWith('/')) {
        res.writeHead(308, { Location: url.pathname + '/' + url.search }); res.end(); return;
      }
      relative = [directory ? directory + '/index.html' : 'index.html', directory ? directory + '/index.htm' : 'index.htm']
        .find(file => files.has(file)) || '';
    }
    if (!files.has(relative)) { fail(404); return; }
    const file = path.join(website.directory, relative), stat = await fs.promises.stat(file);
    let start = 0, end = stat.size - 1, status = 200;
    if (req.headers.range) {
      const range = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range);
      if (!range || (!range[1] && !range[2])) { fail(416); return; }
      start = range[1] ? Number(range[1]) : Math.max(0, stat.size - Number(range[2]));
      end = range[1] && range[2] ? Math.min(Number(range[2]), end) : end;
      if (!Number.isSafeInteger(start) || !Number.isSafeInteger(end) || start > end || start >= stat.size) {
        res.setHeader('Content-Range', `bytes */${stat.size}`); fail(416); return;
      }
      status = 206;
      res.setHeader('Content-Range', `bytes ${start}-${end}/${stat.size}`);
    }
    res.writeHead(status, {
      // Do not override a document's BOM/meta charset with an assumed UTF-8 header.
      'Content-Type': lookup(relative) || 'application/octet-stream',
      'Content-Length': Math.max(0, end - start + 1), 'Accept-Ranges': 'bytes',
      'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff',
    });
    if (req.method === 'HEAD' || !stat.size) { res.end(); return; }
    await pipeline(fs.createReadStream(file, { start, end }), res);
  };
  const server = http.createServer((req, res) => {
    void serve(req, res).catch(() => {
      if (!res.headersSent) res.writeHead(500);
      res.end();
    });
  });
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => { server.off('error', reject); resolve(); });
  });
  origin = `http://127.0.0.1:${(server.address() as { port: number }).port}`;
  let closing: Promise<void> | undefined;
  return {
    origin,
    url(entry) {
      if (!files.has(entry)) throw new Error(`Unknown website entry: ${entry}`);
      return origin + '/' + entry.split('/').map(encodeURIComponent).join('/');
    },
    close() {
      return closing ??= new Promise<void>((resolve, reject) => {
        server.close(error => error ? reject(error) : resolve());
        server.closeAllConnections();
      });
    },
  };
}
