// Copyright (C) 2026 Tencent. All rights reserved.
// @ts-nocheck

// Write `value` as a double-quoted CSS string that also survives the importer's
// XML parse of the `<style>` text. `<`, `>` and `&` would be read as markup or
// entities there, so they are written as CSS hex escapes like the quote,
// backslash and control characters; the browser and the importer's CSS reader
// both decode them back to the original character.
function cssQuotedString(value) {
  let out = '"';
  for (const ch of String(value)) {
    if (ch === '"' || ch === '\\' || ch === '<' || ch === '>' || ch === '&' || ch < ' ') {
      out += '\\' + ch.codePointAt(0).toString(16) + ' ';
    } else {
      out += ch;
    }
  }
  return out + '"';
}

// Collect the page's `@font-face` rules the subset can carry over, as
// `{ family, src }` pairs in stylesheet order. `src` is either the absolute path
// of a font file on local disk or the rule's original `data:` URI. The importer
// registers either form under `family` — a file becomes a PAGX `<Font file>`
// resource, a `data:` URI is decoded into the document's font config — so the
// CSS name the subset carries always resolves, however it relates to the family
// name stored inside the file. Remote sources are left out: the page fetch that
// produced them no longer exists once the subset is handed to the importer.
// Each rule contributes its first `url()` source, ignoring `local()` names and
// `format()` hints. Rules nested in grouping rules (`@media`, `@supports`) and
// imported sheets are visited too. A sheet whose rules are inaccessible
// (cross-origin) is skipped, which loses nothing: its fonts cannot be local.
function collectCarryOverFontFaces(sheets, baseURI) {
  const faces = [];
  const seen = new Set();
  const URL_RE = /url\(\s*(?:"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'|([^)\s]*))\s*\)/gi;
  const unescape = (text) => text.replace(/\\(.)/g, '$1');
  // A `data:` URI is already self-contained, so it travels verbatim; anything else has to
  // resolve to a file on local disk.
  const toCarryOverSource = (raw, base) => {
    if (/^data:/i.test(raw)) {
      return raw;
    }
    try {
      const u = new URL(raw, base);
      if (u.protocol !== 'file:') return '';
      let p = decodeURIComponent(u.pathname);
      if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
      return p;
    } catch (_) {
      return '';
    }
  };
  const visitRules = (rules, base, depth) => {
    if (!rules || depth > 8) return;
    for (const rule of Array.from(rules)) {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const r = rule as any;
      if (r.styleSheet) {
        visitSheet(r.styleSheet, depth + 1);
        continue;
      }
      if (r.type !== 5) {
        if (r.cssRules) visitRules(r.cssRules, base, depth + 1);
        continue;
      }
      const family = unescape((r.style.getPropertyValue('font-family') || '').trim()
        .replace(/^(["'])([\s\S]*)\1$/, '$2'));
      const src = r.style.getPropertyValue('src') || '';
      if (!family || !src) continue;
      URL_RE.lastIndex = 0;
      let m;
      while ((m = URL_RE.exec(src)) !== null) {
        const raw = unescape(m[1] !== undefined ? m[1] : (m[2] !== undefined ? m[2] : m[3]));
        const carried = raw ? toCarryOverSource(raw, base) : '';
        if (!carried) continue;
        const key = family + '\n' + carried;
        if (!seen.has(key)) {
          seen.add(key);
          faces.push({ family, src: carried });
        }
        break;
      }
    }
  };
  const visitSheet = (sheet, depth) => {
    let rules;
    try { rules = sheet.cssRules; } catch (_) { return; }
    visitRules(rules, sheet.href || baseURI, depth);
  };
  for (const sheet of Array.from(sheets || [])) {
    visitSheet(sheet, 0);
  }
  return faces;
}

export { collectCarryOverFontFaces, cssQuotedString };
