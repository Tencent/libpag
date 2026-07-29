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
import fs from 'fs';
import crypto from 'crypto';
import express from 'express';
import { fileURLToPath } from 'url';
import { PreviewSession } from './session.js';
import { resolveFontsDir, listFonts, listFontsPreferringCache } from './fonts.js';
import { ensureAllFonts } from './font-cache.js';
import { mountMcpServer } from '../mcp/server.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PREVIEW_DIR = path.resolve(__dirname, '../..');
const STATIC_DIR = path.join(PREVIEW_DIR, 'static');
// Compiled/copied artifacts (viewer wasm+glue, player bundle, ext bundle, MCP widget bundle) live
// in a single gitignored directory outside static/, mirroring pagx-playground's wasm-mt/ layout.
const GENERATED_DIR = path.join(PREVIEW_DIR, 'wasm');
const VIEWER_INFO_PATH = path.join(GENERATED_DIR, 'viewer', 'info.json');

// Once every tab has closed its SSE stream, wait this long before shutting the server down.
// A short grace period covers "close tab, immediately re-run pagx-preview" without a respawn.
const IDLE_SHUTDOWN_MS = 30_000;

/** Short deterministic id for a file path so the same file always maps to the same session. */
function sessionIdFor(filePath) {
  return crypto.createHash('sha1').update(path.resolve(filePath)).digest('hex').slice(0, 10);
}

/** Short random id for one-shot drop sessions (files opened via browser drag-and-drop). */
function randomSessionId() {
  return 'drop-' + crypto.randomBytes(5).toString('hex');
}

/** Reads the prebuild-produced info.json describing which pagx-viewer variant was staged. */
function readViewerInfo() {
  if (!fs.existsSync(VIEWER_INFO_PATH)) {
    throw new Error(
      `pagx-preview: viewer artifacts not found at ${GENERATED_DIR}/viewer. Run "npm run prebuild" first.`
    );
  }
  return JSON.parse(fs.readFileSync(VIEWER_INFO_PATH, 'utf8'));
}

/**
 * Starts the preview server and creates the initial session for `entryFile` (if provided).
 *
 * The returned object exposes:
 *   - port / host / url / sessionId: connection info for the initial session
 *   - fontsDir: absolute path used for fallback fonts (null if none)
 *   - close(): stop the server and all its watchers
 *   - onIdle(callback): fired when the last SSE subscriber leaves after IDLE_SHUTDOWN_MS
 */
export async function startServer({ entryFile = null, port = 0, host = '127.0.0.1', fontsDir = null }) {
  let resolvedFontsDir = resolveFontsDir(fontsDir);
  // Endpoints look up the current font list on every request so a background lazy-download can
  // become visible without a restart. Explicit --fonts / PAGX_FONTS_DIR overrides skip the
  // download path entirely.
  const useLazyDownload = !fontsDir && !process.env.PAGX_FONTS_DIR;
  function currentFontEntries() {
    // When lazy-downloading, prefer any file that has already landed in the cache. Otherwise
    // stick to the resolved directory so overrides remain authoritative.
    return useLazyDownload
      ? listFontsPreferringCache(resolvedFontsDir)
      : listFonts(resolvedFontsDir);
  }

  const viewerInfo = readViewerInfo();

  const app = express();
  app.use(express.json({ limit: '4mb' }));

  // MCP Apps hosts (Claude Desktop, CodeBuddy, etc.) load the widget HTML inside a cross-origin
  // sandbox iframe. All resource fetches (ES module imports, pagx bytes, fonts, SSE) from the
  // iframe to this server are cross-origin, so without CORS headers the browser blocks them and
  // the widget stays blank. This is a local-only preview server, so a permissive CORS policy is
  // safe here.
  app.use((req, res, next) => {
    res.set('Access-Control-Allow-Origin', '*');
    res.set('Access-Control-Allow-Methods', 'GET, POST, DELETE, OPTIONS');
    res.set('Access-Control-Allow-Headers', 'Content-Type, MCP-Protocol-Version, Accept');
    if (req.method === 'OPTIONS') {
      res.status(204).end();
      return;
    }
    next();
  });

  // The multi-threaded pagx-viewer needs SharedArrayBuffer, which browsers only expose in a
  // cross-origin-isolated context. The single-threaded build has no such requirement.
  if (viewerInfo.multiThreaded) {
    app.use((req, res, next) => {
      res.set('Cross-Origin-Opener-Policy', 'same-origin');
      res.set('Cross-Origin-Embedder-Policy', 'require-corp');
      next();
    });
  }

  const sessions = new Map();
  // Tracks how many SSE responses are open per session id. When it drops to zero for every
  // session, the idle timer fires.
  const sseCountBySession = new Map();

  // Track live TCP sockets and SSE responses so shutdown can force-close them. Node's
  // http.Server.close() only stops accepting new connections and waits for existing ones to
  // drain; without this an open SSE stream keeps the process alive forever after Ctrl+C.
  const sockets = new Set();
  const sseResponses = new Set();

  let idleCallback = null;
  let idleTimer = null;

  function totalSseCount() {
    let n = 0;
    for (const c of sseCountBySession.values()) n += c;
    return n;
  }

  function scheduleIdleCheck() {
    if (!idleCallback) return;
    if (idleTimer !== null) return;
    if (totalSseCount() > 0) return;
    idleTimer = setTimeout(() => {
      idleTimer = null;
      if (totalSseCount() === 0 && idleCallback) idleCallback();
    }, IDLE_SHUTDOWN_MS);
  }

  function cancelIdleCheck() {
    if (idleTimer !== null) {
      clearTimeout(idleTimer);
      idleTimer = null;
    }
  }

  function createOrGetSession(filePath) {
    const id = sessionIdFor(filePath);
    let session = sessions.get(id);
    let reused = true;
    if (!session) {
      session = new PreviewSession(id, filePath);
      sessions.set(id, session);
      reused = false;
    }
    return { session, reused };
  }

  const initialSession = entryFile ? createOrGetSession(path.resolve(entryFile)).session : null;

  // Lightweight probe used by the CLI's health check when deciding whether to reuse a running
  // server. Kept intentionally trivial so a lock check finishes in ~1ms.
  app.get('/health', (req, res) => {
    res.json({ ok: true, sessions: sessions.size });
  });

  // Session index for CLI reuse. POST creates or returns an existing session for a filesystem
  // path; a session id of "drop" indicates a one-shot ephemeral upload from the browser drop
  // handler (see the /session/drop endpoint below).
  app.post('/sessions', (req, res) => {
    // /sessions is a CLI-only endpoint that creates/reuses a session for a filesystem path. No
    // browser context ever calls it: the same-origin preview tab and the cross-origin widget
    // iframe only POST to /session/:id/{resources,document}. Any request carrying browser
    // fetch-metadata is therefore a cross-site attempt to inject an arbitrary path and read the
    // file back via GET /session/:id/pagx, so reject it. The Node CLI sends neither header.
    const site = req.get('Sec-Fetch-Site');
    if (req.get('Origin') || (site && site !== 'same-origin' && site !== 'none')) {
      res.status(403).json({ error: 'forbidden' });
      return;
    }
    const filePath = typeof req.body?.file === 'string' ? req.body.file : null;
    if (!filePath) {
      res.status(400).json({ error: 'missing file' });
      return;
    }
    const absolute = path.resolve(filePath);
    // Defense in depth: the preview only ever handles .pagx files, so refuse any other extension.
    // This shrinks the arbitrary-file-read surface even if the guard above is ever bypassed.
    if (path.extname(absolute).toLowerCase() !== '.pagx') {
      res.status(400).json({ error: 'not a .pagx file', file: absolute });
      return;
    }
    if (!fs.existsSync(absolute)) {
      res.status(404).json({ error: 'file not found', file: absolute });
      return;
    }
    const { session, reused } = createOrGetSession(absolute);
    const hadActiveTab = (sseCountBySession.get(session.id) || 0) > 0;
    if (reused && hadActiveTab) {
      // Ask the existing tab to bring itself to the front. Browsers usually refuse this outside
      // a user gesture, so the CLI must not depend on it — see cli.js for the fallback path.
      session.emit({ type: 'focus' });
    }
    res.json({
      id: session.id,
      url: `/session/${session.id}/`,
      reused,
      hadActiveTab,
    });
  });

  // One-shot ephemeral session for files dropped into the browser. The file bytes are held in
  // memory only (never written to disk) and the session watches nothing: dropped files are
  // preview-only. Storing on disk with live watch would require the browser to give us a real
  // absolute path, which it never does.
  const DROP_SESSION_MAX = 100;
  const dropSessions = new Map();
  app.post('/session/drop', express.raw({ type: '*/*', limit: '32mb' }), (req, res) => {
    if (!req.body || req.body.length === 0) {
      res.status(400).json({ error: 'empty body' });
      return;
    }
    // Evict oldest entries when the cap is reached to prevent unbounded memory growth.
    if (dropSessions.size >= DROP_SESSION_MAX) {
      const firstKey = dropSessions.keys().next().value;
      if (firstKey !== undefined) dropSessions.delete(firstKey);
    }
    const name = String(req.query.name || 'dropped.pagx');
    const id = randomSessionId();
    dropSessions.set(id, { name, data: Buffer.from(req.body) });
    res.json({ id, url: `/session/${id}/`, name });
  });

  // Session page. All same-origin fetches issued by the page are scoped under /session/:id/
  // so the browser sees a single origin per document; the routes below key on :id.
  app.get('/session/:id/', (req, res) => {
    if (!sessions.has(req.params.id) && !dropSessions.has(req.params.id)) {
      res.status(404).send('unknown session');
      return;
    }
    res.sendFile(path.join(STATIC_DIR, 'index.html'));
  });

  // Entry PAGX bytes.
  app.get('/session/:id/pagx', (req, res) => {
    const drop = dropSessions.get(req.params.id);
    if (drop) {
      res.set('Content-Type', 'application/octet-stream');
      res.send(drop.data);
      return;
    }
    const session = sessions.get(req.params.id);
    if (!session) {
      res.status(404).send('unknown session');
      return;
    }
    res.sendFile(session.entryFile, (err) => {
      if (err && !res.headersSent) {
        res.status(500).send('failed to read entry file');
      }
    });
  });

  // Overwrite the entry pagx file with the request body. Used by the editor's "Save" button so
  // in-browser edits become durable on disk. The path is fixed to the session's entryFile and
  // never taken from the URL / body, keeping this endpoint from being turned into an
  // arbitrary-file-write primitive. Drop sessions have no on-disk file so they get a clear 400.
  // The write itself will trigger the session's file watcher, which broadcasts a `reload` SSE
  // event that all connected tabs pick up naturally - so this endpoint stops at persistence and
  // leaves refresh to the existing reload path.
  app.put(
    '/session/:id/pagx',
    express.raw({ type: '*/*', limit: '32mb' }),
    async (req, res) => {
      if (dropSessions.has(req.params.id)) {
        res.status(400).json({ error: 'save is not available for dropped files' });
        return;
      }
      const session = sessions.get(req.params.id);
      if (!session) {
        res.status(404).json({ error: 'unknown session' });
        return;
      }
      if (!req.body || req.body.length === 0) {
        res.status(400).json({ error: 'empty body' });
        return;
      }
      try {
        await fs.promises.writeFile(session.entryFile, req.body);
        res.json({ ok: true, bytes: req.body.length });
      } catch (err) {
        console.error(`pagx-preview: failed to save ${session.entryFile}`, err);
        res.status(500).json({ error: err && err.message ? err.message : 'write failed' });
      }
    },
  );

  // Session metadata (label shown in the tab title, watch mode, etc.).
  app.get('/session/:id/info', (req, res) => {
    const drop = dropSessions.get(req.params.id);
    if (drop) {
      res.json({ name: drop.name, mode: 'drop', watched: false });
      return;
    }
    const session = sessions.get(req.params.id);
    if (!session) {
      res.status(404).send('unknown session');
      return;
    }
    res.json({ name: path.basename(session.entryFile), mode: 'watch', watched: true });
  });

  // External resource fetch. The path is browser-supplied and must stay under the entry file's
  // directory; PreviewSession.resolveResource() enforces that. Drop sessions cannot reference
  // external files (they're one-shot in-memory previews).
  app.get('/session/:id/resources/*', (req, res) => {
    const session = sessions.get(req.params.id);
    if (!session) {
      res.status(404).send('unknown session');
      return;
    }
    const relativePath = req.params[0];
    const absolute = session.resolveResource(relativePath);
    if (!absolute) {
      res.status(400).send('invalid resource path');
      return;
    }
    res.sendFile(absolute, (err) => {
      if (err && !res.headersSent) {
        res.status(404).send('resource not found');
      }
    });
  });

  // Browser reports the PAGX's external file list once the document is parsed. The server uses
  // the list to extend its filesystem watch, so edits to referenced images also trigger reload.
  app.post('/session/:id/resources', (req, res) => {
    const session = sessions.get(req.params.id);
    if (!session) {
      // Drop sessions ignore this endpoint silently: they never watch anything.
      if (dropSessions.has(req.params.id)) {
        res.json({ ok: true, watched: false });
        return;
      }
      res.status(404).send('unknown session');
      return;
    }
    const paths = Array.isArray(req.body?.paths) ? req.body.paths : [];
    session.updateResources(paths);
    res.json({ ok: true, count: paths.length });
  });

  // Client uploads a document summary (node list, dimensions, duration) after a successful load.
  // The MCP get_document tool reads this cache to answer AI queries without a client round-trip.
  app.post('/session/:id/document', (req, res) => {
    const session = sessions.get(req.params.id);
    if (!session) {
      res.status(404).json({ error: 'unknown session' });
      return;
    }
    session.setDocumentSummary(req.body ?? null);
    res.json({ ok: true });
  });

  // Returns the cached document summary. null when the client hasn't uploaded one yet (e.g. the
  // pagx is still loading or the client doesn't support summary upload).
  app.get('/session/:id/document', (req, res) => {
    const session = sessions.get(req.params.id);
    if (!session) {
      res.status(404).json({ error: 'unknown session' });
      return;
    }
    res.json(session.documentSummary);
  });

  // Server-Sent Events: one long-lived response per open tab. The browser reconnects on drop.
  app.get('/session/:id/events', (req, res) => {
    const id = req.params.id;
    const session = sessions.get(id);
    if (!session && !dropSessions.has(id)) {
      res.status(404).end();
      return;
    }
    res.set({
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
      'X-Accel-Buffering': 'no',
    });
    res.flushHeaders();
    res.write(': connected\n\n');
    sseResponses.add(res);
    sseCountBySession.set(id, (sseCountBySession.get(id) || 0) + 1);
    cancelIdleCheck();

    let unsubscribe = () => {};
    if (session) {
      unsubscribe = session.subscribe((event) => {
        res.write(`event: ${event.type}\n`);
        res.write(`data: ${JSON.stringify(event)}\n\n`);
      });
    }

    // Periodic comment keeps intermediaries from closing an idle connection.
    const keepAlive = setInterval(() => res.write(': ping\n\n'), 25000);

    req.on('close', () => {
      clearInterval(keepAlive);
      unsubscribe();
      sseResponses.delete(res);
      const remaining = (sseCountBySession.get(id) || 1) - 1;
      if (remaining <= 0) {
        sseCountBySession.delete(id);
      } else {
        sseCountBySession.set(id, remaining);
      }
      scheduleIdleCheck();
    });
  });

  // Source-only static assets (index.html/css/js, mcp-widget.html/js, icons).
  app.use(
    '/static',
    express.static(STATIC_DIR, {
      setHeaders: (res, filePath) => {
        if (filePath.endsWith('.wasm')) {
          res.set('Content-Type', 'application/wasm');
        }
      },
    })
  );

  // Generated artifacts (viewer wasm/glue, player bundle, ext bundle) served under /wasm. Kept
  // separate from /static so the source tree stays free of build outputs (see scripts/prebuild.js).
  app.use(
    '/wasm',
    express.static(GENERATED_DIR, {
      setHeaders: (res, filePath) => {
        if (filePath.endsWith('.wasm')) {
          res.set('Content-Type', 'application/wasm');
        }
      },
    })
  );

  // Silence the browser's default /favicon.ico probe; we don't ship one and the 404 clutters
  // the Network panel on every reload.
  app.get('/favicon.ico', (req, res) => {
    res.status(204).end();
  });

  // Fallback fonts served for the viewer. The client fetches /fonts/list first, then downloads
  // each referenced file. When no font source is available the list is empty and the client
  // proceeds without registering fonts (text glyphs will render blank).
  app.get('/fonts/list', (req, res) => {
    const entries = currentFontEntries();
    const list = {};
    for (const [role, entry] of Object.entries(entries)) {
      list[role] = entry.name;
    }
    // Surface whether a background download is still working so the client can retry politely.
    res.json({
      fontsDir: resolvedFontsDir,
      fonts: list,
      downloading: useLazyDownload && Object.keys(list).length < 2,
    });
  });

  app.get('/fonts/:name', (req, res) => {
    // Only serve files explicitly enumerated by the current font list to keep the endpoint
    // from turning into a generic file server for the fonts directory.
    const match = Object.values(currentFontEntries()).find((entry) => entry.name === req.params.name);
    if (!match) {
      res.status(404).send('font not found');
      return;
    }
    res.sendFile(match.path);
  });

  // MCP Apps endpoint. CodeBuddy (or any MCP client) connects to /mcp via Streamable HTTP
  // transport. The MCP server exposes 4 tools (preview_pagx, preview_pagx_widget,
  // reload_file, get_document) and 1 HTML resource (ui://pagx-preview/main) so that the host
  // can render a pagx widget directly inside the conversation flow. The MCP server shares the
  // same express app, port, and session state as the rest of pagx-preview.
  //
  // serverBaseUrl is assigned after app.listen resolves the actual port. readResource reads it
  // lazily (via getServerBaseUrl) to inject <base> into the widget HTML, so the widget's relative
  // URLs resolve against the server even when it runs inside a sandbox iframe with opaque origin.
  let serverBaseUrl = '';
  mountMcpServer(app, {
    sessions,
    dropSessions,
    createOrGetSession,
    staticDir: STATIC_DIR,
    generatedDir: GENERATED_DIR,
    getServerBaseUrl: () => serverBaseUrl,
  });

  // Root redirects to the initial session, which is what CLI users hit when the browser opens.
  app.get('/', (req, res) => {
    if (initialSession) {
      res.redirect(`/session/${initialSession.id}/`);
      return;
    }
    res.status(404).send('no default session');
  });

  // Broadcasts an event to every active SSE subscriber. Used by the lazy font download to nudge
  // clients into re-registering their fallback fonts once the download completes.
  function broadcastToAll(event) {
    for (const session of sessions.values()) {
      session.emit(event);
    }
  }

  // Kick off a background download so a globally installed pagx-preview eventually gets fonts
  // even without a libpag checkout. The download is fire-and-forget: server responsiveness
  // never waits on it, and clients poll /fonts/list via SSE-driven retry.
  if (useLazyDownload) {
    (async () => {
      const before = Object.keys(currentFontEntries()).length;
      const result = await ensureAllFonts();
      // If the initial resolveFontsDir() picked null (nothing existed yet) but the cache is now
      // populated, promote it so the /fonts/:name handler can serve out of it.
      if (!resolvedFontsDir && Object.values(result).some((r) => r.path)) {
        resolvedFontsDir = resolveFontsDir(null);
      }
      const after = Object.keys(currentFontEntries()).length;
      if (after > before) {
        broadcastToAll({ type: 'fonts-ready' });
      }
    })().catch((err) => {
      process.stderr.write(`pagx-preview: font download failed: ${err.message}\n`);
    });
  }

  return await new Promise((resolve, reject) => {
    const server = app.listen(port, host, () => {
      const actualPort = server.address().port;
      const displayHost = host === '0.0.0.0' ? 'localhost' : host;
      serverBaseUrl = `http://${displayHost}:${actualPort}`;
      const url = initialSession
        ? `http://${displayHost}:${actualPort}/session/${initialSession.id}/`
        : `http://${displayHost}:${actualPort}/`;
      resolve({
        server,
        port: actualPort,
        host,
        url,
        sessionId: initialSession ? initialSession.id : null,
        // Exposed so a stdio MCP server (see connectStdioMcpServer) can share this in-process
        // preview server's live state instead of talking to it over HTTP.
        sessions,
        dropSessions,
        createOrGetSession,
        staticDir: STATIC_DIR,
        generatedDir: GENERATED_DIR,
        getServerBaseUrl: () => serverBaseUrl,
        get fontsDir() {
          // Reflects the currently-resolved directory: with lazy download, initial startup can
          // return null but a later access (after the download finishes) sees the cache dir.
          return resolvedFontsDir;
        },
        useLazyDownload,
        onIdle(cb) {
          idleCallback = cb;
          scheduleIdleCheck();
        },
        async close() {
          cancelIdleCheck();
          idleCallback = null;
          // Terminate any live SSE streams first so their sockets become drainable.
          for (const res of sseResponses) {
            try {
              res.end();
            } catch (_) {
              // ignore
            }
          }
          sseResponses.clear();
          sseCountBySession.clear();
          // Force-destroy remaining sockets: keep-alive TCP connections would otherwise keep
          // server.close() pending until the peer times out.
          for (const socket of sockets) {
            socket.destroy();
          }
          sockets.clear();
          for (const session of sessions.values()) {
            await session.close();
          }
          sessions.clear();
          dropSessions.clear();
          await new Promise((r) => server.close(() => r()));
        },
      });
    });
    server.on('connection', (socket) => {
      sockets.add(socket);
      socket.on('close', () => sockets.delete(socket));
    });
    server.on('error', reject);
  });
}
