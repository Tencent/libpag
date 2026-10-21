// Copyright (C) 2026 Tencent. All rights reserved.
// @ts-nocheck

// ===== CSS counter resolution for pseudo `content` =====
//
// `getComputedStyle(el, pseudo).content` reports `counter(name[, style])`
// verbatim — the browser resolves counter values at layout time and never
// writes them back into the style. The walker and the decorative-pseudo
// materialiser still need the resolved glyphs, otherwise a plain
// `.list div::before { content: counter(s, decimal-leading-zero) }` numbering
// renders as a text-less (stand-in) box and the digits are silently dropped.
//
// The value is evaluated the way the common authoring patterns lay it out:
// the nearest ancestor-or-self `counter-reset` for the name opens the counter
// scope with its base value, and every element in tree order between that
// scope root and the host (inclusive) adds its `counter-increment` delta; a
// deeper reset on the host's own ancestor chain restarts the count. The
// remaining CSS counter subtleties (sibling-scoped restarts, `counters()`)
// keep the historical "contributes nothing" contract.
function parseCssCounterList(value, defaultDelta) {
  const out = [];
  const re = /([A-Za-z_][\w-]*)\s*(-?\d+)?/g;
  let m;
  while ((m = re.exec(value || '')) !== null) {
    if (m[1].toLowerCase() === 'none') continue;
    out.push([m[1], m[2] !== undefined ? parseInt(m[2], 10) : defaultDelta]);
  }
  return out;
}

function cssRomanCounter(value) {
  const ROMAN = [
    [1000, 'm'], [900, 'cm'], [500, 'd'], [400, 'cd'], [100, 'c'], [90, 'xc'],
    [50, 'l'], [40, 'xl'], [10, 'x'], [9, 'ix'], [5, 'v'], [4, 'iv'], [1, 'i'],
  ];
  let s = '';
  let n = value;
  for (let i = 0; i < ROMAN.length; i++) {
    while (n >= ROMAN[i][0]) {
      s += ROMAN[i][1];
      n -= ROMAN[i][0];
    }
  }
  return s;
}

function cssAlphaCounter(value) {
  let s = '';
  let n = value;
  while (n > 0) {
    const m = (n - 1) % 26;
    s = String.fromCharCode(97 + m) + s;
    n = Math.floor((n - 1) / 26);
  }
  return s;
}

function formatCssCounterValue(value, style) {
  const n = Math.trunc(value);
  switch ((style || 'decimal').trim().toLowerCase()) {
    case 'decimal-leading-zero':
      return (n < 0 ? '-' : '') + String(Math.abs(n)).padStart(2, '0');
    case 'lower-alpha': case 'lower-latin': return n > 0 ? cssAlphaCounter(n) : String(n);
    case 'upper-alpha': case 'upper-latin': return n > 0 ? cssAlphaCounter(n).toUpperCase() : String(n);
    case 'lower-roman': return n > 0 && n < 4000 ? cssRomanCounter(n) : String(n);
    case 'upper-roman': return n > 0 && n < 4000 ? cssRomanCounter(n).toUpperCase() : String(n);
    case 'none': return '';
    case 'decimal': default: return String(n);
  }
}

function resolveCssCounterValue(el, name) {
  let scope = null;
  let value = 0;
  for (let node = el; node; node = node.parentElement) {
    const resets = parseCssCounterList(getComputedStyle(node).getPropertyValue('counter-reset'), 0);
    let hit = null;
    for (let i = 0; i < resets.length; i++) {
      if (resets[i][0] === name) hit = resets[i][1];
    }
    if (hit !== null) {
      scope = node;
      value = hit;
      break;
    }
  }
  const root = scope || el.ownerDocument.documentElement;
  if (!root) return null;
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT);
  let node = root;
  while (node) {
    if (node !== scope) {
      // A reset restarts this counter only when it scopes the host (its own
      // subtree contains the host); a reset on a sibling subtree opens a
      // different counter instance and must not leak in.
      const resets = parseCssCounterList(getComputedStyle(node).getPropertyValue('counter-reset'), 0);
      for (let i = 0; i < resets.length; i++) {
        if (resets[i][0] === name && (node === el || node.contains(el))) value = resets[i][1];
      }
    }
    const incs = parseCssCounterList(getComputedStyle(node).getPropertyValue('counter-increment'), 1);
    for (let i = 0; i < incs.length; i++) {
      if (incs[i][0] === name) value += incs[i][1];
    }
    if (node === el) break;
    node = walker.nextNode();
  }
  return value;
}

// Resolve a pseudo `content` value into the text a browser would paint.
// Quoted strings concatenate in place; `counter(...)` receives its evaluated
// value; every other form (`attr()` / `url()` / `counters()` / the quote
// keywords) still contributes nothing, preserving the historical contract of
// the quoted-run scan this replaces. Returns null when the pseudo is not
// generated (`content: none` / `normal`).
//
// `pseudoComputed` (the pseudo's own computed style, when the caller has it)
// contributes the pseudo's own counter operations — the classic
// `::before { counter-increment: s; content: counter(s) }` self-increment
// pattern, which never shows up on the host's computed style.
function resolvePseudoContentText(el, raw, pseudoComputed) {
  const trimmed = (raw || '').trim();
  if (!trimmed) return '';
  if (trimmed === 'none' || trimmed === 'normal') return null;
  if (trimmed.indexOf('counter(') === -1) {
    // Identical to the legacy scan: concatenate every quoted run.
    let out = '';
    const re = /"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'/g;
    let m;
    while ((m = re.exec(trimmed)) !== null) {
      out += (m[1] !== undefined ? m[1] : m[2]);
    }
    return out;
  }
  // Positional scan so `"a" counter(x) "b"` keeps its authored order.
  let out = '';
  let i = 0;
  while (i < trimmed.length) {
    const ch = trimmed[i];
    if (ch === '"' || ch === "'") {
      let j = i + 1;
      let s = '';
      while (j < trimmed.length && trimmed[j] !== ch) {
        if (trimmed[j] === '\\' && j + 1 < trimmed.length) {
          s += trimmed[j] + trimmed[j + 1];
          j += 2;
        } else {
          s += trimmed[j];
          j += 1;
        }
      }
      out += s;
      i = j + 1;
      continue;
    }
    const fn = /^(counter|counters|attr|url)\(/i.exec(trimmed.slice(i, i + 9));
    if (fn) {
      const open = i + fn[0].length;
      const close = trimmed.indexOf(')', open);
      const inner = close === -1 ? trimmed.slice(open) : trimmed.slice(open, close);
      if (fn[1].toLowerCase() === 'counter') {
        const parts = inner.split(',');
        const name = (parts[0] || '').trim();
        const style = (parts[1] || 'decimal').trim();
        if (name) {
          const value = resolveCssCounterValue(el, name);
          if (value !== null) out += formatCssCounterValue(applyPseudoCounterOps(value, name, pseudoComputed), style);
        }
      }
      i = close === -1 ? trimmed.length : close + 1;
      continue;
    }
    i += 1;
  }
  return out;
}

// Fold the pseudo's own `counter-reset` / `counter-increment` into the host
// value resolved by `resolveCssCounterValue`.
function applyPseudoCounterOps(value, name, pseudoComputed) {
  if (!pseudoComputed) return value;
  let v = value;
  const resets = parseCssCounterList(pseudoComputed.getPropertyValue('counter-reset'), 0);
  for (let i = 0; i < resets.length; i++) {
    if (resets[i][0] === name) v = resets[i][1];
  }
  const incs = parseCssCounterList(pseudoComputed.getPropertyValue('counter-increment'), 1);
  for (let i = 0; i < incs.length; i++) {
    if (incs[i][0] === name) v += incs[i][1];
  }
  return v;
}

export { parseCssCounterList, cssRomanCounter, cssAlphaCounter, formatCssCounterValue, resolveCssCounterValue, resolvePseudoContentText, applyPseudoCounterOps };
export const COUNTER_HELPERS_SRC = [parseCssCounterList, cssRomanCounter, cssAlphaCounter, formatCssCounterValue, resolveCssCounterValue, resolvePseudoContentText, applyPseudoCounterOps].map(fn => fn.toString()).join('\n\n');
