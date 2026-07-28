// MCP Apps widget script for pagx-preview. Loaded as an external ES module so that CSP
// script-src (which allows the server origin via _meta.ui.csp.resourceDomains) permits it —
// inline scripts are blocked by the host's sandbox CSP.
//
// Lifecycle:
// 1. host injects widget HTML into a sandbox iframe
// 2. this module loads, imports App + PAGXPlayer
// 3. app.connect() establishes postMessage bridge with host
// 4. app.ontoolresult fires with { file, sessionId } from preview_pagx's structuredContent
// 5. widget fetches /session/:id/pagx, loads it via PAGXPlayer
// 6. widget opens SSE stream to /session/:id/events for reload events

import { App } from '/wasm/ext/app-with-deps.js';
import { PAGXPlayer } from '/wasm/player/pagx-player.esm.js';

// Server base URL — injected as a global (`__SERVER_BASE__`) when inlined into the widget HTML.
// Falls back to '' (relative paths) when running as an external script in basic-host.
const BASE = (typeof __SERVER_BASE__ !== 'undefined') ? __SERVER_BASE__ : '';

// WASM module factory — loads the pagx-viewer WASM init function and instantiates it.
// Mirrors the exact pattern from static/index.js.
let PAGXInit = null;
async function loadPagxInit() {
  if (PAGXInit) return PAGXInit;
  const infoResp = await fetch(`${BASE}/wasm/viewer/info.json`, { cache: 'no-store' });
  if (!infoResp.ok) throw new Error(`fetch viewer info failed: ${infoResp.status}`);
  const viewerInfo = await infoResp.json();
  const glueUrl = `${BASE}/wasm/viewer/${viewerInfo.glueFile}`;
  const mod = await import(glueUrl);
  PAGXInit = mod.PAGXInit;
  return PAGXInit;
}
async function moduleFactory() {
  // tgfx's emscripten glue has a Safari WebGL2 polyfill in its createContext() that patches
  // canvas instances. In hosts using double-layered sandbox iframes (basic-host, Claude Desktop)
  // the inner iframe's canvas may not have a working getContext. Pre-patch both prototypes so
  // the glue's `if (!canvas.getContextSafariWebGL2Fixed)` check sees a truthy value and skips
  // its problematic patch block entirely.
  for (const Ctor of [
    typeof HTMLCanvasElement !== 'undefined' ? HTMLCanvasElement : null,
    typeof OffscreenCanvas !== 'undefined' ? OffscreenCanvas : null,
  ]) {
    if (Ctor && !Ctor.prototype.getContextSafariWebGL2Fixed) {
      const orig = Ctor.prototype.getContext;
      if (orig) {
        Object.defineProperty(Ctor.prototype, 'getContextSafariWebGL2Fixed', {
          get() { return orig; },
          set() {},
          configurable: true,
        });
      }
    }
  }
  const init = await loadPagxInit();
  return init({ locateFile: (file) => `${BASE}/wasm/viewer/${file}` });
}

const container = document.getElementById('pagx-container');
const statusEl = document.getElementById('status');
const errorEl = document.getElementById('error');

const loadingFallback = document.getElementById('loading-fallback');
if (loadingFallback) loadingFallback.style.display = 'none';

function setStatus(text) {
  statusEl.textContent = text;
}

function showError(text) {
  errorEl.textContent = text;
  errorEl.style.display = 'block';
  container.style.display = 'none';
}

function clearError() {
  errorEl.style.display = 'none';
  container.style.display = 'block';
}

function applyTheme(theme) {
  if (!theme) return;
  document.documentElement.setAttribute('data-theme', theme);
  document.documentElement.style.colorScheme = theme;
}

const app = new App({
  name: 'pagx-preview-widget',
  version: '1.0.0',
  autoResize: true,
});

let player = null;
let currentSessionId = null;
let eventSource = null;

async function resolveResource(rel) {
  const buf = await fetch(`${BASE}/session/${currentSessionId}/resources/${encodeURIComponent(rel)}`)
    .then((r) => (r.ok ? r.arrayBuffer() : null));
  return buf ? new Uint8Array(buf) : null;
}

async function registerFonts(view) {
  const fontList = await fetch(`${BASE}/fonts/list`).then((r) => r.json());
  if (!fontList.fonts) return;
  const fonts = fontList.fonts;
  const primaryName = fonts.primary || fonts.regular;
  const emojiName = fonts.emoji;
  // registerFonts resets fontConfig on each call, so both fonts must be passed together.
  let primaryBytes = new Uint8Array(0);
  let emojiBytes = new Uint8Array(0);
  if (primaryName) {
    const data = await fetch(`${BASE}/fonts/${encodeURIComponent(primaryName)}`).then((r) =>
      r.ok ? r.arrayBuffer() : null,
    );
    if (data) primaryBytes = new Uint8Array(data);
  }
  if (emojiName) {
    const data = await fetch(`${BASE}/fonts/${encodeURIComponent(emojiName)}`).then((r) =>
      r.ok ? r.arrayBuffer() : null,
    );
    if (data) emojiBytes = new Uint8Array(data);
  }
  if (primaryBytes.length || emojiBytes.length) {
    view.registerFonts(primaryBytes, emojiBytes);
  }
}

// Upload document summary so the MCP get_document tool can answer AI queries.
async function uploadDocumentSummary(view) {
  try {
    const duration = view.durationMicros();
    const width = view.contentWidth();
    const height = view.contentHeight();
    const summary = {
      nodeCount: 0,
      width,
      height,
      duration,
      frameRate: view.frameRate(),
    };
    await fetch(`${BASE}/session/${currentSessionId}/document`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(summary),
    });
  } catch (e) {
    // Non-fatal: get_document will return null, AI falls back to reading the XML file.
  }
}

function openEventStream(sessionId) {
  if (eventSource) eventSource.close();
  eventSource = new EventSource(`${BASE}/session/${sessionId}/events`);
  eventSource.addEventListener('reload', async () => {
    setStatus('Reloading...');
    try {
      const buf = await fetch(`${BASE}/session/${sessionId}/pagx`).then((r) => r.arrayBuffer());
      await player.load(new Uint8Array(buf), {
        registerFonts,
        resolveResource,
        preserveCurrentTime: true,
      });
      setStatus('');
    } catch (err) {
      showError(`Reload failed: ${err.message}`);
    }
  });
  eventSource.addEventListener('focus', () => {});
  eventSource.onerror = () => {};
}

app.ontoolresult = async (r) => {
  // structuredContent may be at r.structuredContent (MCP Apps spec) or directly at r (some hosts
  // pass the full CallToolResult as-is). Try both paths.
  const sc = r?.structuredContent ?? r;
  if (!sc || !sc.sessionId) {
    showError('missing sessionId in structuredContent');
    return;
  }
  currentSessionId = sc.sessionId;
  clearError();
  setStatus('Loading...');

  try {
    if (!player) {
      player = new PAGXPlayer({ container, moduleFactory, iconBaseUrl: `${BASE}/static/icons/` });
    }
    const buf = await fetch(`${BASE}/session/${sc.sessionId}/pagx`).then((res) => res.arrayBuffer());
    await player.load(new Uint8Array(buf), {
      registerFonts,
      resolveResource,
      preserveCurrentTime: true,
    });
    await uploadDocumentSummary(player.getView());
    openEventStream(sc.sessionId);
    setStatus('');
  } catch (err) {
    showError(`Failed to load pagx: ${err.message}`);
  }
};

app.onhostcontextchanged = (ctx) => {
  applyTheme(ctx?.theme);
};

await app.connect();
applyTheme(app.hostContext?.theme);
