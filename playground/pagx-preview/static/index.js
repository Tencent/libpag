/*
 * Tencent is pleased to support the open source community by making libpag available.
 * Copyright (C) 2026 Tencent. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

// Preview client: instantiates the shared PAGXPlayer component (canvas / gestures / playback
// bar / toolbar) and layers preview-only chrome on top - session-aware fetches, SSE-driven
// reloads, drop-to-open, status pill, refresh banner. Everything player-related now lives in
// pagx-player; this file is deliberately kept small and preview-specific.

import { PAGXPlayer } from '/wasm/player/pagx-player.esm.js';

const SESSION_MATCH = window.location.pathname.match(/^\/session\/([^/]+)\//);
if (!SESSION_MATCH) {
  // No player yet at this point, so fall back to a plain full-screen text overlay for the
  // one bad-URL case; the normal status pill is a player concern and is unavailable here.
  const el = document.createElement('div');
  el.textContent = 'Invalid session URL';
  el.style.cssText =
    'position:fixed;inset:0;display:flex;align-items:center;justify-content:center;' +
    'color:#fff;background:rgba(220,53,69,0.85);font-size:14px;z-index:9999;';
  document.body.appendChild(el);
  throw new Error('pagx-preview: cannot determine session id from URL');
}
const SESSION_ID = SESSION_MATCH[1];
const SESSION_BASE = `/session/${SESSION_ID}`;

const container = document.getElementById('container');
const refreshBanner = document.getElementById('refresh-banner');
const dropOverlay = document.getElementById('drop-overlay');
const dropHint = document.getElementById('drop-hint');

// Drop sessions are identified server-side by a `drop-` prefix on the random session id. Those
// sessions are memory-only and can't be watched for filesystem changes (the browser never gives
// us the file's absolute path), so surface a persistent hint pointing users at the CLI when we
// detect we're rendering one of them.
if (/\/session\/drop-/.test(window.location.pathname)) {
  dropHint?.classList.remove('hidden');
  document.getElementById('drop-hint-close')?.addEventListener('click', () => {
    dropHint?.classList.add('hidden');
  });
}

// ---------- Status pill + refresh banner ----------

// Thin wrapper around player.showStatus so preview code keeps its concise call sites. Declared
// as a function (hoisted) because a few status calls sit above the player instantiation below.
// The player instance is initialized before any of these calls actually run (all callers are
// inside async functions or event handlers), so `player` is guaranteed to exist by then.
function showStatus(text, isError = false, autoHideMs = 0) {
  player.showStatus(text, {
    kind: isError ? 'error' : 'info',
    autoHideMs,
  });
}

let refreshBannerTimer = null;
function showRefreshBanner(text, autoHideMs = 1600) {
  refreshBanner.textContent = text;
  refreshBanner.classList.remove('hidden');
  clearTimeout(refreshBannerTimer);
  if (autoHideMs > 0) {
    refreshBannerTimer = setTimeout(() => refreshBanner.classList.add('hidden'), autoHideMs);
  }
}

// ---------- Viewer module loading (glue file + wasm) ----------

let PAGXInit = null;
let viewerInfo = null;
let fontsRegistered = false;

async function loadPagxInit() {
  if (PAGXInit) return PAGXInit;
  const infoResp = await fetch('/wasm/viewer/info.json', { cache: 'no-store' });
  if (!infoResp.ok) throw new Error(`fetch viewer info failed: ${infoResp.status}`);
  viewerInfo = await infoResp.json();
  const glueUrl = `/wasm/viewer/${viewerInfo.glueFile}`;
  const mod = await import(glueUrl);
  PAGXInit = mod.PAGXInit;
  return PAGXInit;
}

async function moduleFactory() {
  const init = await loadPagxInit();
  return init({
    locateFile: (file) => `/wasm/viewer/${file}`,
  });
}

// ---------- Font registration ----------

// Downloads the primary + emoji fallback fonts from the server and hands them to the viewer.
// Empty buffers are acceptable (the C++ side treats them as "no override") so a partial
// availability (only primary, only emoji) still works. When the server reports that a lazy
// background download is in progress, an SSE 'fonts-ready' event later triggers another call
// to this function so the current view picks up the fonts without a manual refresh.
async function registerFallbackFonts(view) {
  if (fontsRegistered) return;
  try {
    const listResp = await fetch('/fonts/list', { cache: 'no-store' });
    if (!listResp.ok) return;
    const info = await listResp.json();
    const fonts = info?.fonts || {};
    const primaryName = fonts.primary;
    const emojiName = fonts.emoji;
    if (!primaryName && !emojiName) {
      if (info?.downloading) {
        console.info('pagx-preview: fonts downloading in background; will retry on fonts-ready.');
      } else {
        console.warn('pagx-preview: no fallback fonts available; text will render blank.');
      }
      return;
    }
    const [primaryData, emojiData] = await Promise.all([
      fetchFontBytes(primaryName),
      fetchFontBytes(emojiName),
    ]);
    view.registerFonts(primaryData, emojiData);
    // Consider registration complete only when both slots are populated; partial results still
    // allow a retry after the background download fills in the missing role.
    if (primaryName && emojiName) fontsRegistered = true;
  } catch (err) {
    console.warn('pagx-preview: font registration failed', err);
  }
}

async function fetchFontBytes(name) {
  if (!name) return new Uint8Array(0);
  const resp = await fetch(`/fonts/${encodeURIComponent(name)}`, { cache: 'force-cache' });
  if (!resp.ok) {
    console.warn(`pagx-preview: font ${name} -> ${resp.status}`);
    return new Uint8Array(0);
  }
  return new Uint8Array(await resp.arrayBuffer());
}

// ---------- Resource cache and invalidation ----------

// Client-side cache for external resource bytes keyed by the relative path the pagx references.
// External resources rarely change compared to the pagx XML itself (AI edits typically only
// tweak colors/positions/text), so caching them saves the whole fetch + arrayBuffer decode
// round-trip on every reload. The wasm loadFileData() call still has to run every time because
// parsePAGX() resets the underlying document.
const resourceCache = new Map();

// Absolute paths (as reported by the server's `reload` event) whose bytes we need to re-fetch
// on the next loadPAGX. Populated when the SSE payload names a specific changed file so the
// resource cache below only invalidates that one entry instead of dumping everything.
let invalidatedResources = new Set();

function consumeInvalidations() {
  const invalidations = invalidatedResources;
  invalidatedResources = new Set();
  if (invalidations === null) {
    resourceCache.clear();
  } else if (invalidations.size > 0) {
    // We can't map absolute watcher paths back to the relative paths the client cache uses
    // without rebuilding the server's rel->abs table here, so simplify: if any invalidated
    // path doesn't look like a .pagx source, treat it as "some resource changed" and drop
    // the whole resource cache. When *only* .pagx files show up we keep every cached resource.
    const nonPagx = [...invalidations].some((p) => !p.toLowerCase().endsWith('.pagx'));
    if (nonPagx) resourceCache.clear();
  }
}

// External paths in PAGX are '/'-separated and may contain characters that must be encoded per
// URL segment while preserving the segment structure.
function encodeExternalPath(rel) {
  return rel
    .split('/')
    .map((seg) => encodeURIComponent(seg))
    .join('/');
}

// Records how many cache hits occurred during the current load pipeline. The player calls the
// resolveResource callback for every relative path, and we want to log the aggregate count in
// the summary line, so we track it on a module-scoped counter rather than plumbing an extra
// return value through the resolver signature.
let cacheHitsThisLoad = 0;

async function resolveResource(rel) {
  const cached = resourceCache.get(rel);
  if (cached) {
    cacheHitsThisLoad += 1;
    return cached;
  }
  try {
    const resp = await fetch(
      `${SESSION_BASE}/resources/${encodeExternalPath(rel)}`,
      { cache: 'no-store' }
    );
    if (!resp.ok) {
      console.warn(`pagx-preview: resource ${rel} -> ${resp.status}`);
      return null;
    }
    const buf = new Uint8Array(await resp.arrayBuffer());
    resourceCache.set(rel, buf);
    return buf;
  } catch (err) {
    console.warn(`pagx-preview: resource ${rel} failed`, err);
    return null;
  }
}

// ---------- Performance measurement ----------

// Wraps the User Timing API so every load cycle marks distinct start/end points that the
// browser Performance panel can visualize, plus emits a compact console summary that's useful
// when DevTools is closed.
let perfCycleId = 0;
function makePerf(label) {
  const cycle = ++perfCycleId;
  const total = `${label}.${cycle}.total`;
  performance.mark(`${total}.start`);
  const stages = [];
  return {
    begin(stage) {
      performance.mark(`${label}.${cycle}.${stage}.start`);
    },
    end(stage) {
      performance.mark(`${label}.${cycle}.${stage}.end`);
      const measure = performance.measure(
        `${label}.${cycle}.${stage}`,
        `${label}.${cycle}.${stage}.start`,
        `${label}.${cycle}.${stage}.end`
      );
      stages.push({ stage, dur: measure.duration });
    },
    summarize() {
      performance.mark(`${total}.end`);
      const measure = performance.measure(total, `${total}.start`, `${total}.end`);
      const stageStrs = stages.map((s) => `${s.stage}=${s.dur.toFixed(1)}ms`).join(' ');
      // eslint-disable-next-line no-console
      console.log(
        `[pagx-preview] loadPAGX total=${measure.duration.toFixed(1)}ms | ${stageStrs}`
      );
    },
  };
}

// Give the browser one frame + one microtask hop so any pending status/DOM updates paint
// before the caller launches another multi-second synchronous wasm call. Two-step wait: rAF
// alone can coalesce with the same task, chaining a subsequent setTimeout(0) makes the paint
// definitely land first. Cheap when the browser is idle; the cost is negligible compared to
// parsePAGX/buildLayers timings on large documents.
function yieldToBrowser() {
  return new Promise((resolve) => {
    requestAnimationFrame(() => setTimeout(resolve, 0));
  });
}

// ---------- Player instance ----------

const player = new PAGXPlayer({
  container,
  moduleFactory,
  iconBaseUrl: '/static/icons/',
  // Source editor is enabled here so `L` opens the panel and the toolbar shows the </>
  // button. Apply routes the edited XML back through the wasm pipeline; Save downloads a
  // copy since the preview server never persists edits (the file on disk is authoritative
  // and the user is expected to edit it directly for durable changes).
  enableEditor: true,
  editorCallbacks: {
    // Async apply: the editor awaits this and keeps the buttons disabled + "Applying..."
    // status visible until the whole pipeline settles. Errors returned as a non-empty string
    // are rendered by the editor as a red status pill; a thrown error is treated the same
    // way. `silent: true` prevents the internal loadPAGX status ("Parsing.../Loaded") from
    // fighting with the editor's own "Applying.../Changes applied" for the shared pill.
    onApply: async (xmlText) => {
      try {
        const bytes = new TextEncoder().encode(xmlText);
        await loadPAGXFromBytes(bytes, { silent: true });
        return '';
      } catch (err) {
        return `Apply failed: ${err && err.message ? err.message : String(err)}`;
      }
    },
    // Preview owns a real filesystem path (unlike the playground which downloads a copy), so
    // Save writes back to the entry pagx via the server. The write triggers the session's
    // file watcher, which broadcasts a `reload` SSE - the client picks that up on the same
    // loop it uses for external IDE edits, so no manual refresh is needed here.
    onSave: async (xmlText) => {
      try {
        const resp = await fetch(`${SESSION_BASE}/pagx`, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/xml' },
          body: xmlText,
        });
        if (!resp.ok) {
          const info = await resp.json().catch(() => ({ error: `status ${resp.status}` }));
          return `Save failed: ${info.error}`;
        }
        return '';
      } catch (err) {
        return `Save failed: ${err && err.message ? err.message : String(err)}`;
      }
    },
  },
});

// ---------- Load pipeline ----------

let loading = false;
let reloadQueued = false;
// Captures the strategy chosen by the most recent player.load() call. Populated by the 'loaded'
// event listener installed below and consumed once per load in loadPAGXFromBytes. A plain module
// scoped variable is enough because player.load()'s internal generation gate guarantees a single
// 'loaded' dispatch per completed load and loadPAGXFromBytes drains this value in the same
// microtask that resolves the await.
let lastLoadStrategy = 'fullReplace';
player.addEventListener('loaded', (event) => {
  // 'strategy' was added by the V0 update pipeline; older host builds still fire 'loaded'
  // without it, in which case we assume the historical full-replace behavior for backward
  // compatibility.
  lastLoadStrategy = event.detail.strategy || 'fullReplace';
});

// Wraps player.load() with preview chrome (status/perf/invalidation/external-path report).
// Split from loadPAGX() so the editor's Apply callback can feed edited XML through the exact
// same pipeline as an SSE-driven reload without re-fetching from the server.
// `silent` suppresses the pipeline's own status updates so a caller that already owns the
// status slot (e.g. the editor showing "Changes applied") isn't fighting with "Parsing..."
// and "Loaded" for the same pill - which would otherwise leave the caller's message visible
// for only a fraction of a second before the wasm-load path overwrites it.
async function loadPAGXFromBytes(pagxBuf, { silent = false } = {}) {
  if (loading) {
    reloadQueued = true;
    return;
  }
  loading = true;
  cacheHitsThisLoad = 0;
  consumeInvalidations();

  const perf = makePerf('loadPAGX');
  try {
    if (!silent) showStatus('Parsing document...');
    await yieldToBrowser();

    perf.begin('load');
    // The player performs updatePAGX -> font registration -> resource fetch -> buildLayers ->
    // first draw internally. Font registration and resource resolution are handled via the
    // callbacks below. The pagx-preview stage-level timing that the old flat pipeline reported
    // is now folded into the single 'load' stage; the component's internal marks still show up
    // in the Performance panel for detailed profiling.
    await player.load(pagxBuf, {
      registerFonts: async (view) => {
        // Font registration is idempotent and fast (bytes come from HTTP cache after the first
        // hit), so calling it on every reload keeps behavior consistent whether or not the
        // wasm view was recycled by the player.
        await registerFallbackFonts(view);
      },
      resolveResource,
      preserveCurrentTime: true,
    });
    perf.end('load');

    // Skip the noise-side-effects entirely on a noChange short-circuit. The bytes were
    // byte-identical to the previously accepted document, so the external path list can't
    // have changed and re-POSTing it would only expand the server's watcher noise. The
    // "Loaded" pill is also suppressed: chokidar's rename/attr/content bursts routinely fire
    // 2-3 SSE reloads for a single save, and after V0 only the first one produces a real
    // update — flashing three "Loaded" pills for what the user perceives as a single save
    // is exactly what the equality short-circuit is meant to fix.
    if (lastLoadStrategy === 'noChange') {
      perf.summarize();
      // eslint-disable-next-line no-console
      console.log('[pagx-preview] noChange short-circuit');
      return;
    }

    // Report the resource list so the server extends its watch to referenced images/fonts.
    // Fire-and-forget: only used to widen the file watcher scope on the server side.
    try {
      const view = player.getView();
      const externalPaths = view ? view.getExternalFilePaths() : [];
      fetch(`${SESSION_BASE}/resources`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ paths: externalPaths }),
      }).catch(() => {});
      // eslint-disable-next-line no-console
      console.log(
        `[pagx-preview] resources=${externalPaths.length} (cache hits=${cacheHitsThisLoad})`
      );
      // Upload a document summary so the MCP get_document tool can answer AI queries without a
      // client round-trip.
      if (view) {
        const summary = {
          width: view.contentWidth(),
          height: view.contentHeight(),
          duration: view.durationMicros(),
          frameRate: view.frameRate(),
        };
        fetch(`${SESSION_BASE}/document`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(summary),
        }).catch(() => {});
      }
    } catch (_) {
      // getView() may return null in a very narrow race (destroy fired mid-load); ignore.
    }

    perf.summarize();
    if (!silent) showStatus('Loaded', false, 1200);
  } catch (err) {
    console.error('pagx-preview: load failed', err);
    if (silent) {
      // Silent callers (editor Apply) own their own status pill; rethrow so they can surface
      // the error via the editor's own feedback path instead of the pipeline stealing focus
      // with a global "Load failed" pill.
      throw err;
    }
    showStatus(`Load failed: ${err.message || err}`, true);
  } finally {
    loading = false;
    if (reloadQueued) {
      reloadQueued = false;
      // Refetch the server's current bytes rather than replaying the buffer we just loaded:
      // a reload arriving while a load was in flight means the file changed again on disk.
      loadPAGX();
    }
  }
}

// Server-fetch wrapper around loadPAGXFromBytes(). Used by boot, SSE reload, and every other
// "re-fetch and render" path.
async function loadPAGX() {
  try {
    showStatus('Loading document...');
    const perf = makePerf('fetchPagx');
    perf.begin('fetch');
    const pagxResp = await fetch(`${SESSION_BASE}/pagx`, { cache: 'no-store' });
    if (!pagxResp.ok) throw new Error(`fetch pagx failed: ${pagxResp.status}`);
    const pagxBuf = new Uint8Array(await pagxResp.arrayBuffer());
    perf.end('fetch');
    perf.summarize();
    await loadPAGXFromBytes(pagxBuf);
  } catch (err) {
    console.error('pagx-preview: fetch failed', err);
    showStatus(`Fetch failed: ${err.message || err}`, true);
  }
}

// ---------- SSE (server-sent events) ----------

// Set to true when a 'reload' arrives while the tab is hidden. On visibilitychange -> visible
// we consume the flag and refresh once, so a background tab doesn't burn cycles reloading
// (and potentially reloading again while still loading) while its user isn't looking.
let pendingBackgroundReload = false;

function connectSSE() {
  const es = new EventSource(`${SESSION_BASE}/events`);
  es.addEventListener('reload', (ev) => {
    // The server emits payloads like {type:'reload', file:'/abs/path', event:'change'}. `file`
    // may point at the pagx itself or at an external resource; either way, remember it so the
    // resource cache can skip untouched files. Silently degrade if the payload is malformed.
    try {
      const data = JSON.parse(ev.data);
      if (typeof data?.file === 'string' && data.file.length > 0) {
        invalidatedResources.add(data.file);
      }
    } catch (_) {
      // No payload / bad JSON: fall back to a full cache flush so we don't render stale bytes.
      invalidatedResources = null;
    }
    if (document.hidden) {
      // Coalesce every reload received while hidden into a single refresh on return.
      pendingBackgroundReload = true;
      return;
    }
    showStatus('Reloading...');
    loadPAGX();
  });
  es.addEventListener('focus', () => {
    // Browsers routinely ignore window.focus() outside a user gesture; call it anyway so the
    // CLI's "reuse existing tab" flow at least has a chance to work on browsers that allow it.
    try {
      window.focus();
    } catch (_) {
      // ignore
    }
  });
  es.addEventListener('fonts-ready', async () => {
    // The background font download finished. Re-register so the currently displayed PAGX picks
    // up the newly available glyphs; then repaint by re-running the load pipeline.
    const view = player.getView();
    if (view) await registerFallbackFonts(view);
    if (fontsRegistered) {
      showStatus('Fonts loaded', false, 1200);
      loadPAGX();
    }
  });
  es.onerror = () => {
    if (es.readyState === EventSource.CLOSED) {
      showStatus('Disconnected', true);
    }
  };
  return es;
}

document.addEventListener('visibilitychange', () => {
  // Apply any file changes that arrived while the tab was hidden. The banner makes the update
  // visible enough that the user notices even if the visual delta is subtle. The player itself
  // handles start()/stop() on visibility change internally.
  if (!document.hidden && pendingBackgroundReload) {
    pendingBackgroundReload = false;
    showRefreshBanner('File updated, refreshing...');
    loadPAGX();
  }
});

// ---------- Drag & drop for opening additional PAGX files ----------

let dragCounter = 0;

function isPagxDrag(e) {
  const items = e.dataTransfer?.items;
  if (!items) return false;
  for (const item of items) {
    if (item.kind === 'file') return true;
  }
  return false;
}

window.addEventListener('dragenter', (e) => {
  if (!isPagxDrag(e)) return;
  e.preventDefault();
  dragCounter++;
  dropOverlay.classList.add('active');
});
window.addEventListener('dragover', (e) => {
  if (!isPagxDrag(e)) return;
  // preventDefault is required for the browser to treat the element as a drop target.
  e.preventDefault();
});
window.addEventListener('dragleave', (e) => {
  if (!isPagxDrag(e)) return;
  dragCounter = Math.max(0, dragCounter - 1);
  if (dragCounter === 0) dropOverlay.classList.remove('active');
});
window.addEventListener('drop', async (e) => {
  if (!isPagxDrag(e)) return;
  e.preventDefault();
  dragCounter = 0;
  dropOverlay.classList.remove('active');
  const file = e.dataTransfer?.files?.[0];
  if (!file) return;
  if (!file.name.toLowerCase().endsWith('.pagx')) {
    showStatus('Only .pagx files can be dropped', true, 2500);
    return;
  }
  try {
    const buf = await file.arrayBuffer();
    // Browsers don't hand us the absolute filesystem path, so live watch isn't possible for
    // dropped files. The server keeps the bytes in memory for a one-shot preview.
    const resp = await fetch(`/session/drop?name=${encodeURIComponent(file.name)}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/octet-stream' },
      body: buf,
    });
    if (!resp.ok) throw new Error(`server returned ${resp.status}`);
    const info = await resp.json();
    const target = window.location.origin + info.url;
    window.open(target, '_blank', 'noopener');
  } catch (err) {
    console.error('pagx-preview: drop upload failed', err);
    showStatus(`Drop failed: ${err.message || err}`, true, 3000);
  }
});

// ---------- Boot ----------

(async function main() {
  await loadPAGX();
  connectSSE();
})();
