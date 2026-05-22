// Icon-font → inline SVG conversion.
//
// Many design templates depend on icon webfonts (Phosphor, Material Icons,
// Font Awesome, Lucide, Remix, …). Their visual glyphs live in a TTF/WOFF2
// resource hosted via `@font-face` and are inserted into the DOM through
// `::before { content: "\eXXX" }` pseudo-elements. The snapshot's default
// pseudo-text path (renderPseudoTextLeaf in browser-snapshot.js) preserves
// the codepoint + `font-family: "Phosphor"`, but PAGX's renderer can only
// resolve the font through `tgfx::Typeface::MakeFromName` — i.e. it has to
// find the face on the host system. The PAGX file itself does not embed the
// icon font, so the rendering only succeeds on machines that happen to have
// the same icon font installed.
//
// This module bypasses that fragility by extracting each used glyph as a
// vector path and inlining it as an `<svg><path/></svg>`. The snapshot
// walker then renders the SVG via its existing inline-SVG path
// (renderInlineIconSvg), which is fully self-contained and survives any
// downstream PAGX file move.
//
// Pipeline:
//   1. browser-side: walk every leaf element with a single PUA pseudo
//      character whose computed `font-family` matches a registered
//      `@font-face` rule. Tag each host with `data-icon-target-id="…"` and
//      report `{ id, codepoint, fontFamily, fontUrl, format }`.
//   2. node-side: download each unique font URL (woff2 → wawoff2 → ttf →
//      opentype.js), extract the codepoint's `glyph.path`, and emit a
//      `<svg viewBox="0 0 unitsPerEm unitsPerEm">` with `currentColor` fill.
//   3. browser-side: re-find each tagged host and attach the SVG markup as
//      `data-snapshot-icon-svg`; remove the temp `data-icon-target-id`.
//
// The snapshot walker (browser-snapshot.js) checks for
// `data-snapshot-icon-svg` and routes via `renderInlineIconSvg` instead of
// the pseudo-text leaf, sizing the SVG to the host's measured rect.

'use strict';

const fsp = require('fs').promises;

let opentype = null;
let wawoff2 = null;
function loadDeps() {
  if (!opentype) opentype = require('opentype.js');
  if (!wawoff2) wawoff2 = require('wawoff2');
  return { opentype, wawoff2 };
}

// ===== Browser-side helpers (serialised into puppeteer page.evaluate) =====
//
// Each fn below is `.toString()`-able and self-contained (no closures over
// node-side state). They are concatenated into a single IIFE for the
// puppeteer driver to ship to Chromium, and exported individually so the
// browser bundle (build-browser-bundle.js) can expose them as standalone
// hooks for callers running outside of node.

// Walk `document.styleSheets` for `@font-face` rules and return a
// JSON-safe `{ family: { url, format } }` map.
//
// `cssRules` access throws `SecurityError` on cross-origin stylesheets
// that did not opt in via `crossorigin="anonymous"` — which is most
// stylesheets loaded via plain `<link href="https://cdn.example/...">`.
// For each such sheet we issue a regular `fetch(href)` and parse the
// returned CSS text manually; nearly every font CDN (unpkg, cdnjs,
// jsdelivr, googlefonts) sends `Access-Control-Allow-Origin: *` on the
// stylesheet response, so the fallback succeeds without page-side
// configuration. Sheets that fail both paths are silently skipped (the
// snapshot will keep emitting a font-named span for those glyphs).
//
// `font-family` values are unquoted; `src` URLs are resolved against the
// stylesheet's own `href` so relative paths in CDN-hosted stylesheets
// produce absolute, fetchable URLs.
async function browserCollectFontFaceMap() {
  // Format-rank lookup for picking the most-decode-friendly source. WOFF2
  // is preferred (smallest, modern), then WOFF (zlib-only), then raw
  // SFNT containers. `truetype`/`opentype` are aliases for ttf/otf.
  function formatRank(f) {
    if (f === 'woff2') return 1;
    if (f === 'woff') return 2;
    if (f === 'ttf' || f === 'truetype') return 3;
    if (f === 'otf' || f === 'opentype') return 4;
    return 5;
  }
  function parseSrcList(srcRaw, sheetBase) {
    const urls = [];
    const re = /url\(\s*(?:"([^"]+)"|'([^']+)'|([^)]+?))\s*\)(?:\s*format\(\s*["']?([^"')]+)["']?\s*\))?/g;
    let m;
    while ((m = re.exec(srcRaw)) !== null) {
      const u = (m[1] || m[2] || m[3] || '').trim();
      const fmt = (m[4] || '').trim().toLowerCase();
      if (u) urls.push({ url: u, format: fmt });
    }
    if (urls.length === 0) return null;
    urls.sort(function (a, b) { return formatRank(a.format) - formatRank(b.format); });
    const best = urls[0];
    let absUrl = best.url;
    try { absUrl = new URL(best.url, sheetBase).href; } catch (_) { /* keep raw */ }
    return { url: absUrl, format: best.format };
  }
  function parseFontFaceFromText(cssText, sheetBase) {
    if (!cssText) return [];
    const stripped = cssText.replace(/\/\*[\s\S]*?\*\//g, '');
    const out = [];
    const ffRe = /@font-face\s*\{([^}]*)\}/gi;
    let m;
    while ((m = ffRe.exec(stripped)) !== null) {
      const body = m[1];
      const fm = body.match(/font-family\s*:\s*([^;]+)/i);
      const sm = body.match(/src\s*:\s*([^;]+)/i);
      if (!fm || !sm) continue;
      const family = fm[1].trim().replace(/^["']|["']$/g, '');
      const parsed = parseSrcList(sm[1], sheetBase);
      if (family && parsed) out.push({ family: family, url: parsed.url, format: parsed.format });
    }
    return out;
  }
  const map = {};
  const inaccessible = [];
  const sheets = document.styleSheets ? Array.from(document.styleSheets) : [];
  for (const sheet of sheets) {
    let rules;
    try { rules = sheet.cssRules; } catch (_) {
      if (sheet.href) inaccessible.push(sheet.href);
      continue;
    }
    if (!rules) continue;
    const sheetBase = sheet.href || document.baseURI;
    for (const rule of Array.from(rules)) {
      // CSSFontFaceRule: rule.type === 5. Newer browsers expose
      // CSSFontFaceRule as a class; the type-number check works on both.
      if (rule.type !== 5) continue;
      const familyRaw = (rule.style.getPropertyValue('font-family') || '').trim();
      const srcRaw = rule.style.getPropertyValue('src') || '';
      if (!familyRaw || !srcRaw) continue;
      const family = familyRaw.replace(/^["']|["']$/g, '');
      const parsed = parseSrcList(srcRaw, sheetBase);
      // First-write wins: a duplicate `@font-face` (e.g. weight variants
      // sharing the same family) keeps the first source — good enough
      // for icon fonts (single regular weight) and avoids round-tripping
      // a heavier italic/bold variant.
      if (parsed && !map[family]) map[family] = parsed;
    }
  }
  for (const href of inaccessible) {
    try {
      const res = await fetch(href, { mode: 'cors', credentials: 'omit' });
      if (!res.ok) continue;
      const css = await res.text();
      const entries = parseFontFaceFromText(css, href);
      for (const e of entries) {
        if (!map[e.family]) map[e.family] = { url: e.url, format: e.format };
      }
    } catch (_) { /* CORS or network — skip */ }
  }
  return map;
}

// Walk every leaf element in the document and report icon-font candidates.
// A candidate is an element with no element children, no direct text, and a
// single PUA character in its `::before` (or `::after`) `content` whose
// computed `font-family` matches an entry in the `@font-face` map. Each
// candidate is tagged with a unique `data-icon-target-id` attribute so the
// node-side resolver can route the produced SVG back to it.
//
// PUA-only filtering (U+E000–U+F8FF) is the standard convention for icon
// webfonts (Phosphor, Material Icons, Font Awesome, …) and avoids
// converting genuine pseudo-text content (e.g. `content: "—"`) into
// rendering glyph paths.
//
// Async because cross-origin stylesheets routinely deny `cssRules` access
// (Chromium throws `SecurityError` when the response was loaded without an
// opt-in `crossorigin="anonymous"` attribute). For each such sheet we fall
// back to fetching the CSS text via `window.fetch` — the network response
// almost always carries `Access-Control-Allow-Origin: *` even when the
// stylesheet `<link>` itself didn't request CORS — and parse the
// `@font-face` declarations with a small regex pipeline. This is the only
// way to discover the Phosphor / FontAwesome / Material Icons URL when the
// snapshot loads them straight off `unpkg.com` / `cdnjs.com` without any
// page-side configuration.
async function browserCollectIconFontTargets() {
  function formatRank(f) {
    if (f === 'woff2') return 1;
    if (f === 'woff') return 2;
    if (f === 'ttf' || f === 'truetype') return 3;
    if (f === 'otf' || f === 'opentype') return 4;
    return 5;
  }
  function parseSrcList(srcRaw, sheetBase) {
    const urls = [];
    const re = /url\(\s*(?:"([^"]+)"|'([^']+)'|([^)]+?))\s*\)(?:\s*format\(\s*["']?([^"')]+)["']?\s*\))?/g;
    let m;
    while ((m = re.exec(srcRaw)) !== null) {
      const u = (m[1] || m[2] || m[3] || '').trim();
      const fmt = (m[4] || '').trim().toLowerCase();
      if (u) urls.push({ url: u, format: fmt });
    }
    if (urls.length === 0) return null;
    urls.sort(function (a, b) { return formatRank(a.format) - formatRank(b.format); });
    const best = urls[0];
    let absUrl = best.url;
    try { absUrl = new URL(best.url, sheetBase).href; } catch (_) { /* keep raw */ }
    return { url: absUrl, format: best.format };
  }
  // Parse `@font-face { ... }` blocks out of a raw CSS string. Used as a
  // fallback when `cssRules` is denied by CORS — Chromium still loaded
  // and applied the stylesheet (so the page renders), but JS can't
  // introspect it. Regex parsing is intentionally permissive: the only
  // tokens we care about are `font-family` and `src`, both of which use
  // the same syntax that `cssRules` would yield. Comments are stripped
  // first so a `/* src: url(...) */` decoy doesn't mislead the matcher.
  function parseFontFaceFromText(cssText, sheetBase) {
    if (!cssText) return [];
    const stripped = cssText.replace(/\/\*[\s\S]*?\*\//g, '');
    const out = [];
    const ffRe = /@font-face\s*\{([^}]*)\}/gi;
    let m;
    while ((m = ffRe.exec(stripped)) !== null) {
      const body = m[1];
      const fm = body.match(/font-family\s*:\s*([^;]+)/i);
      const sm = body.match(/src\s*:\s*([^;]+)/i);
      if (!fm || !sm) continue;
      const family = fm[1].trim().replace(/^["']|["']$/g, '');
      const parsed = parseSrcList(sm[1], sheetBase);
      if (family && parsed) out.push({ family: family, url: parsed.url, format: parsed.format });
    }
    return out;
  }
  async function collectFontFaceMap() {
    const map = {};
    const inaccessible = [];
    const sheets = document.styleSheets ? Array.from(document.styleSheets) : [];
    for (const sheet of sheets) {
      let rules;
      try { rules = sheet.cssRules; } catch (_) {
        if (sheet.href) inaccessible.push(sheet.href);
        continue;
      }
      if (!rules) continue;
      const sheetBase = sheet.href || document.baseURI;
      for (const rule of Array.from(rules)) {
        if (rule.type !== 5) continue;
        const familyRaw = (rule.style.getPropertyValue('font-family') || '').trim();
        const srcRaw = rule.style.getPropertyValue('src') || '';
        if (!familyRaw || !srcRaw) continue;
        const family = familyRaw.replace(/^["']|["']$/g, '');
        const parsed = parseSrcList(srcRaw, sheetBase);
        if (parsed && !map[family]) map[family] = parsed;
      }
    }
    // Fallback for cross-origin sheets that denied cssRules access. Most
    // CDNs (unpkg, cdnjs, jsdelivr, fonts.googleapis) send an unrestricted
    // `Access-Control-Allow-Origin: *` on the CSS response, so a plain
    // `fetch(href)` from the page's origin succeeds even when the linked
    // stylesheet itself was loaded without `crossorigin`. Failures (rare,
    // typically same-origin policy on a self-hosted icon font without
    // ACAO) silently drop the family from the map; the snapshot will keep
    // emitting a font-named span for those glyphs.
    for (const href of inaccessible) {
      try {
        const res = await fetch(href, { mode: 'cors', credentials: 'omit' });
        if (!res.ok) continue;
        const css = await res.text();
        const entries = parseFontFaceFromText(css, href);
        for (const e of entries) {
          if (!map[e.family]) map[e.family] = { url: e.url, format: e.format };
        }
      } catch (_) { /* CORS or network — skip */ }
    }
    return map;
  }
  function pseudoChar(el, pseudo) {
    const cs = getComputedStyle(el, pseudo);
    const raw = (cs.getPropertyValue('content') || '').trim();
    if (!raw || raw === 'none' || raw === 'normal') return '';
    let out = '';
    const re = /"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'/g;
    let m;
    while ((m = re.exec(raw)) !== null) {
      out += m[1] !== undefined ? m[1] : m[2];
    }
    return out;
  }
  const fontFaceMap = await collectFontFaceMap();
  const targets = [];
  let counter = 0;
  // PUA-only filter, applied per-codepoint (not per-string) so a single
  // surrogate-pair character in PUA-A (U+F0000+) is also accepted via
  // `codePointAt(0)`. Single-char check ensures we don't accidentally
  // capture multi-glyph ligature names ("home", "menu") that some icon
  // fonts use; ligature handling would require renderText measurements
  // we don't have here.
  function isPuaCodepoint(cp) {
    return cp >= 0xE000 && cp <= 0xF8FF;
  }
  const all = document.body ? document.body.querySelectorAll('*') : [];
  for (const el of all) {
    if (!el || el.children.length > 0) continue;
    let hasText = false;
    for (const n of el.childNodes) {
      if (n.nodeType === 3 && n.nodeValue && n.nodeValue.trim()) { hasText = true; break; }
    }
    if (hasText) continue;
    for (const pseudo of ['::before', '::after']) {
      const ch = pseudoChar(el, pseudo);
      if (!ch) continue;
      // String length ≠ codepoint count for surrogate pairs, but icon-font
      // glyphs all live in BMP PUA today (U+E000..U+F8FF) so length === 1
      // is the right gate. Reject longer strings (ligature names like
      // "home", multi-char prefixes, …) so we never mis-route real text.
      if (ch.length !== 1) continue;
      const cp = ch.codePointAt(0);
      if (!isPuaCodepoint(cp)) continue;
      const cs = getComputedStyle(el, pseudo);
      const fontFamilyRaw = (cs.getPropertyValue('font-family') || '').trim();
      const families = fontFamilyRaw
        .split(',')
        .map(function (s) { return s.trim().replace(/^["']|["']$/g, ''); });
      let entry = null;
      let matchedFamily = '';
      for (const fam of families) {
        if (fontFaceMap[fam]) { entry = fontFaceMap[fam]; matchedFamily = fam; break; }
      }
      if (!entry) continue;
      const id = '__pagx_icon_' + (counter++) + '__';
      el.setAttribute('data-icon-target-id', id);
      targets.push({
        id: id,
        fontFamily: matchedFamily,
        codepoint: cp,
        fontUrl: entry.url,
        format: entry.format,
        pseudo: pseudo,
      });
      // First non-empty pseudo wins; the second pseudo on the same element
      // would need a second tag attribute — no real-world icon-font CSS
      // uses both pseudos at once, so skip for simplicity.
      break;
    }
  }
  return targets;
}

// Re-find each tagged host and attach the resolved SVG markup as
// `data-snapshot-icon-svg`. The snapshot walker reads that attribute and
// routes via `renderInlineIconSvg`. Hosts that didn't get a result (font
// fetch failed, glyph missing, …) keep their original `::before` pseudo,
// so the snapshot falls back to the legacy font-named span path.
function browserApplyIconFontSvgs(results) {
  const arr = results || [];
  for (let i = 0; i < arr.length; i++) {
    const r = arr[i];
    if (!r || !r.id || !r.svg) continue;
    const el = document.querySelector('[data-icon-target-id="' + r.id + '"]');
    if (!el) continue;
    el.setAttribute('data-snapshot-icon-svg', r.svg);
    el.removeAttribute('data-icon-target-id');
  }
  // Strip any leftover tags whose host failed to resolve, so the snapshot
  // output isn't polluted with internal tracking attributes.
  const stragglers = document.querySelectorAll('[data-icon-target-id]');
  for (let i = 0; i < stragglers.length; i++) {
    stragglers[i].removeAttribute('data-icon-target-id');
  }
}

// ===== Node-side helpers =====

// Fetch `url` into a Buffer. Supports `http(s)://`, `data:`, and `file://`
// schemes; everything else is rejected so a relative-path src that slipped
// through URL resolution doesn't silently become a no-op.
async function fetchFontBytes(url) {
  if (url.startsWith('data:')) {
    const comma = url.indexOf(',');
    if (comma < 0) throw new Error('malformed data: URL');
    const meta = url.slice(5, comma);
    const payload = url.slice(comma + 1);
    if (/;base64/i.test(meta)) {
      return Buffer.from(payload, 'base64');
    }
    return Buffer.from(decodeURIComponent(payload), 'binary');
  }
  if (url.startsWith('file://')) {
    return fsp.readFile(new URL(url));
  }
  if (!/^https?:/i.test(url)) {
    throw new Error('unsupported scheme: ' + url);
  }
  const res = await fetch(url);
  if (!res.ok) throw new Error('HTTP ' + res.status);
  return Buffer.from(await res.arrayBuffer());
}

// Parse a font byte buffer (TTF / OTF / WOFF / WOFF2) into an opentype.js
// `Font`. WOFF2 is detected by its `wOF2` magic number (0x77 0x4F 0x46 0x32)
// and decompressed via the `wawoff2` WASM port of Google's reference
// decoder; everything else is fed straight to `opentype.parse`, which
// already handles WOFF natively.
async function parseFontBuffer(buffer) {
  const { opentype, wawoff2 } = loadDeps();
  let bytes = buffer;
  if (bytes.length >= 4 && bytes[0] === 0x77 && bytes[1] === 0x4F && bytes[2] === 0x46 && bytes[3] === 0x32) {
    const decoded = await wawoff2.decompress(bytes);
    bytes = Buffer.isBuffer(decoded) ? decoded : Buffer.from(decoded);
  }
  return opentype.parse(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
}

// Round a float to `digits` decimal places, matching the precision used
// throughout browser-snapshot.js for path-data emission. `0` round-trips
// to `0` (avoids `-0`), and non-finite values fall back to `0`.
function roundTo(n, digits) {
  if (!Number.isFinite(n)) return 0;
  const m = Math.pow(10, digits);
  const r = Math.round(n * m) / m;
  return r === 0 ? 0 : r;
}

// Build an inline SVG fragment that paints the glyph at `codepoint`.
//
// Layout: the viewBox is the em-square (`0 0 unitsPerEm unitsPerEm`); the
// path is positioned via opentype.js's `glyph.getPath(0, ascender,
// unitsPerEm)` so its baseline sits at `y = ascender` within that box,
// matching SVG's Y-down convention. The wrapper that hosts this SVG
// (renderInlineIconSvg in browser-snapshot.js) injects explicit width /
// height attributes derived from the host's measured pixel rect, so the
// em-square is uniformly scaled to the icon's on-screen size.
//
// `fill="currentColor"` lets the wrapper's inherited `color` flow through
// without a per-element resolution pass — icon webfonts always render in
// the host's text color, so this is a perfect 1:1 mapping.
//
// Returns null when the codepoint is unmapped (.notdef, glyph index 0).
// The caller falls back to the legacy font-named span in that case.
function glyphToSvg(font, codepoint) {
  const ch = String.fromCodePoint(codepoint);
  const glyph = font.charToGlyph(ch);
  if (!glyph) return null;
  // opentype.js maps unmapped codepoints to .notdef (glyph index 0). The
  // .notdef shape (typically an X or empty box) is never the right
  // fallback for an icon — letting the snapshot keep the original font
  // span produces a visible question-mark glyph instead, which at least
  // tells the viewer "this icon is missing" rather than silently painting
  // an X.
  if (glyph.index === 0) return null;
  const unitsPerEm = font.unitsPerEm || 1000;
  const baseline = (typeof font.ascender === 'number' && font.ascender !== 0)
    ? font.ascender : unitsPerEm;
  let path;
  try {
    path = glyph.getPath(0, baseline, unitsPerEm);
  } catch (_) {
    return null;
  }
  // opentype.js's `path.toPathData` accepts a precision; 2 decimals
  // matches the path emission in renderBorderTriangle and keeps the
  // output stable across repeated runs (no float jitter in path data).
  const data = (path && typeof path.toPathData === 'function') ? path.toPathData(2) : '';
  if (!data || !data.trim()) return null;
  // viewBox = em-square. We do not embed `width`/`height` here — the
  // snapshot wrapper rewrites the opening tag to pin pixel dimensions
  // from the icon's measured rect, mirroring how `freezeSvg` handles
  // real inline SVGs.
  const ue = roundTo(unitsPerEm, 0);
  return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ' + ue + ' ' + ue + '">'
    + '<path d="' + data + '" fill="currentColor"/></svg>';
}

// Take the `targets` array reported by `browserCollectIconFontTargets` and
// produce a parallel `[{ id, svg }]` list for the apply phase. Each unique
// font URL is fetched + parsed once; per-glyph extraction reuses the cached
// font. Failures (bad URL, unsupported format, missing glyph) are logged
// via `logger` and degrade gracefully — the host stays untagged and the
// snapshot falls back to the font-named span.
async function resolveIconFontSvgs(targets, logger) {
  if (!targets || targets.length === 0) return [];
  const log = typeof logger === 'function' ? logger : function () {};
  const fontByUrl = new Map();
  const failedUrls = new Set();
  const results = [];
  for (const t of targets) {
    if (!t || !t.fontUrl) continue;
    if (failedUrls.has(t.fontUrl)) continue;
    if (!fontByUrl.has(t.fontUrl)) {
      try {
        const bytes = await fetchFontBytes(t.fontUrl);
        const font = await parseFontBuffer(bytes);
        fontByUrl.set(t.fontUrl, font);
      } catch (err) {
        failedUrls.add(t.fontUrl);
        log('failed to load icon font ' + t.fontUrl + ': ' + (err && err.message ? err.message : err));
        continue;
      }
    }
    const font = fontByUrl.get(t.fontUrl);
    if (!font) continue;
    let svg = null;
    try {
      svg = glyphToSvg(font, t.codepoint);
    } catch (err) {
      log('failed to extract glyph U+' + t.codepoint.toString(16).toUpperCase() + ' from ' + t.fontUrl
        + ': ' + (err && err.message ? err.message : err));
      continue;
    }
    if (!svg) continue;
    results.push({ id: t.id, svg: svg });
  }
  return results;
}

// Convenience: run the full collect → resolve → apply pipeline against a
// puppeteer page. Returns `{ inlined, total }` so the driver can log how
// many icons were converted ("inlined N / M icon-font glyphs"). A zero
// return is silent — pages without icon-font usage round-trip identically
// to the legacy snapshot.
async function inlineIconFontsOnPage(page, opts) {
  const options = opts || {};
  const logger = typeof options.logger === 'function' ? options.logger : function () {};
  // Ship `browserCollectIconFontTargets` to the page. The fn is fully
  // self-contained (collectFontFaceMap and pseudoChar are inlined), so a
  // straight `page.evaluate(fn)` is enough.
  const targets = await page.evaluate(browserCollectIconFontTargets);
  if (!targets || targets.length === 0) return { inlined: 0, total: 0 };
  const results = await resolveIconFontSvgs(targets, logger);
  await page.evaluate(browserApplyIconFontSvgs, results);
  return { inlined: results.length, total: targets.length };
}

module.exports = {
  // Browser-side hooks (also re-exported in the standalone bundle so
  // callers running outside of node can plug in their own resolver).
  browserCollectFontFaceMap,
  browserCollectIconFontTargets,
  browserApplyIconFontSvgs,
  // Node-side helpers.
  fetchFontBytes,
  parseFontBuffer,
  glyphToSvg,
  resolveIconFontSvgs,
  inlineIconFontsOnPage,
};
