#!/usr/bin/env node
/*
 * Build a standalone browser bundle from lib/browser-snapshot.js.
 *
 * The puppeteer driver in snapshot.js stringifies the snapshot helpers and
 * ships them to a headless Chromium via `page.evaluate`. Those same helpers
 * are pure DOM/CSSOM code and run unmodified in any browser context — the
 * only thing missing is a UMD/ESM wrapper that exposes them as callable
 * functions instead of an IIFE-string + a function passed to puppeteer.
 *
 * Outputs:
 *   dist/html-snapshot.umd.js  — `<script>` tag → window.HtmlSnapshot,
 *                                 also CommonJS / AMD compatible
 *   dist/html-snapshot.esm.js  — `import { takeSnapshot } from "..."`
 *   dist/example.html          — minimal page that loads the UMD bundle
 *                                 and dumps the snapshot for download
 *
 * Run:  npm run build:browser   (or)   node build-browser-bundle.js
 */

'use strict';

const fs = require('fs');
const path = require('path');

const {
  inlineExternalImages,
  inlineCanvases,
  HELPERS_SRC,
  PAYLOAD_CONSTANTS_SRC,
} = require('./lib/browser-snapshot');

const distDir = path.join(__dirname, 'dist');
fs.mkdirSync(distDir, { recursive: true });

const banner = `/*!
 * html-snapshot · browser bundle
 *
 * Generated from tools/html-snapshot/lib/browser-snapshot.js — do not edit
 * by hand; rerun \`npm run build:browser\` after changing the source.
 *
 * Public API (operates on the live document):
 *   takeSnapshot()              -> { html, width, height }
 *   inlineExternalImages()      -> Promise<void>
 *   inlineCanvases()            -> Promise<void>
 *
 * Run \`pagx import --format html\` on the resulting HTML to convert it to
 * PAGX. The snapshot conforms to spec/html_subset.md.
 */`;

// `snapshotMain` (the entry point) and every helper are emitted as plain
// function declarations inside the factory closure, so they hoist to the
// top of the closure scope. The constants block defines Sets/Maps that
// reference helpers by name; hoisting makes those references resolve even
// though the constants appear textually before the helpers in the IIFE
// payload (matching the layout the puppeteer driver uses today).
const factoryBody = `
${HELPERS_SRC}

${PAYLOAD_CONSTANTS_SRC}

${inlineExternalImages.toString()}

${inlineCanvases.toString()}

return {
  takeSnapshot: snapshotMain,
  inlineExternalImages: inlineExternalImages,
  inlineCanvases: inlineCanvases,
};
`;

const umd = `${banner}
(function (root, factory) {
  if (typeof exports === 'object' && typeof module !== 'undefined') {
    module.exports = factory();
  } else if (typeof define === 'function' && define.amd) {
    define([], factory);
  } else {
    root.HtmlSnapshot = factory();
  }
}(typeof globalThis !== 'undefined' ? globalThis : this, function () {
'use strict';
${factoryBody}
}));
`;

const esm = `${banner}

const _module = (function () {
'use strict';
${factoryBody}
}());

export const takeSnapshot = _module.takeSnapshot;
export const inlineExternalImages = _module.inlineExternalImages;
export const inlineCanvases = _module.inlineCanvases;
export default _module;
`;

const exampleHtml = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>html-snapshot · browser bundle demo</title>
  <style>
    :root { color-scheme: light; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
      margin: 0;
      padding: 32px 24px 64px;
      max-width: 760px;
      margin-inline: auto;
      color: #111827;
      background: linear-gradient(180deg, #f8fafc 0%, #eef2ff 100%);
      min-height: 100vh;
    }
    h1 { font-size: 28px; font-weight: 700; margin: 0 0 8px; }
    p.lede { color: #4b5563; line-height: 1.6; margin: 0 0 24px; }
    code, pre { font-family: ui-monospace, "SF Mono", Menlo, monospace; }
    code { background: #e5e7eb; padding: 1px 6px; border-radius: 4px; font-size: 0.9em; }
    .card {
      background: white;
      border: 1px solid #e5e7eb;
      border-radius: 16px;
      padding: 20px 24px;
      box-shadow: 0 4px 12px rgba(15, 23, 42, 0.05);
    }
    .card h2 { margin: 0 0 8px; font-size: 18px; }
    .card p { margin: 0; color: #4b5563; line-height: 1.6; }
    .badge-row { display: flex; gap: 8px; margin-top: 16px; flex-wrap: wrap; }
    .badge {
      display: inline-flex; align-items: center; gap: 4px;
      padding: 4px 10px; border-radius: 999px;
      background: #dbeafe; color: #1d4ed8;
      font-size: 12px; font-weight: 500;
    }
    .badge.green { background: #dcfce7; color: #166534; }
    .badge.amber { background: #fef3c7; color: #92400e; }
    .controls { margin: 24px 0; display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    button {
      padding: 10px 18px; border: none; border-radius: 10px;
      background: #2563eb; color: white;
      font-size: 14px; font-weight: 500; cursor: pointer;
      transition: background 0.15s;
    }
    button:hover { background: #1d4ed8; }
    button:disabled { background: #93c5fd; cursor: progress; }
    a.dl {
      color: #2563eb; text-decoration: none; font-size: 14px;
      border-bottom: 1px solid currentColor;
    }
    pre.out {
      background: #0f172a; color: #cbd5f5;
      padding: 16px; border-radius: 12px;
      font-size: 12px; line-height: 1.5;
      overflow: auto; max-height: 360px;
      white-space: pre-wrap; word-break: break-word;
      margin: 0;
    }
  </style>
</head>
<body>
  <h1>html-snapshot &middot; browser bundle</h1>
  <p class="lede">
    Click <strong>Capture snapshot</strong> below to walk this very page and
    emit a flat, absolute-positioned HTML snapshot that conforms to
    <code>spec/html_subset.md</code>. The output can be saved and fed
    straight into <code>pagx import --format html</code>.
  </p>

  <div class="card">
    <h2>Sample card</h2>
    <p>
      The bundle reads computed styles, freezes inline SVGs, splits text
      into per-line spans, and preserves flex containers when they fit the
      subset. Everything you see here will round-trip into PAGX.
    </p>
    <div class="badge-row">
      <span class="badge">demo</span>
      <span class="badge green">flex preserved</span>
      <span class="badge amber">no puppeteer</span>
    </div>
  </div>

  <div class="controls">
    <button id="run">Capture snapshot</button>
    <a id="dl" class="dl" download="snapshot.html" hidden>Download snapshot.html</a>
  </div>

  <pre class="out" id="out">// click "Capture snapshot" to run</pre>

  <script src="./html-snapshot.umd.js"></script>
  <script>
    const btn = document.getElementById('run');
    const out = document.getElementById('out');
    const dl  = document.getElementById('dl');

    btn.addEventListener('click', async () => {
      btn.disabled = true;
      btn.textContent = 'Capturing…';
      try {
        // Optional: inline cross-origin <img src="https://…"> as data: URIs.
        // No-op when the page has no http(s)-served images.
        await HtmlSnapshot.inlineExternalImages();
        // Optional: capture every <canvas> bitmap as a data: URI so chart
        // libraries (ECharts, Chart.js, …) survive the snapshot. No-op when
        // the page has no canvases.
        await HtmlSnapshot.inlineCanvases();
        const { html, width, height } = HtmlSnapshot.takeSnapshot();
        const head = '// ' + width + 'x' + height + ', ' + html.length + ' bytes\\n';
        const preview = html.length > 4000 ? html.slice(0, 4000) + '\\n// …truncated' : html;
        out.textContent = head + preview;
        const blob = new Blob([html], { type: 'text/html' });
        if (dl.href) URL.revokeObjectURL(dl.href);
        dl.href = URL.createObjectURL(blob);
        dl.hidden = false;
        dl.textContent = 'Download snapshot.html (' + (html.length / 1024).toFixed(1) + ' KB)';
      } catch (err) {
        out.textContent = 'error: ' + (err && err.stack ? err.stack : err);
      } finally {
        btn.disabled = false;
        btn.textContent = 'Capture snapshot';
      }
    });
  </script>
</body>
</html>
`;

fs.writeFileSync(path.join(distDir, 'html-snapshot.umd.js'), umd, 'utf8');
fs.writeFileSync(path.join(distDir, 'html-snapshot.esm.js'), esm, 'utf8');
fs.writeFileSync(path.join(distDir, 'example.html'), exampleHtml, 'utf8');

const sizeKB = (p) => (fs.statSync(p).size / 1024).toFixed(1);
console.log(`html-snapshot: wrote dist/html-snapshot.umd.js (${sizeKB(path.join(distDir, 'html-snapshot.umd.js'))} KB)`);
console.log(`html-snapshot: wrote dist/html-snapshot.esm.js (${sizeKB(path.join(distDir, 'html-snapshot.esm.js'))} KB)`);
console.log(`html-snapshot: wrote dist/example.html`);
