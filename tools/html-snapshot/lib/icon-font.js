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
const { DROP_TAG_NAMES } = require('./dom-tags');
const { errMessage } = require('./cli');

let opentype = null;
let wawoff2 = null;
function loadDeps() {
  if (!opentype) opentype = require('opentype.js');
  if (!wawoff2) wawoff2 = require('wawoff2');
  return { opentype, wawoff2 };
}

// ===== Browser-side helpers =====
//
// Each helper below is a top-level function declaration — never nested inside
// another function — so its `Function.prototype.toString()` source can be
// concatenated into a single IIFE string and shipped to the page via
// `page.evaluate`. The same source is also injected into the standalone
// browser bundle (build-browser-bundle.js) so callers running outside of
// node have access to the same helpers under one shared closure.
//
// Layout mirrors lib/browser-snapshot.js:
//   ICON_FONT_HELPER_FNS  — node-side array of helper references.
//   ICON_FONT_HELPERS_SRC — concatenated source string. Empty until the
//                           array is finalised at module-load time.
// The two `browserCollect…` entry points reference these helpers by name;
// they only resolve when run inside a closure that has already declared the
// helpers (i.e. the IIFE strings below, or the bundle's factory closure).

// Format-rank lookup for picking the most-decode-friendly source. WOFF2 is
// preferred (smallest, modern), then WOFF (zlib-only), then raw SFNT
// containers. `truetype`/`opentype` are aliases for ttf/otf.
function formatRank(f) {
  if (f === 'woff2') return 1;
  if (f === 'woff') return 2;
  if (f === 'ttf' || f === 'truetype') return 3;
  if (f === 'otf' || f === 'opentype') return 4;
  return 5;
}

// Parse a `src:` declaration into the best-ranked `{url, format}` pair.
// `font-family` values are unquoted by the caller; `src` URLs are resolved
// against the stylesheet's own `href` so relative paths in CDN-hosted
// stylesheets produce absolute, fetchable URLs.
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
// fallback when `cssRules` is denied by CORS — Chromium still loaded and
// applied the stylesheet (so the page renders), but JS can't introspect it.
// Regex parsing is intentionally permissive: the only tokens we care about
// are `font-family` and `src`, both of which use the same syntax that
// `cssRules` would yield. Comments are stripped first so a `/* src: url(...)
// */` decoy doesn't mislead the matcher.
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

// Walk `document.styleSheets` for `@font-face` rules and return a
// JSON-safe `{ family: { url, format } }` map.
//
// `cssRules` access throws `SecurityError` on cross-origin stylesheets that
// did not opt in via `crossorigin="anonymous"` — which is most stylesheets
// loaded via plain `<link href="https://cdn.example/...">`. For each such
// sheet we issue a regular `fetch(href)` and parse the returned CSS text
// manually; nearly every font CDN (unpkg, cdnjs, jsdelivr, googlefonts)
// sends `Access-Control-Allow-Origin: *` on the stylesheet response, so the
// fallback succeeds without page-side configuration. Sheets that fail both
// paths are silently skipped (the snapshot will keep emitting a font-named
// span for those glyphs).
//
// First-write wins: a duplicate `@font-face` (e.g. weight variants sharing
// the same family) keeps the first source — good enough for icon fonts
// (single regular weight) and avoids round-tripping a heavier italic/bold
// variant.
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
      // CSSFontFaceRule: rule.type === 5. Newer browsers expose
      // CSSFontFaceRule as a class; the type-number check works on both.
      if (rule.type !== 5) continue;
      const familyRaw = (rule.style.getPropertyValue('font-family') || '').trim();
      const srcRaw = rule.style.getPropertyValue('src') || '';
      if (!familyRaw || !srcRaw) continue;
      const family = familyRaw.replace(/^["']|["']$/g, '');
      const parsed = parseSrcList(srcRaw, sheetBase);
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

// PUA-only filter, applied per-codepoint (not per-string) so a single
// surrogate-pair character in PUA-A (U+F0000+) is also accepted via
// `codePointAt(0)`. Single-char check (in the caller) ensures we don't
// accidentally capture multi-glyph ligature names ("home", "menu") that
// some icon fonts use; ligature handling would require renderText
// measurements we don't have here.
function isPuaCodepoint(cp) {
  return cp >= 0xE000 && cp <= 0xF8FF;
}

// Tags whose subtree never paints visible icon glyphs. Skip them up front
// so a page with hundreds of inline `<style>` / `<script>` blocks doesn't
// pay for a per-element `getComputedStyle(el, '::before')` round-trip.
//
// Backed by the shared `DROP_TAG_NAMES` list in lib/dom-tags.js so this
// pass and the main snapshot walker stay in lockstep — the function body
// is constructed at module load and serialised into the browser payload
// through `ICON_FONT_DROP_TAGS_SRC` below.
function isIconScanSkippedTag(tag) {
  return tag ? DROP_TAGS_UPPER.has(tag) : false;
}

// ===== Browser-side entry points =====
//
// `browserCollect…` functions are not self-contained: they reference the
// helpers above by name and only resolve when executed inside a closure
// that has those helpers in scope. The IIFE strings below build that
// closure for `page.evaluate`; the standalone browser bundle does the same
// inside its UMD/ESM factory.

// Walk styleSheets and return the JSON-safe `@font-face` family→{url,format}
// map. Async because the cross-origin CSS fallback uses `fetch`.
async function browserCollectFontFaceMap() {
  return await collectFontFaceMap();
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
async function browserCollectIconFontTargets() {
  const fontFaceMap = await collectFontFaceMap();
  const targets = [];
  let counter = 0;
  const all = document.body ? document.body.querySelectorAll('*') : [];
  // Hoist the quoted-segment regex once and inline the `content` decode
  // alongside the `font-family` lookup so each candidate pays a single
  // `getComputedStyle(el, pseudo)` call for both reads. Splitting the two
  // (an earlier helper extracted just the character) doubled the
  // pseudo-element layout queries on dense pages.
  const QUOTED_RE = /"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'/g;
  for (const el of all) {
    if (!el || el.children.length > 0) continue;
    if (isIconScanSkippedTag(el.tagName)) continue;
    let hasText = false;
    for (const n of el.childNodes) {
      if (n.nodeType === 3 && n.nodeValue && n.nodeValue.trim()) { hasText = true; break; }
    }
    if (hasText) continue;
    for (const pseudo of ['::before', '::after']) {
      const cs = getComputedStyle(el, pseudo);
      const raw = (cs.getPropertyValue('content') || '').trim();
      if (!raw || raw === 'none' || raw === 'normal') continue;
      let ch = '';
      QUOTED_RE.lastIndex = 0;
      let m;
      while ((m = QUOTED_RE.exec(raw)) !== null) {
        ch += m[1] !== undefined ? m[1] : m[2];
      }
      if (!ch) continue;
      // String length ≠ codepoint count for surrogate pairs, but icon-font
      // glyphs all live in BMP PUA today (U+E000..U+F8FF) so length === 1
      // is the right gate. Reject longer strings (ligature names like
      // "home", multi-char prefixes, …) so we never mis-route real text.
      if (ch.length !== 1) continue;
      const cp = ch.codePointAt(0);
      if (!isPuaCodepoint(cp)) continue;
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

// Re-find each tagged host, store the resolved SVG markup in a window-level
// dictionary keyed by the host's stable `data-icon-target-id`, then leave
// only the id reference on the host as `data-snapshot-icon-svg-id`. The
// snapshot walker reads that id and looks the SVG up in
// `window.__pagxIconSvgs` instead of round-tripping arbitrary XML through
// an HTML attribute value — earlier flat-attribute storage worked for
// today's purely numeric path data but would have silently corrupted any
// future glyph payload containing nested attribute quoting (embedded
// `<text>`, metadata, etc.). The dictionary persists into the
// `takeSnapshot` evaluate call because both run in the same page context
// without intervening navigation.
//
// Self-contained (no helper dependencies), so it can be passed straight to
// `page.evaluate(fn, args)` without an IIFE wrapper.
function browserApplyIconFontSvgs(results) {
  const arr = results || [];
  const dict = (window.__pagxIconSvgs = window.__pagxIconSvgs || Object.create(null));
  for (let i = 0; i < arr.length; i++) {
    const r = arr[i];
    if (!r || !r.id || !r.svg) continue;
    const el = document.querySelector('[data-icon-target-id="' + r.id + '"]');
    if (!el) continue;
    dict[r.id] = r.svg;
    el.setAttribute('data-snapshot-icon-svg-id', r.id);
    el.removeAttribute('data-icon-target-id');
  }
  // Strip any leftover tags whose host failed to resolve, so the snapshot
  // output isn't polluted with internal tracking attributes.
  const stragglers = document.querySelectorAll('[data-icon-target-id]');
  for (let i = 0; i < stragglers.length; i++) {
    stragglers[i].removeAttribute('data-icon-target-id');
  }
}

// ===== Browser-side payload assembly =====
//
// Mirrors the layout in lib/browser-snapshot.js: a single array of helper
// references → concatenated source string → IIFE wrappers around the two
// async entry points. The standalone browser bundle re-uses
// `ICON_FONT_HELPERS_SRC` to expose the helpers under its factory closure.

const ICON_FONT_HELPER_FNS = [
  formatRank,
  parseSrcList,
  parseFontFaceFromText,
  collectFontFaceMap,
  isPuaCodepoint,
  isIconScanSkippedTag,
];
// Prepend the shared drop-tag set as raw JS so the serialised
// `isIconScanSkippedTag` body resolves `DROP_TAGS_UPPER` from the same IIFE
// scope. Keeping the constant here (rather than inside the function via a
// closure) ensures both browser entry points reuse one Set instead of
// reconstructing it on every call.
const ICON_FONT_DROP_TAGS_SRC =
  'const DROP_TAGS_UPPER = new Set(' +
  JSON.stringify(DROP_TAG_NAMES.map(function (s) { return s.toUpperCase(); })) +
  ');';
const ICON_FONT_HELPERS_SRC =
  ICON_FONT_DROP_TAGS_SRC + '\n\n' +
  ICON_FONT_HELPER_FNS.map((fn) => fn.toString()).join('\n\n');

// `page.evaluate(string)` evaluates the string and awaits its result, so an
// async-IIFE is the right shape for both entry points: the helpers are
// declared first (function declarations hoist), then the entry function is
// declared and immediately invoked.
const COLLECT_FONT_FACE_MAP_PAYLOAD =
  '(async () => {\n' + ICON_FONT_HELPERS_SRC + '\n' + browserCollectFontFaceMap.toString() +
  '\nreturn await browserCollectFontFaceMap();\n})()';
const COLLECT_ICON_FONT_TARGETS_PAYLOAD =
  '(async () => {\n' + ICON_FONT_HELPERS_SRC + '\n' + browserCollectIconFontTargets.toString() +
  '\nreturn await browserCollectIconFontTargets();\n})()';

// ===== Node-side helpers =====

// Hard cap on a single icon-font network fetch. Without this, a slow or
// hung CDN blocks the entire snapshot pipeline indefinitely (the caller
// already wraps fetchFontBytes in try/catch, so timing out just routes
// the failed font to the legacy span path — the rest of the page still
// snapshots cleanly).
const ICON_FONT_FETCH_TIMEOUT_MS = 10_000;

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
  const res = await fetch(url, { signal: AbortSignal.timeout(ICON_FONT_FETCH_TIMEOUT_MS) });
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
  const log = logger;
  const fontByUrl = new Map();
  const failedUrls = new Set();

  // Pre-fetch each unique font URL in parallel. The previous implementation
  // walked targets sequentially and downloaded on-demand, which serialised
  // every CDN round-trip — a page using Phosphor + FontAwesome + Material
  // Icons paid three RTTs back-to-back. Pulling the fetch+parse out into a
  // Promise.all collapses those into a single overlapping window. Per-glyph
  // SVG extraction stays sequential (CPU-bound, not benefitted by parallel
  // dispatch) and runs against the now-warm cache.
  const uniqueUrls = [];
  const seen = new Set();
  for (const t of targets) {
    if (!t || !t.fontUrl || seen.has(t.fontUrl)) continue;
    seen.add(t.fontUrl);
    uniqueUrls.push(t.fontUrl);
  }
  await Promise.all(uniqueUrls.map(async (url) => {
    try {
      const bytes = await fetchFontBytes(url);
      const font = await parseFontBuffer(bytes);
      fontByUrl.set(url, font);
    } catch (err) {
      failedUrls.add(url);
      log('failed to load icon font ' + url + ': ' + errMessage(err));
    }
  }));

  const results = [];
  for (const t of targets) {
    if (!t || !t.fontUrl) continue;
    if (failedUrls.has(t.fontUrl)) continue;
    const font = fontByUrl.get(t.fontUrl);
    if (!font) continue;
    let svg = null;
    try {
      svg = glyphToSvg(font, t.codepoint);
    } catch (err) {
      log('failed to extract glyph U+' + t.codepoint.toString(16).toUpperCase() + ' from ' + t.fontUrl
        + ': ' + errMessage(err));
      continue;
    }
    if (!svg) continue;
    results.push({ id: t.id, svg: svg });
  }
  return results;
}

// Top-level pipeline: discover icon-font targets in the page, fetch their
// font URLs, render each target glyph to an inline `<svg>` payload and
// stash that payload on the host element via `data-snapshot-icon-svg`.
// The downstream snapshot walker (`render` in browser-snapshot.js) routes
// any element carrying that attribute to `renderInlineIconSvg`, which
// emits the SVG instead of a font-named span.
//
// Returns `{ inlined, total }` so the caller can log how many targets
// survived. An empty `targets` (no PUA pseudo backed by a webfont) early
// return is silent — pages without icon-font usage round-trip identically
// to the legacy snapshot.
//
// Each phase has its own try/catch: the previous flat layout meant a
// cssRules SecurityError during *target collection* would skip the whole
// page even when the resolver and applicator would have worked fine on
// the targets that *did* parse. Now collection failures abort cleanly,
// resolver failures fall back to "apply nothing" (which still cleans up
// the `data-icon-target-id` markers the collector left behind), and
// applicator failures only lose the visual replacement — the page is
// still snapshotted, just with the original font-named spans.
async function inlineIconFontsOnPage(page, opts) {
  const options = opts || {};
  const logger = typeof options.logger === 'function' ? options.logger : function () {};

  let targets = null;
  try {
    targets = await page.evaluate(COLLECT_ICON_FONT_TARGETS_PAYLOAD);
  } catch (err) {
    logger('inline-icon-fonts: target collection failed: ' + errMessage(err));
    return { inlined: 0, total: 0 };
  }
  if (!targets || targets.length === 0) return { inlined: 0, total: 0 };

  let results = [];
  try {
    results = await resolveIconFontSvgs(targets, logger);
  } catch (err) {
    logger('inline-icon-fonts: SVG resolution failed: ' + errMessage(err));
    results = [];
  }

  // Always run the applicator, even when `results` is empty: it doubles as
  // the cleanup pass that strips the `data-icon-target-id` markers the
  // collector left on every candidate. Skipping it would leak those
  // attributes into the final snapshot HTML.
  try {
    await page.evaluate(browserApplyIconFontSvgs, results);
  } catch (err) {
    logger('inline-icon-fonts: SVG application failed: ' + errMessage(err));
  }
  return { inlined: results.length, total: targets.length };
}

module.exports = {
  // Browser-side hooks (also re-exported in the standalone bundle so
  // callers running outside of node can plug in their own resolver). Each
  // entry function references the helpers below by name; consumers that
  // run them in isolation must inject `ICON_FONT_HELPERS_SRC` into the
  // surrounding closure first.
  browserCollectFontFaceMap,
  browserCollectIconFontTargets,
  browserApplyIconFontSvgs,
  ICON_FONT_HELPERS_SRC,
  // Node-side helpers.
  fetchFontBytes,
  parseFontBuffer,
  glyphToSvg,
  resolveIconFontSvgs,
  inlineIconFontsOnPage,
};
