// Browser-side payload for html-snapshot.
//
// All helpers used by the snapshot pass live as Node-level functions in this
// file; they are concatenated (via `Function.prototype.toString`) into a single
// IIFE string that runs inside the headless browser. The Node side never
// invokes the helpers itself — they reference browser globals such as
// `document`, `getComputedStyle`, `Range`, `Node`, and `getBoundingClientRect`.
//
// The IIFE layout is:
//   1. Helper function declarations (hoisted to the top of the IIFE).
//   2. Constants that depend on the helpers (Sets, Maps, the STYLE_SCHEMA
//      table, the KIND_DISPATCH table). Embedded as raw JS source because
//      Sets/Maps/Objects-with-arrow-functions aren't trivially serialisable.
//   3. `return snapshotMain();` — the entry point.
//
// `inlineExternalImages` stays as a regular self-contained function passed to
// `page.evaluate`; it does not need the helpers above.
//
// TypeScript caveats for this file:
//   - Functions whose `.toString()` source is concatenated and shipped to the
//     browser MUST remain top-level `function` declarations. Do not rewrite
//     them as arrow functions or class methods — the IIFE wrapper depends on
//     hoisting + on the textual form re-parsing as a stand-alone declaration.
//   - The compile target is ES2022 (commonjs module), so async/await and
//     generators are preserved verbatim — no helper insertion (`__awaiter`,
//     `__generator`) pollutes the serialised source.
//   - `@ts-nocheck` disables type-checking for this file. Most of the body is
//     DOM-mutation code that operates on dynamic structural shapes; introducing
//     full types would require interface declarations for every internal
//     mini-record this walker passes around (style maps, layout boxes, the
//     STYLE_SCHEMA / KIND_DISPATCH tables, etc.). The existing JS is heavily
//     reviewed and tested; running it through tsc strict mode would force
//     hundreds of `any` casts without buying any callable type safety, since
//     the only consumers are puppeteer's `page.evaluate` (untyped) and the
//     thin Node-side `inlineCanvases`/`inlineExternalImages` wrappers.
//     Public re-exports below are typed via the explicit `module.exports`
//     equivalent at the bottom of the file.

// @ts-nocheck

import { DROP_TAG_NAMES_JSON } from './dom-tags';

/* eslint-disable no-undef, no-inner-declarations */

// ===== Style-value normalisers =====

// Preserve the full CSS font stack but rewrite embedded double quotes as
// single quotes so the value embeds safely inside the surrounding
// style="…" attribute. CSS parses single- and double-quoted family names
// equivalently, and the PAGX HTML importer (`ParseFontFamilyTokens`)
// splits on top-level commas and strips either quote style, so a stack
// like `"PingFang SC", -apple-system, sans-serif` round-trips: the
// importer's first concrete family lands on `Text::fontFamily` and the
// remaining concrete entries are registered as document-level fallbacks.
function normalizeFontFamily(value) {
  if (!value) return '';
  return value.replace(/"/g, "'");
}

// PAGX only models `overflow: hidden`. `clip`, `auto`, and `scroll` all map to
// hidden (closest visual approximation — no scrollbars / no painting outside).
function normalizeOverflow(value) {
  if (!value) return '';
  if (value === 'clip' || value === 'auto' || value === 'scroll') return 'hidden';
  return value;
}

// Chromium reports `border-radius` in its long-form (`T R B L`). The subset only
// accepts a single value, and authoring the long form (especially `0px 0px 0px
// 0px`) pollutes the diff. Collapse `T R B L` → single value when all four
// lengths agree, and drop entirely when they all collapse to zero. Mixed values
// are passed through verbatim and left to the importer to handle.
function normalizeBorderRadius(value) {
  if (!value) return '';
  const tokens = value.split(/\s+/).filter(Boolean);
  if (tokens.length === 0) return '';
  if (tokens.every((t) => t === tokens[0])) {
    if (tokens[0] === '0' || tokens[0] === '0px') return '';
    return tokens[0];
  }
  return value;
}

// PAGX supports `background-image` when the value is one of the gradient
// functions (linear/radial/conic) or a single `url(...)` reference. A url
// background round-trips into an `ImagePattern` fill on import (the inverse of
// the HTML exporter's emission). Chromium reports the url already resolved to
// an absolute form; rewrite a local `file://` url to a plain absolute path
// (the form the PAGX importer + tgfx load directly, matching the `<img>`
// `data-snapshot-src` path), and keep `data:` / `http(s)` urls verbatim (the
// `inlineExternalImages` pre-pass converts remote bytes to a data URI).
// Compound url + gradient stacks and the `none` keyword are dropped.
function normalizeBackgroundImage(value) {
  if (!value) return '';
  if (value === 'none') return '';
  if (/(?:^|\s|,)(?:repeating-)?(?:linear|radial|conic)-gradient\(/i.test(value)) {
    return value;
  }
  const trimmed = value.trim();
  if (/^url\(/i.test(trimmed)) {
    const m = /^url\(\s*(['"]?)([^'")]+)\1\s*\)$/i.exec(trimmed);
    if (!m) return '';
    let url = m[2];
    if (/^file:/i.test(url)) {
      try {
        const u = new URL(url);
        let p = decodeURIComponent(u.pathname);
        if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
        url = p;
      } catch (_) {
        return '';
      }
    }
    return `url("${url.replace(/"/g, "'")}")`;
  }
  if (/url\(/i.test(value)) return '';
  return '';
}

// PAGX only models `background-clip: text` — combined with a gradient
// `background-image`, the importer routes the gradient to descendant text
// fills (so the text glyphs are filled with the gradient instead of the
// element painting a rectangular gradient). Every other clip value is
// dropped: PAGX has no `padding-box` / `content-box` distinction. Chromium
// computed style already coalesces `-webkit-background-clip` into the
// unprefixed `background-clip`, so a single schema entry suffices.
function normalizeBackgroundClip(value) {
  if (!value) return '';
  return value.trim().toLowerCase() === 'text' ? 'text' : '';
}

// PAGX models `clip-path` only as a reference to a <clipPath> def (`url(#id)`),
// which the importer turns into a contour mask layer. Chromium reports the
// reference verbatim as `url("#id")`. Geometric forms (`inset()`, `circle()`,
// `ellipse()`, `polygon()`, `path()`) and `none` are out of subset and collapse
// to '' so STYLE_SCHEMA's `defaults` filter drops the property entirely.
function normalizeClipPath(value) {
  if (!value) return '';
  return /^url\(/i.test(value.trim()) ? value.trim() : '';
}

// PAGX models only `WritingMode::Horizontal` and `WritingMode::Vertical`.
// CSS `vertical-rl` / `vertical-lr` map to Vertical (kept verbatim so the
// importer can tell them apart); `sideways-rl/lr` (which would also rotate
// the glyphs themselves) are degraded to the default `horizontal-tb`
// because libpag has no upright/sideways distinction. Any other value
// (including the default `horizontal-tb`) collapses to '' so STYLE_SCHEMA's
// `defaults` filter drops the property entirely.
function normalizeWritingMode(value) {
  if (!value) return '';
  const v = String(value).trim().toLowerCase();
  if (v === 'vertical-rl' || v === 'vertical-lr') return v;
  return '';
}

// ===== Pure number / string utilities =====

// Preserve sub-pixel precision (one decimal). Authors regularly use
// half-pixel values like Tailwind's `inset-[1.5px]`; combined with an
// integer parent border the child's measured offset becomes 2.5px, which
// a plain `Math.round` would snap to 3px and introduce a visible 0.5px
// shift on every such element. One decimal place is enough to round-trip
// the common 0.5/1.5/2.5 cases while still snapping cleanly when the
// browser reports a noisy measurement like 19.99998.
function roundPx(n) {
  if (!isFinite(n)) return 0;
  return Math.round(n * 10) / 10;
}

function px(n) {
  if (!isFinite(n)) return '0px';
  return `${roundPx(n)}px`;
}

// Format a numeric `flex-grow` for the subset HTML's `flex: <N>` shorthand.
// Integers stay integer-shaped (`flex: 1`) so the imported PAGX renders the
// canonical form; non-integers keep up to three decimals to round-trip the
// rare `flex: 0.5` / `flex: 2.5` values without leaking float noise (e.g.
// `1.0000001`) into the snapshot.
function formatFlexGrow(n) {
  if (!isFinite(n) || n <= 0) return '0';
  if (Number.isInteger(n)) return String(n);
  return n.toFixed(3).replace(/\.?0+$/, '');
}

function escapeHtml(s) {
  return s
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

// Join non-empty CSS declarations with `; ` so callers don't have to guard
// against empty fragments (e.g. when the box style is empty under a flex
// parent).
function joinStyles(...parts) {
  return parts.filter(Boolean).join('; ');
}

// Append `white-space: nowrap` to a text style unless it already declares
// white-space. Used in every spot where a single-line text span sits inside a
// flex item or rides on a wrapper whose width was measured by Chromium —
// prevents PAGX's text engine from re-wrapping at a sub-pixel divergence.
// Applies to vertical writing-mode spans too: `emitVerticalTextSpans` now
// splits a run into one span PER COLUMN exactly as the horizontal path splits
// per line, so forcing `nowrap` (which routes through the importer's
// `hasNoWrap` branch to disable wordWrap) is precisely what stops PAGX from
// re-breaking a Chromium-measured single column into two when its vertical
// glyph metrics diverge.
function withNowrap(style) {
  if (!style) return 'white-space: nowrap';
  return /white-space:/.test(style) ? style : `${style}; white-space: nowrap`;
}

// Collapse 4 px sides into the shortest CSS shorthand.
function paddingShorthand(t, r, b, l) {
  const a = Math.max(0, roundPx(t));
  const rr = Math.max(0, roundPx(r));
  const bb = Math.max(0, roundPx(b));
  const ll = Math.max(0, roundPx(l));
  if (a === rr && rr === bb && bb === ll) return `${a}px`;
  if (a === bb && ll === rr) return `${a}px ${rr}px`;
  if (ll === rr) return `${a}px ${rr}px ${bb}px`;
  return `${a}px ${rr}px ${bb}px ${ll}px`;
}

function nonZero(rect) {
  return rect.width > 0 && rect.height > 0;
}

// ===== Computed-style readers =====

// Centralised visibility check. CSS `display: contents` is intentionally NOT
// considered hidden here — it generates no box of its own but its children
// still render, and the snapshot walker handles the pass-through separately.
function isVisible(computed) {
  if (computed.display === 'none') return false;
  if (computed.visibility === 'hidden') return false;
  if (parseFloat(computed.opacity) === 0) return false;
  return true;
}

// Read a single CSSOM declaration as a number, defaulting to 0 when the
// property is unset / empty / NaN. `getComputedStyle` always returns a
// string (`''` for missing entries) and `parseFloat('')` is NaN, so the
// `|| 0` is load-bearing wherever the result feeds arithmetic. Centralised
// so the dozen `parseFloat(computed.getPropertyValue(...)) || 0` sites
// across this file stop carrying their own copy.
function readNum(computed, prop) {
  return parseFloat(computed.getPropertyValue(prop)) || 0;
}

// Read a 4-sided CSS box (`padding-*`, `margin-*`, `border-*-width`, …) into
// a `{ top, right, bottom, left }` object. Tolerates missing / non-numeric
// values by coercing to 0 — `getComputedStyle` always returns a string but
// `parseFloat('')` is NaN, so the `|| 0` is load-bearing for the callers'
// arithmetic.
function readBox(computed, prefix, suffix) {
  const sfx = suffix ? `-${suffix}` : '';
  return {
    top:    readNum(computed, `${prefix}-top${sfx}`),
    right:  readNum(computed, `${prefix}-right${sfx}`),
    bottom: readNum(computed, `${prefix}-bottom${sfx}`),
    left:   readNum(computed, `${prefix}-left${sfx}`),
  };
}

function readPadding(computed) {
  return readBox(computed, 'padding');
}

function readMargin(computed) {
  return readBox(computed, 'margin');
}

function borderWidthOf(computed, side) {
  return readNum(computed, `border-${side}-width`);
}

// Read all three relevant declarations for one border side in a single helper
// so callers that need more than width (uniformity check, overlay paint,
// triangle hack detection) avoid sprinkling `getPropertyValue('border-${side}-
// width|style|color')` three times each. `widthRaw` is preserved as a string
// alongside the parsed `width` so `isUniformBorder` can string-compare the raw
// values (avoiding `'1px'` vs `'1.0px'` floating-point hazards) without
// re-reading the property.
function readBorderSide(computed, side) {
  const widthRaw = computed.getPropertyValue(`border-${side}-width`);
  return {
    widthRaw,
    width: parseFloat(widthRaw) || 0,
    style: computed.getPropertyValue(`border-${side}-style`),
    color: computed.getPropertyValue(`border-${side}-color`),
  };
}

// Resolve the origin used for measuring an element's children's `left`/`top`.
// CSS positions `position: absolute` descendants relative to the containing
// block's PADDING BOX, not its border box; `getBoundingClientRect()` reports
// the border box, so we shift inward by the parent's top/left border widths
// before passing the rect down to children. Without this shift, an
// absolutely-positioned child of a bordered ancestor lands one border-width
// too deep when the snapshot is reopened in a browser or fed to pagx
// (e.g. Tailwind's `border` + `inset-[1.5px]` battery icon: the inner fill
// bar should sit 1.5px from the inner padding edge, but emitting the raw
// border-box-relative offset (2.5px) drifts it by the 1px border).
function paddingBoxOrigin(rect, computed) {
  const bt = borderWidthOf(computed, 'top');
  const bl = borderWidthOf(computed, 'left');
  if (bt === 0 && bl === 0) return rect;
  return {
    left: rect.left + bl,
    top: rect.top + bt,
    right: rect.right,
    bottom: rect.bottom,
    width: rect.width,
    height: rect.height,
  };
}

// True iff all four borders have the same non-zero width, the same supported
// style (`solid`, `dashed`, or `dotted`), and the same color — i.e. cases the
// subset's single `border` shorthand can losslessly express. Compares the raw
// width string (not the parsed value) so 0.5px vs 0.5000001px noise from
// computed-style serialisation never sneaks past the equality check.
function isUniformBorder(computed) {
  const top = readBorderSide(computed, 'top');
  if (top.style !== 'solid' && top.style !== 'dashed' && top.style !== 'dotted') return false;
  if (top.width <= 0) return false;
  for (const side of SIDES) {
    if (side === 'top') continue;
    const s = readBorderSide(computed, side);
    if (s.widthRaw !== top.widthRaw) return false;
    if (s.style !== top.style) return false;
    if (s.color !== top.color) return false;
  }
  return true;
}

function hasAnyBorder(computed) {
  for (const side of SIDES) {
    if (borderWidthOf(computed, side) > 0) return true;
  }
  return false;
}

// Resolve a positioned element's z-index for paint-order sorting. Computed
// value `auto` means the author omitted z-index; CSS treats that as 0 for
// stacking purposes within the same stacking context. We DO NOT model real
// stacking contexts here — the subset flattens every box to `position:
// absolute`, so a single z-index dimension along the DOM is enough to
// capture the common "foreground decoration" pattern (a phone notch with
// `z-index: 50` painted over the gradient header, a sticky tab bar painted
// over the content beneath, …).
function resolveZIndex(computed) {
  const raw = (computed.getPropertyValue('z-index') || '').trim();
  if (!raw || raw === 'auto') return 0;
  const v = parseInt(raw, 10);
  return isFinite(v) ? v : 0;
}

// Replay CSS paint order: non-stackable (flow) children first, then
// stackable children sorted by z-index ascending, with DOM order as
// tie-break. A child is "stackable" when its z-index participates in
// stacking — that is, when it is `position` != static (CSS2 rule) OR
// when it is a flex / grid item (CSS Flexbox & Grid both honour
// `z-index` on items regardless of `position`). Without the
// flex/grid carve-out a `position: static` flex item authored with a
// high z-index (e.g. the slide-13 `.center-node { z-index: 20 }`
// sitting above its `.satellite-card` siblings at `z-index: 10`)
// would always sort to the back, painting underneath siblings the
// browser draws on top.
function paintOrder(a, b) {
  if (a.stackable !== b.stackable) return a.stackable ? 1 : -1;
  if (a.stackable && a.zIndex !== b.zIndex) return a.zIndex - b.zIndex;
  return a.domIndex - b.domIndex;
}

// ===== DOM-node helpers =====

function gatherDirectText(el) {
  let s = '';
  for (const n of el.childNodes) {
    if (n.nodeType === Node.TEXT_NODE) s += n.nodeValue;
  }
  return s.replace(/\s+/g, ' ').trim();
}

function elementHasChildren(el) {
  for (const c of el.children) {
    if (c.nodeType === Node.ELEMENT_NODE) return true;
  }
  return false;
}

// Single-pass `childNodes` scan that produces `{hasElementChild, directText}`.
// `render()` reaches a chain of branches (rect-degenerate guard, text-leaf
// dispatch, pseudo-text dispatch, inline-text-leaf candidate) that each
// independently consults `elementHasChildren` and/or `gatherDirectText`.
// Doing it one node at a time forces 3–4 separate `childNodes` walks per
// element on the common box path; this helper folds them into one. Callers
// that need only one of the two values still use the dedicated helpers
// above so they don't allocate the unused half.
function scanChildNodes(el) {
  let hasElementChild = false;
  let directText = '';
  for (const n of el.childNodes) {
    if (n.nodeType === Node.ELEMENT_NODE) {
      hasElementChild = true;
    } else if (n.nodeType === Node.TEXT_NODE) {
      directText += n.nodeValue;
    }
  }
  return { hasElementChild, directText: directText.replace(/\s+/g, ' ').trim() };
}

function firstTextNodeChild(el) {
  for (const n of el.childNodes) {
    if (n.nodeType === Node.TEXT_NODE) return n;
  }
  return null;
}

// Resolve the text content of `el`'s ::before / ::after pseudo-element. Used
// to surface icon-font glyphs (Phosphor, Font Awesome, Material Icons, ...)
// whose visible character is composed by the stylesheet via
// `content: "\e123"`. Without this hook the host element renders as an
// empty box in the snapshot because the glyph lives in a pseudo node that
// has no DOM presence.
//
// Returns '' when the pseudo has no content (the CSS defaults `none` /
// `normal`) or when the value can't be expressed as a plain string —
// `url(...)`, `attr(...)`, `counter(...)` and the quote keywords are not
// supported because we have no clean way to render them inside a `<span>`.
// Chromium's getComputedStyle returns the value with escapes already
// resolved into actual characters and string tokens wrapped in double
// quotes, so the parser only needs to extract quoted runs.
function pseudoText(el, pseudo) {
  const cs = getComputedStyle(el, pseudo);
  const raw = (cs.getPropertyValue('content') || '').trim();
  if (!raw || raw === 'none' || raw === 'normal') return '';
  let out = '';
  const re = /"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'/g;
  let m;
  while ((m = re.exec(raw)) !== null) {
    out += (m[1] !== undefined ? m[1] : m[2]);
  }
  return out;
}

// True when `el` has a non-empty ::before and/or ::after pseudo content.
// The cheap fast path for the common case of a non-decorative element
// (zero pseudo content) skips two extra getComputedStyle calls per node.
function hasPseudoContent(el) {
  return !!(pseudoText(el, '::before') || pseudoText(el, '::after'));
}

// Resolve the <img>'s source for the snapshot output. We prefer the inlined
// data URI produced by the pre-snapshot `inlineExternalImages` pass: PAGX's
// renderer only knows how to load local files and `data:` URIs, so leaving an
// `http(s)://` src would render as an empty box. Local/relative paths and
// `data:` URIs already work out of the box and are kept verbatim.
function imgSrc(el) {
  const inlined = el.getAttribute('data-snapshot-src');
  if (inlined) return inlined;
  return el.getAttribute('src') || '';
}

// Synthesize text content for form elements (placeholder / value / button
// label). For checkboxes / radios / file pickers / colour swatches / date
// pickers there is no meaningful text to surface; their `value` attribute is
// metadata (`"on"`, hidden file path, ISO date) that would only confuse
// downstream rendering, so we return empty and let `classify` drop the
// element.
function syntheticText(el) {
  const tag = el.tagName.toLowerCase();
  if (tag === 'textarea') {
    return (el.value || el.getAttribute('placeholder') || '').trim();
  }
  if (tag === 'input') {
    const type = (el.getAttribute('type') || 'text').toLowerCase();
    if (NON_TEXT_INPUT_TYPES.has(type)) return '';
    return (el.value || el.getAttribute('placeholder') || '').trim();
  }
  if (tag === 'select') {
    const opt = el.options && el.options[el.selectedIndex];
    return opt ? (opt.text || opt.label || opt.value || '').trim() : '';
  }
  return '';
}

// Apply CSS `text-transform` to a text fragment. The PAGX importer downstream
// has no `text-transform` model, and the `Range.getClientRects()` measurements
// upstream were already taken against the *rendered* (transformed) glyphs —
// so emitting the original mixed-case `nodeValue` would render with the
// pre-transform glyphs at widths that no longer match the measured rects
// (e.g. `<div class="uppercase">Complete Deconstruction</div>` would emit
// the literal mixed-case characters even though Chromium painted them as
// `COMPLETE DECONSTRUCTION`). Pre-transforming the text here keeps the
// emitted span's literal characters in agreement with the box widths.
//
// `capitalize` matches Chromium's whitespace-bounded word starts via a
// simple regex; punctuation-bounded words (e.g. "hello-world" → "Hello-world"
// vs. Chromium's "Hello-World") may diverge for non-Latin punctuation but
// match Chromium for the common ASCII whitespace cases. `full-width` and
// `full-size-kana` are CJK glyph remappings without a clean JS implementation
// and are passed through unchanged.
function applyTextTransform(text, computed) {
  if (!text) return text;
  const tt = String(computed.getPropertyValue('text-transform') || '').trim().toLowerCase();
  if (!tt || tt === 'none') return text;
  if (tt === 'uppercase') return text.toUpperCase();
  if (tt === 'lowercase') return text.toLowerCase();
  if (tt === 'capitalize') {
    return text.replace(/(^|\s)(\S)/g, (_, prefix, ch) => prefix + ch.toUpperCase());
  }
  return text;
}

// Decide whether to keep an element. Returns null to drop it.
function classify(el, computed) {
  const tag = el.tagName.toLowerCase();
  if (DROP_TAGS.has(tag)) return null;
  if (!isVisible(computed)) return null;
  if (tag === 'svg') return 'svg';
  if (tag === 'img') return 'img';
  if (tag === 'canvas') {
    // Canvas is supported only when `inlineCanvases` (the pre-snapshot pass)
    // managed to extract a data URI from the live bitmap. WebGL contexts
    // without `preserveDrawingBuffer: true`, tainted 2D canvases (with a
    // cross-origin `drawImage` source), and zero-sized canvases all fall
    // through to null and the element is dropped — same outcome as before
    // canvas support landed.
    return el.getAttribute('data-snapshot-canvas-src') ? 'canvas' : null;
  }
  if (tag === 'input' || tag === 'textarea' || tag === 'select') {
    if (syntheticText(el)) return 'text';
    // No visible text content (e.g. Material's floating-label
    // `placeholder=" "` trick that pairs with `:placeholder-shown`, or a
    // value-less input that paints only its border-bottom underline). Keep
    // the box when the author painted any box visuals so the underline /
    // background / shadow survives the snapshot.
    if (hasBoxVisualsForInline(computed)) return 'box';
    return null;
  }
  return 'box';
}

// ===== Style emission =====

// Append a single schema-driven CSS declaration to `parts` if its computed
// value isn't a default and survives normalisation. `ctx.textColor` (when
// provided) is consulted by entries flagged `skipIfEqualsTextColor`.
// `ctx.textColorOverride` lets callers substitute the rendered `color`
// value (e.g. an `<input>` whose visible text is the placeholder, painted
// with `::placeholder { color: ... }`) without doing post-hoc string
// surgery on the joined style — see renderTextInput.
function appendStyleProp(parts, computed, entry, ctx) {
  let raw = computed.getPropertyValue(entry.prop).trim();
  if (entry.prop === 'color' && ctx && ctx.textColorOverride) {
    raw = ctx.textColorOverride;
  }
  const v = entry.normalize ? entry.normalize(raw) : raw;
  if (!v) return;
  if (entry.defaults && entry.defaults.includes(v)) return;
  if (entry.skipIfEqualsTextColor && ctx && v === ctx.textColor) return;
  const outProp = entry.outProp || entry.prop;
  // Rewrite embedded double quotes as single quotes so the value embeds safely
  // inside the surrounding style="…" attribute. CSS treats single- and double-
  // quoted strings (and url("…") / url('…')) equivalently, so a value like
  // `filter: url("#blur")` round-trips while no longer prematurely closing the
  // attribute and breaking the XML parse on import.
  parts.push(`${outProp}: ${v.replace(/"/g, "'")}`);
}

// Forward a uniform `border` shorthand, preserving the actual line style
// (solid/dashed/dotted) so the importer can reproduce dash patterns. Asymmetric
// borders are emitted as overlay rectangles by borderOverlayHTML(); see that
// comment for the trade-off.
function appendBorder(parts, computed) {
  if (!isUniformBorder(computed)) return;
  const top = readBorderSide(computed, 'top');
  if (top.width <= 0) return;
  parts.push(`border: ${px(top.width)} ${top.style} ${top.color.trim()}`);
}

// Pass `box-shadow` through verbatim if non-none.
function appendBoxShadow(parts, computed) {
  const shadow = computed.getPropertyValue('box-shadow').trim();
  if (shadow && shadow !== 'none') parts.push(`box-shadow: ${shadow}`);
}

// Coalesce the two `-webkit-text-stroke` longhands Chromium's computed style
// exposes (`-webkit-text-stroke-width` / `-webkit-text-stroke-color`) into the
// single shorthand the HTML exporter emits and the importer parses. The
// shorthand round-trips into a PAGX text `<Stroke>` (the inverse of
// HTMLWriter::ResolveTextStrokeCss). Returns '' for a zero / missing width
// (the CSS default, which paints nothing) so unstroked text stays compact.
// `text-stroke-color` defaults to the resolved `color`; it is forwarded
// verbatim so the importer reproduces the authored stroke tint rather than
// re-deriving it from the fill.
function resolveTextStroke(computed) {
  const widthRaw = computed.getPropertyValue('-webkit-text-stroke-width').trim();
  const width = parseFloat(widthRaw);
  if (!(width > 0)) return '';
  let color = computed.getPropertyValue('-webkit-text-stroke-color').trim();
  if (!color) color = computed.getPropertyValue('color').trim();
  return `${widthRaw} ${color}`;
}

function appendTextStroke(parts, computed) {
  const value = resolveTextStroke(computed);
  if (value) parts.push(`-webkit-text-stroke: ${value}`);
}

// Forward `background-size` / `background-repeat` / `background-position` when the element
// carries a `url(...)` background image. The importer recovers them into the ImagePattern's
// scaleMode / tile modes / matrix (the inverse of the exporter). Gradients ignore these
// properties, so emitting them there would be noise; only forward for url backgrounds. Each
// value is suppressed when it equals the CSS default so the common fitted card stays compact.
function appendBackgroundImageFitting(parts, computed) {
  const bgImage = computed.getPropertyValue('background-image').trim();
  if (!/^url\(/i.test(bgImage)) return;
  const size = computed.getPropertyValue('background-size').trim();
  if (size && size !== 'auto') parts.push(`background-size: ${size}`);
  // `background-repeat` is always forwarded for url backgrounds (never suppressed against the
  // CSS default `repeat`): the importer maps `repeat` to the tiling ScaleMode::None pattern, so
  // dropping it would silently turn a tiled card into a single off-screen image.
  const repeat = computed.getPropertyValue('background-repeat').trim();
  if (repeat) parts.push(`background-repeat: ${repeat}`);
  const position = computed.getPropertyValue('background-position').trim();
  if (position && position !== '0% 0%') parts.push(`background-position: ${position}`);
}

// Forward the `mask-mode` / `mask-size` / `mask-position` / `mask-repeat`
// descriptors when the element carries a `mask-image`. The importer needs them
// to rebuild the PAGX mask layer at the right type, scale and offset (the inverse
// of HTMLWriter::writeMaskCSS, which always pins size/repeat and shifts position
// by the masked layer's render origin). On an unmasked box these descriptors are
// pure noise, so gate the whole group on a non-`none` `mask-image`. Each value is
// forwarded verbatim (no default suppression) because the exporter only emits them
// when they matter, and the importer treats a missing descriptor as the CSS default.
function appendMaskFitting(parts, computed) {
  const maskImage = computed.getPropertyValue('mask-image').trim();
  if (!maskImage || maskImage === 'none') return;
  const mode = computed.getPropertyValue('mask-mode').trim();
  if (mode && mode !== 'match-source') parts.push(`mask-mode: ${mode}`);
  const size = computed.getPropertyValue('mask-size').trim();
  if (size && size !== 'auto') parts.push(`mask-size: ${size}`);
  const position = computed.getPropertyValue('mask-position').trim();
  if (position && position !== '0% 0%' && position !== '0px 0px') {
    parts.push(`mask-position: ${position}`);
  }
  const repeat = computed.getPropertyValue('mask-repeat').trim();
  if (repeat && repeat !== 'repeat') parts.push(`mask-repeat: ${repeat}`);
}

// Build the style string for a kept element. `opts.box` includes background/
// border/shadow/etc.; `opts.text` includes color/font-*; `opts.positioned`
// toggles the absolute-positioning header (off for inline text children that
// ride on their parent's box). `opts.flexItem` swaps the absolute header for
// plain `width/height` so the element can participate in a parent flex flow
// without being taken out of it (PAGX's flex engine ignores items with
// `position: absolute` by spec; mixing them with flex siblings collapses the
// parent back to absolute on import).
//
// Flex items also receive `position: relative`. Their subtree contains
// `position: absolute` descendants (every non-flex-item kept element is
// emitted as absolute), and without a positioned ancestor those descendants
// would escape the flex item entirely when the snapshot is opened in a real
// browser — they would anchor to whichever positioned ancestor lives higher
// up the tree, collapsing every card's content into a single overlapping
// cluster. The PAGX importer treats `position: relative` as a no-op (PAGX has
// no containing-block concept; all children anchor to their direct parent),
// so the declaration is dropped during import and the flex layout is
// preserved.
//
// Flex items finally get `flex-shrink: 0`. Without it the browser applies the
// CSS default of `flex-shrink: 1`, which compresses every item proportionally
// whenever the flex container's natural content height exceeds its declared
// size (think a feed of post cards rendered into a 792-px phone screen with
// `overflow: hidden`: live content scrolls, but the snapshot is a flat
// capture, so the container is over-full by construction). The shrink would
// crush each item's measured height while leaving the `top: …px` offsets of
// its `position: absolute` descendants untouched — those descendants then
// leak outside the wrapper and disappear behind later siblings (the
// post's like row, topic banner, poll widget, etc. all vanish). PAGX's flex
// engine never shrinks, so the snapshot looked right under `pagx render` but
// diverged from Chromium; pinning the item size makes both engines agree.
// PAGX's importer warns about `flex-shrink` as outside the subset and drops
// it (HTMLStyleResolver.cpp), which is the desired no-op since PAGX already
// honours the explicit `height`.
function buildStyle(left, top, width, height, computed, opts) {
  const parts = [];
  if (opts.flexItem) {
    parts.push(`position: relative`);
    // `flex-grow` on the source flex item means CSS distributes the parent
    // container's leftover main-axis space into this item. Forward it as the
    // PAGX subset's `flex: <grow>` shorthand so the importer maps it onto
    // `Layer.flex` (HTMLLayerBuilder::applySizeAndPosition). When grow is
    // active, also drop the explicit main-axis dimension: PAGX ignores
    // `flex` whenever an explicit width/height is set on the same axis
    // (Layer.h doc), so emitting `width: Npx` alongside `flex: 1` would
    // pin the item to its measured size and silently negate the grow.
    // The cross-axis size still goes through verbatim. The parent's main
    // axis is forwarded via `opts.flexMainAxis` from `renderFlexContainer`;
    // a missing axis (e.g. flex item created outside the dedicated
    // container path) falls back to the conservative previous behaviour
    // — emit both dimensions and let `flex-shrink: 0` plus the measured
    // size pin the layout.
    //
    // A main-axis `max-width` / `max-height` on the item (e.g. Tailwind's
    // `max-w-xl` on a `flex-1` search-bar wrapper) caps how far the browser
    // lets the grow stretch the item — leftover main-axis space stays with
    // the parent and downstream siblings sit next to the capped item. PAGX's
    // HTML subset drops `max-width` / `max-height`
    // (HTMLSubsetPropertyTable.cpp), so forwarding bare `flex: <grow>`
    // would let PAGX hand the item the full leftover space and displace the
    // downstream siblings (the xiaohongshu_react header was the canonical
    // case: the `+发布` cluster jumped from x≈730 to x≈1177 because the
    // `flex-1 max-w-xl` search wrapper grew to 1010px instead of 576px).
    // When such a cap exists on the main axis, fall through to the pinned-
    // size branch so the measured layout survives.
    const grow = readNum(computed, 'flex-grow');
    const mainAxis = opts.flexMainAxis;
    const mainAxisMaxProp = mainAxis === 'column' ? 'max-height' : 'max-width';
    const mainAxisMax = (computed.getPropertyValue(mainAxisMaxProp) || '').trim().toLowerCase();
    const mainAxisCapped = mainAxisMax !== '' && mainAxisMax !== 'none';
    const growActive =
      grow > 0 && (mainAxis === 'row' || mainAxis === 'column') && !mainAxisCapped;
    if (growActive) {
      if (mainAxis === 'row') {
        parts.push(`height: ${px(height)}`);
      } else {
        parts.push(`width: ${px(width)}`);
      }
      parts.push(`flex: ${formatFlexGrow(grow)}`);
    } else {
      parts.push(`width: ${px(width)}`);
      parts.push(`height: ${px(height)}`);
      parts.push(`flex-shrink: 0`);
    }
    // CSS `margin` on a flex item shifts where its outer box sits inside the flex
    // container's main + cross axes (e.g. `mt-[8px]` on a 6×6 dot to align it with
    // text baseline). The PAGX HTML subset honours margin via padding-wrapper /
    // edge-fold (`HTMLParserContext::wrapWithMargin`), so forwarding the resolved
    // margin onto the subset HTML preserves the visual offset both in the live
    // browser preview and in `pagx render`. Suppress when every side is zero so
    // the common case stays as a clean `position/width/height/flex-shrink` row.
    const flexMargin = readMargin(computed);
    if (flexMargin.top || flexMargin.right || flexMargin.bottom || flexMargin.left) {
      parts.push(
        `margin: ${paddingShorthand(flexMargin.top, flexMargin.right,
                                    flexMargin.bottom, flexMargin.left)}`,
      );
    }
  } else if (opts.positioned !== false) {
    parts.push(`position: absolute`);
    parts.push(`left: ${px(left)}`);
    parts.push(`top: ${px(top)}`);
    parts.push(`width: ${px(width)}`);
    parts.push(`height: ${px(height)}`);
  }

  if (opts.box) {
    for (const entry of STYLE_SCHEMA_BOX) {
      appendStyleProp(parts, computed, entry);
    }
    // Border / shadow are emitted between the box-scope and text-scope blocks
    // so the resulting declaration order matches the historical hand-written
    // layout. See appendBorder / appendBoxShadow for the per-helper
    // rationale.
    appendBorder(parts, computed);
    appendBoxShadow(parts, computed);
    appendBackgroundImageFitting(parts, computed);
    appendMaskFitting(parts, computed);
  }

  if (opts.text) {
    const textColor = computed.getPropertyValue('color').trim();
    const ctx = { textColor, textColorOverride: opts.textColorOverride };
    for (const entry of STYLE_SCHEMA_TEXT) {
      appendStyleProp(parts, computed, entry, ctx);
    }
    appendTextStroke(parts, computed);
  } else if (opts.colorOnly) {
    // Icon wrappers (host of an inline <svg>) only need `color` so that any
    // currentColor stroke/fill picks up the right tint.
    appendStyleProp(parts, computed, STYLE_SCHEMA_BY_PROP.get('color'));
  }

  // Forward the host's resolved CSS `transform` (and `transform-origin`) when
  // the element-level wrapper in `render()` detected a non-identity transform
  // on this element. The wrapper measured this element's geometry under
  // `withStrippedTransform`, so the `left/top/width/height` above describe
  // the unrotated layout box; the browser then re-applies the same transform
  // when the snapshot is reopened, and the PAGX importer's `matrix(...)`
  // branch (`HTMLSubsetPropertyTable.cpp` / `HTMLStyleResolver.cpp`) writes
  // the six floats straight onto `Layer.matrix` so `pagx render` reproduces
  // the rotation/skew/scale as well. Origin defaults to `50% 50%` (CSS
  // default and the importer's only supported origin).
  if (opts.transform && opts.transform.transform) {
    parts.push(`transform: ${opts.transform.transform}`);
    if (opts.transform.transformOrigin) {
      parts.push(`transform-origin: ${opts.transform.transformOrigin}`);
    }
  }

  return parts.join('; ');
}

// Read the four per-corner border-radius values from computed style. CSS uses
// `Hpx` or `Hpx Vpx` (horizontal / vertical radii); we collapse to a single
// scalar by taking the horizontal component, which is enough for the uniform-
// radius fast path below (it bails the moment the four corners disagree).
function readCornerRadii(computed) {
  function radius(prop) {
    const raw = (computed.getPropertyValue(prop) || '').trim();
    if (!raw) return 0;
    return parseFloat(raw.split(/\s+/)[0]) || 0;
  }
  return {
    tl: radius('border-top-left-radius'),
    tr: radius('border-top-right-radius'),
    br: radius('border-bottom-right-radius'),
    bl: radius('border-bottom-left-radius'),
  };
}

// SVG-based renderer for the "uniform-width borders + uniform radius + per-side
// colours" pattern, as used by pure-CSS ring spinners:
//
//   .spinner {
//     width: 16px; height: 16px;
//     border: 2px solid rgba(255,255,255,0.4);
//     border-top-color: #fff;
//     border-radius: 50%;
//   }
//
// The plain rectangular-overlay fallback in `borderOverlayHTML` paints four
// straight strips and silently drops the host's `border-radius` (overlays use
// `position: absolute` and the host is not `overflow: hidden`), which collapses
// the ring into a square frame. CSS instead splits each corner along the
// diagonal between the outer and inner corner; for uniform width that diagonal
// is the 45° bisector. We reproduce this with one stroked <path> per side that
// covers (corner-arc-half) + (straight edge) + (next-corner-arc-half),
// stroke-width = border width, mid-line radius = r - w/2 (so the stroke's outer
// edge lands on the host's `border-radius` and its inner edge on the matching
// inner radius).
//
// Returns null when the pattern doesn't apply: per-side widths differ, radii
// disagree across corners, the radius is too small to host a ring (r ≤ w/2,
// where the inner corner collapses), or any side uses a non-solid style — in
// which case the caller falls back to rectangular overlays.
function roundedUniformBorderSvg(computed, width, height) {
  const top = readBorderSide(computed, 'top');
  const right = readBorderSide(computed, 'right');
  const bottom = readBorderSide(computed, 'bottom');
  const left = readBorderSide(computed, 'left');
  const w = top.width;
  if (w <= 0) return null;
  if (right.width !== w || bottom.width !== w || left.width !== w) return null;
  if (top.style !== 'solid' || right.style !== 'solid' ||
      bottom.style !== 'solid' || left.style !== 'solid') return null;
  const radii = readCornerRadii(computed);
  if (radii.tl <= 0) return null;
  if (radii.tr !== radii.tl || radii.br !== radii.tl || radii.bl !== radii.tl) return null;
  const r = radii.tl;
  if (r <= w / 2) return null;
  // Clamp the radius the same way CSS does when the corners would otherwise
  // overlap (r > min(W, H) / 2). Without the clamp a `border-radius: 50%` host
  // whose computed top-left-radius was reported per-corner (Chromium can do
  // this for non-square boxes) would draw a path larger than the box.
  const maxR = Math.min(width, height) / 2;
  const rOuter = Math.min(r, maxR);
  if (rOuter <= w / 2) return null;
  const rMid = rOuter - w / 2;
  const W = width;
  const H = height;
  const centers = {
    tl: [rOuter, rOuter],
    tr: [W - rOuter, rOuter],
    br: [W - rOuter, H - rOuter],
    bl: [rOuter, H - rOuter],
  };
  function pt(center, angleDeg) {
    const a = angleDeg * Math.PI / 180;
    return [center[0] + rMid * Math.cos(a), center[1] + rMid * Math.sin(a)];
  }
  function fmt(p) {
    return `${roundPx(p[0])} ${roundPx(p[1])}`;
  }
  // Eight corner-cut points (two per corner) plus eight tangent points where
  // each arc meets the straight edge segment. SVG y-axis points down, so 270°
  // is up, 0° is right, 90° is down, 180° is left.
  const tl225 = pt(centers.tl, 225);
  const tl270 = pt(centers.tl, 270);
  const tr270 = pt(centers.tr, 270);
  const tr315 = pt(centers.tr, 315);
  const tr0   = pt(centers.tr, 0);
  const br0   = pt(centers.br, 0);
  const br45  = pt(centers.br, 45);
  const br90  = pt(centers.br, 90);
  const bl90  = pt(centers.bl, 90);
  const bl135 = pt(centers.bl, 135);
  const bl180 = pt(centers.bl, 180);
  const tl180 = pt(centers.tl, 180);
  const arc = `${roundPx(rMid)} ${roundPx(rMid)} 0 0 1`;
  const segments = [
    { color: top.color,    d: `M ${fmt(tl225)} A ${arc} ${fmt(tl270)} L ${fmt(tr270)} A ${arc} ${fmt(tr315)}` },
    { color: right.color,  d: `M ${fmt(tr315)} A ${arc} ${fmt(tr0)}   L ${fmt(br0)}  A ${arc} ${fmt(br45)}`  },
    { color: bottom.color, d: `M ${fmt(br45)}  A ${arc} ${fmt(br90)}  L ${fmt(bl90)} A ${arc} ${fmt(bl135)}` },
    { color: left.color,   d: `M ${fmt(bl135)} A ${arc} ${fmt(bl180)} L ${fmt(tl180)} A ${arc} ${fmt(tl225)}` },
  ];
  const paths = segments
    .filter((s) => colorAlpha(s.color) > 0)
    .map((s) => `<path d="${s.d}" fill="none" stroke="${s.color.trim()}" ` +
      `stroke-width="${px(w)}" stroke-linecap="butt"/>`)
    .join('');
  if (!paths) return '';
  return `<svg xmlns="http://www.w3.org/2000/svg" style="position: absolute; ` +
    `left: 0px; top: 0px; width: ${px(W)}; height: ${px(H)}; pointer-events: none" ` +
    `width="${px(W)}" height="${px(H)}" viewBox="0 0 ${roundPx(W)} ${roundPx(H)}">${paths}</svg>`;
}

// Returns an array of HTML strings — one absolutely-positioned <div> per
// non-zero border side — to overlay onto the host box. Used when the element
// has an asymmetric border (e.g. `border-bottom: 1px solid #e5e7eb` on a list
// row).
//
// When the host has uniform border widths + uniform border-radius + only
// asymmetric *colours* (the canonical CSS ring-spinner pattern), the overlay
// rectangles can't honour the radius (they're absolutely positioned and the
// host has no `overflow: hidden`), so the radius would be silently dropped and
// the spinner would render as a square frame. `roundedUniformBorderSvg`
// short-circuits that case with one stroked <path> per side; the overlay
// rectangle path stays for genuinely asymmetric-width borders (list-row
// dividers, single-side underlines, etc.) where the radius rarely matters.
function borderOverlayHTML(computed, width, height) {
  if (isUniformBorder(computed)) return [];
  const ringSvg = roundedUniformBorderSvg(computed, width, height);
  if (ringSvg !== null) return ringSvg ? [ringSvg] : [];
  const out = [];
  for (const side of SIDES) {
    const b = readBorderSide(computed, side);
    if (b.width <= 0) continue;
    // The subset has no model for dashed/dotted/double per-side borders.
    // Downgrade them to solid: it's an approximation but preserves the
    // visual divider, which is what the page actually wanted.
    if (b.style === 'none' || b.style === 'hidden') continue;
    const r = SIDE_OVERLAY[side](width, height, b.width);
    out.push(`<div style="position: absolute; left: ${px(r.left)}; top: ${px(r.top)}; ` +
      `width: ${px(r.w)}; height: ${px(r.h)}; background-color: ${b.color.trim()}"></div>`);
  }
  return out;
}

// Resolve the alpha channel of a computed colour string. `transparent` and
// `rgba(..., 0)` both round-trip to alpha 0; opaque keywords / `rgb(...)`
// resolve to 1. Used to tell whether a border / background colour actually
// paints anything.
function colorAlpha(colorStr) {
  if (!colorStr) return 0;
  const s = colorStr.trim().toLowerCase();
  if (s === 'transparent' || s === 'none') return 0;
  const m = s.match(/^rgba?\(([^)]+)\)$/);
  if (!m) return 1;
  const parts = m[1].split(',').map((p) => p.trim());
  if (parts.length < 4) return 1;
  const a = parseFloat(parts[3]);
  return Number.isFinite(a) ? a : 1;
}

// Parse the computed `transform-origin` (e.g. "100px 140px" or "50% 50%")
// against an element box of size (w, h). Returns the origin in element-box
// coordinates. Modern browsers always resolve transform-origin to absolute
// pixels in the computed style, but we still accept percentages defensively.
function transformOriginXY(computed, w, h) {
  const raw = (computed.transformOrigin || '').trim();
  if (!raw) return [w / 2, h / 2];
  const parts = raw.split(/\s+/);
  function resolve(token, total) {
    if (!token) return total / 2;
    if (token.endsWith('%')) return total * (parseFloat(token) || 0) / 100;
    return parseFloat(token) || 0;
  }
  return [resolve(parts[0], w), resolve(parts[1], h)];
}

// Build a DOMMatrix from a computed `transform` string. `none` (the default)
// returns the identity matrix.
function transformMatrix(computed) {
  const t = (computed.transform || '').trim();
  if (!t || t === 'none') return new DOMMatrix();
  return new DOMMatrix(t);
}

// Read the inline `transform` and `transform-origin` declarations off an
// element. Returns `null` when no inline transform is set; otherwise returns
// the original CSS function strings (e.g. `rotate(10deg)`, `skewX(-5deg)`)
// preserved verbatim. Used by the text-leaf path: a single-function inline
// `transform` is forwarded to the inner `<span>` so PAGX's importer maps it
// onto the TextBox's own skew/rotation/scale fields. Computed style would
// always serialise to `matrix(...)`, which the TextBox path historically
// could not parse — that path therefore relies on the source function
// strings still being present in `el.style`.
function readInlineTransform(el) {
  if (!el || !el.style) return null;
  const t = (el.style.transform || '').trim();
  if (!t || t === 'none') return null;
  const o = (el.style.transformOrigin || '').trim();
  return { transform: t, transformOrigin: o };
}

// Read the resolved `transform` off an element from its computed style.
// Returns `null` when the resolved value is identity (the property
// effectively no-ops). Otherwise returns the computed CSS string — almost
// always a `matrix(a, b, c, d, tx, ty)` token — paired with the computed
// `transform-origin` (resolved to absolute pixels by Chromium).
//
// Why computed style instead of `el.style.transform`? Modern stylesheets and
// utility frameworks (Tailwind's `rotate-45`, `scale-110`, …) deliver
// `transform` through CSS class rules; the inline `style` slot is then
// empty, but `getComputedStyle(el).transform` still surfaces the resolved
// matrix. The downstream renderer needs the matrix to bake the rotation /
// skew / scale into the wrapper, and the PAGX importer's `matrix(...)`
// branch (HTMLSubsetPropertyTable.cpp / HTMLStyleResolver.cpp) writes the
// six floats straight into `Layer.matrix` regardless of the source CSS
// function used — so we never need to recover the original function form.
function readBoxTransform(computed) {
  if (!computed) return null;
  const t = (computed.transform || '').trim();
  if (!t || t === 'none') return null;
  if (t === 'matrix(1, 0, 0, 1, 0, 0)') return null;
  const o = (computed.transformOrigin || '').trim();
  return { transform: t, transformOrigin: o };
}

// Run `fn` with the element's inline transform temporarily cleared, then
// restore the original declarations. Used by the text path to obtain
// pre-transform `getBoundingClientRect()` / `Range.getClientRects()`
// measurements so the geometry handed to PAGX matches the unrotated layout
// the TextBox node will reproduce after applying its own skew/rotation/scale.
function withStrippedTransform(el, fn) {
  if (!el || !el.style) return fn();
  const savedT = el.style.transform;
  const savedO = el.style.transformOrigin;
  el.style.transform = 'none';
  if (savedO) el.style.transformOrigin = '';
  try {
    return fn();
  } finally {
    el.style.transform = savedT;
    if (savedO) el.style.transformOrigin = savedO;
  }
}

// Detect the classic "0×0 element + asymmetric borders to paint a triangle"
// pattern. The CSS rules look like:
//   width: 0; height: 0;
//   border-left: 100px solid transparent;
//   border-right: 100px solid transparent;
//   border-bottom: 280px solid #3a5afd;
// The element box is degenerate, but the four border regions form four
// triangles meeting at the (BL, BT) corner; the visible border-colour sides
// paint the visible parts of the shape. We can't represent this with a single
// `border` shorthand or an axis-aligned overlay rectangle, so we render the
// element as an inline <svg> with one <polygon> per visible side.
function isCssBorderTrianglePattern(el, computed) {
  if (elementHasChildren(el)) return false;
  if (gatherDirectText(el)) return false;
  // The element's *content* area must be empty for the four border regions to
  // collapse into the four-triangles-meet-at-(BL,BT) configuration that the
  // CSS triangle hack relies on. `clientWidth`/`clientHeight` report the
  // padding-box size (content + padding, no border), so they are zero exactly
  // when the box has no content area regardless of `box-sizing`. Reading
  // `width` from computed style is unreliable here: under `box-sizing:
  // border-box` (Tailwind's preflight default) Chromium clamps `width: 0` up
  // to `border-left + border-right`, so the computed width on a triangle host
  // is its full border span (e.g. 200px), not 0.
  if (el.clientWidth !== 0 || el.clientHeight !== 0) return false;
  // Defence in depth: if the author added padding the four triangles would
  // get pushed apart and the shape would no longer be a clean triangle.
  const pad = readPadding(computed);
  if (pad.top !== 0 || pad.right !== 0 || pad.bottom !== 0 || pad.left !== 0) return false;
  let visibleSides = 0;
  for (const side of SIDES) {
    const b = readBorderSide(computed, side);
    if (b.width <= 0) continue;
    if (b.style !== 'solid') return false;
    if (colorAlpha(b.color) > 0) visibleSides++;
  }
  if (visibleSides === 0) return false;
  if (colorAlpha(computed.getPropertyValue('background-color')) > 0) return false;
  const bgImage = (computed.getPropertyValue('background-image') || '').trim();
  if (bgImage && bgImage !== 'none') return false;
  return true;
}

// Render a CSS-triangle element as an absolutely-positioned wrapper containing
// an inline <svg>. The svg's viewBox matches the host's paint-box rect (which
// is the transform-aware bounding rect that `getBoundingClientRect()` reports).
// Each visible border side becomes one <polygon> with three vertices in the
// unrotated element coordinates, then run through the element's own CSS
// `transform` so the rotation / skew / scale gets baked into the polygon
// points. We do this baking here because PAGX's HTML subset has no model for
// CSS `transform`; without baking, the snapshot would lose the rotation and
// the triangle would land axis-aligned inside an oversized paint-box wrapper.
function renderBorderTriangle(el, parentRect, rect, left, top, computed, opts) {
  const BL = borderWidthOf(computed, 'left');
  const BR = borderWidthOf(computed, 'right');
  const BT = borderWidthOf(computed, 'top');
  const BB = borderWidthOf(computed, 'bottom');
  const unrotW = BL + BR;
  const unrotH = BT + BB;
  const paintW = rect.width;
  const paintH = rect.height;
  const m = transformMatrix(computed);
  const [ox, oy] = transformOriginXY(computed, unrotW, unrotH);

  // Project the four corners of the unrotated element box through the
  // transform to find the paint-box's top-left in pre-transform world units.
  // The wrapper is sized to the paint box, so subtracting (minX, minY) lands
  // every transformed point inside the wrapper's local (0,0)-(paintW, paintH).
  const corners = [[0, 0], [unrotW, 0], [unrotW, unrotH], [0, unrotH]];
  let minX = Infinity;
  let minY = Infinity;
  for (const [cx, cy] of corners) {
    const p = m.transformPoint(new DOMPoint(cx - ox, cy - oy));
    const qx = p.x + ox;
    const qy = p.y + oy;
    if (qx < minX) minX = qx;
    if (qy < minY) minY = qy;
  }
  function mapPoint(ux, uy) {
    const p = m.transformPoint(new DOMPoint(ux - ox, uy - oy));
    return [p.x + ox - minX, p.y + oy - minY];
  }
  const triangles = {
    top:    [[0, 0],      [unrotW, 0],      [BL, BT]],
    right:  [[unrotW, 0], [unrotW, unrotH], [BL, BT]],
    bottom: [[0, unrotH], [unrotW, unrotH], [BL, BT]],
    left:   [[0, 0],      [BL, BT],         [0, unrotH]],
  };
  const polys = [];
  for (const side of SIDES) {
    const b = readBorderSide(computed, side);
    if (b.width <= 0) continue;
    const color = b.color.trim();
    if (colorAlpha(color) <= 0) continue;
    const pts = triangles[side]
      .map(([x, y]) => mapPoint(x, y))
      .map(([x, y]) => `${roundPx(x)},${roundPx(y)}`)
      .join(' ');
    polys.push(`<polygon points="${pts}" fill="${color}"/>`);
  }
  const wrapperStyle = buildStyle(left, top, paintW, paintH, computed, {
    box: true, ...opts,
  });
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${px(paintW)}" `
    + `height="${px(paintH)}" viewBox="0 0 ${roundPx(paintW)} ${roundPx(paintH)}">`
    + polys.join('') + `</svg>`;
  return `<div style="${wrapperStyle}">${svg}</div>`;
}

// Style of the inner <img> element. Under absolute mode it pins itself to the
// wrapper's (0,0) and inherits the wrapper's size; under flex mode the
// wrapper is no longer absolutely positioned, so the image fills the wrapper
// via flow sizing instead.
//
// `objectFit` is the computed CSS `object-fit` keyword from the source
// element (`'fill' | 'contain' | 'cover' | 'none' | 'scale-down'`). It is
// forwarded verbatim so the PAGX subset importer can map it onto the
// ImagePattern's ScaleMode. Default (`fill` / empty) is omitted; without this
// pass-through the importer would always default to ScaleMode::Stretch and
// thumbnails authored with `object-fit: cover` (CMS hero images, avatar
// crops, …) would render with the wrong aspect ratio against the live
// browser baseline.
function imageInnerStyle(rect, flexItem, objectFit) {
  const base = flexItem
    ? `width: 100%; height: 100%`
    : `position: absolute; left: 0px; top: 0px; ` +
      `width: ${px(rect.width)}; height: ${px(rect.height)}`;
  if (objectFit && objectFit !== 'fill') {
    return `${base}; object-fit: ${objectFit}`;
  }
  return base;
}

// Walk an SVG subtree and freeze the CSS-resolved paint state onto each
// cloned element so the snapshot renders identically without the page's
// original stylesheet:
//
//   1. `currentColor` / `context-fill` / `context-stroke` literal attribute
//      values get rewritten to the node's resolved color.
//   2. Every presentation attribute in `SVG_PRESENTATION_ATTRS` (defined
//      below as a function-local constant so it ships into the browser
//      payload via `fn.toString()`) whose resolved value differs from what
//      the cloned ancestor chain already provides gets written as an
//      attribute on the clone. Author-emitted attributes (preserved by
//      `cloneNode(true)`) are kept as-is — we only add what would otherwise
//      be lost when the page's stylesheet is dropped.
//
// The clone is detached from the document, so `getComputedStyle` can't
// resolve it directly; we walk the *original* subtree (where the cascade is
// live) and the clone in lockstep, threading the resolved color and the
// resolved presentation map down the recursion.
function freezeSvg(svgEl, rect) {
  // SVG presentation attributes that real-world pages routinely set via CSS
  // rules (e.g. `.icon path { stroke: #fff; stroke-width: 2; }`) rather
  // than as element attributes in markup. The subset HTML throws away the
  // page's stylesheets, so any such CSS-driven paint disappears unless we
  // freeze the resolved value onto the cloned element here. The list
  // covers the paint / stroke / fill / opacity / vector-effect family —
  // the attributes that actually affect what the SVG paints. Layout-only
  // properties (width, height, x, y, transform) are left to the SVG's own
  // attributes.
  const SVG_PRESENTATION_ATTRS = [
    'fill', 'fill-opacity', 'fill-rule',
    'stroke', 'stroke-opacity', 'stroke-width',
    'stroke-linecap', 'stroke-linejoin', 'stroke-miterlimit',
    'stroke-dasharray', 'stroke-dashoffset',
    'opacity', 'visibility', 'clip-rule', 'vector-effect',
  ];
  // SVG-spec defaults for the attributes above. Used as the inheritance
  // root so the top-level <svg>'s computed values only get serialised when
  // they differ from the spec default. A missing entry is treated as "" so
  // any non-empty resolved value is emitted.
  const SVG_PRESENTATION_DEFAULTS = {
    'fill': 'rgb(0, 0, 0)',
    'fill-opacity': '1',
    'fill-rule': 'nonzero',
    'stroke': 'none',
    'stroke-opacity': '1',
    'stroke-width': '1px',
    'stroke-linecap': 'butt',
    'stroke-linejoin': 'miter',
    'stroke-miterlimit': '4',
    'stroke-dasharray': 'none',
    'stroke-dashoffset': '0px',
    'opacity': '1',
    'visibility': 'visible',
    'clip-rule': 'nonzero',
    'vector-effect': 'none',
  };
  const readPresentation = (cs) => {
    const out = {};
    for (let i = 0; i < SVG_PRESENTATION_ATTRS.length; i++) {
      const name = SVG_PRESENTATION_ATTRS[i];
      out[name] = (cs.getPropertyValue(name) || '').trim();
    }
    return out;
  };
  const clone = svgEl.cloneNode(true);
  // Drop the SVG's own positioning: `renderSvg` wraps the frozen markup in a
  // flex-centring `<div>` that already carries the SVG's measured `(left, top)`
  // against its parent. An author-emitted `style="position:absolute;top:..;left:.."`
  // on the `<svg>` (as the PAGX HTMLExporter writes for absolutely-placed icons)
  // would then offset the shape a second time relative to that wrapper, landing it
  // at roughly double its intended position. Stripping these here mirrors how the
  // inner `<img>` anchors to its wrapper's origin (see `imageInnerStyle`).
  if (clone.nodeType === 1 && clone.style) {
    clone.style.removeProperty('position');
    clone.style.removeProperty('top');
    clone.style.removeProperty('left');
    clone.style.removeProperty('right');
    clone.style.removeProperty('bottom');
    // `margin` is the same class of offset: getBoundingClientRect already
    // reports the position margin pushed the SVG to, so a retained margin would
    // shift it a second time inside the centring wrapper. Strip the shorthand
    // and each longhand (the SVG may carry either spelling).
    clone.style.removeProperty('margin');
    clone.style.removeProperty('margin-top');
    clone.style.removeProperty('margin-right');
    clone.style.removeProperty('margin-bottom');
    clone.style.removeProperty('margin-left');
    // `opacity` is a wrapper-owned box property too: `renderSvg`'s flex-centring
    // `<div>` already carries the SVG's computed `opacity` (it is in STYLE_SCHEMA's
    // `box` scope). Leaving the same opacity on the `<svg>` would composite it twice
    // — wrapper × svg — washing the shape out (0.7 × 0.7 ≈ 0.49). Strip the root's own
    // opacity here; the walk below also skips freezing it back as an attribute. Only the
    // root is affected — CSS `opacity` does not inherit, so descendants keep theirs.
    clone.style.removeProperty('opacity');
  }
  // A directly-authored `opacity` attribute on the root <svg> is the same wrapper-owned
  // value in attribute form; drop it so the walk's freeze pass treats the root as having
  // no opacity to carry.
  if (clone.nodeType === 1 && clone.removeAttribute) {
    clone.removeAttribute('opacity');
  }
  // Inline SVGs in real pages usually rely on CSS classes (e.g. Tailwind `w-4
  // h-4`) to set their on-screen size and leave the `<svg>` element itself
  // without `width`/`height` attributes. PAGX's import pipeline does not
  // interpret those classes, so the resulting SVG would fall back to its
  // viewBox extents (typically 24×24) and get clipped by its parent wrapper.
  // Pin the dimensions here from the browser's measured rect so the importer
  // scales the viewBox into the visible box.
  if (rect && Number.isFinite(rect.width) && Number.isFinite(rect.height)) {
    clone.setAttribute('width', String(Math.round(rect.width * 1000) / 1000));
    clone.setAttribute('height', String(Math.round(rect.height * 1000) / 1000));
  }
  const fallback = (getComputedStyle(svgEl).color || 'rgb(0, 0, 0)').trim();
  const walk = (orig, dst, inheritedColor, inheritedAttrs, isRoot) => {
    // SVG elements inherit `color` from their CSS parent. We only re-query
    // getComputedStyle if the node is actually an element (text nodes inside
    // <text> don't carry style on their own).
    let here = inheritedColor;
    let resolvedAttrs = inheritedAttrs;
    if (orig && orig.nodeType === 1) {
      const cs = getComputedStyle(orig);
      if (cs && cs.color) here = cs.color.trim() || here;
      resolvedAttrs = readPresentation(cs);
    }
    if (dst && dst.nodeType === 1 && dst.getAttribute) {
      const stroke = dst.getAttribute('stroke');
      const fill = dst.getAttribute('fill');
      if (stroke === 'currentColor' || stroke === 'context-stroke') {
        dst.setAttribute('stroke', here);
      }
      if (fill === 'currentColor' || fill === 'context-fill') {
        dst.setAttribute('fill', here);
      }
      // Inline `style="stroke: currentColor"` is uncommon but legal.
      if (dst.style) {
        if (dst.style.stroke === 'currentColor') dst.style.stroke = here;
        if (dst.style.fill === 'currentColor') dst.style.fill = here;
        if (dst.style.color === 'currentColor') dst.style.color = here;
      }
      // Freeze any CSS-resolved presentation attribute that the cloned
      // ancestor chain doesn't already supply. This is what carries
      // `<style>.cls path { stroke: ... }` rules into the snapshot.
      for (let i = 0; i < SVG_PRESENTATION_ATTRS.length; i++) {
        const name = SVG_PRESENTATION_ATTRS[i];
        // The root <svg>'s own `opacity` is hoisted onto the wrapper `<div>` (see the
        // strip above), so never freeze it back onto the root. Descendants still freeze
        // their opacity normally — it composites under the wrapper exactly once.
        if (isRoot && name === 'opacity') continue;
        if (dst.hasAttribute(name)) continue;
        const value = resolvedAttrs[name];
        if (!value) continue;
        const inherited = inheritedAttrs[name] || SVG_PRESENTATION_DEFAULTS[name] || '';
        if (value === inherited) continue;
        dst.setAttribute(name, value);
      }
    }
    const origKids = (orig && orig.children) || [];
    const dstKids = (dst && dst.children) || [];
    const n = Math.min(origKids.length, dstKids.length);
    for (let i = 0; i < n; i++) walk(origKids[i], dstKids[i], here, resolvedAttrs, false);
  };
  walk(svgEl, clone, fallback, SVG_PRESENTATION_DEFAULTS, true);
  // outerHTML serialises U+00A0 as the named entity &nbsp;, which the importer's
  // XML parser rejects (XML predefines only &amp; &lt; &gt; &quot; &apos;).
  // Rewrite it to the numeric character reference so the frozen SVG stays valid
  // XML; every other entity the HTML serialiser emits is already XML-legal.
  return clone.outerHTML.replace(/&nbsp;/g, '&#160;');
}

// ===== Multi-line text emission =====

// Split a text node into one entry per visually-rendered line (horizontal
// writing modes) or column (vertical writing modes). We use
// Range.getClientRects() to obtain the per-line/column bounding boxes from the
// browser's own layout, then binary-search the character offsets at which one
// line/column gives way to the next. This matches Chromium's line-break
// decisions exactly while keeping the work to O(lines · log n)
// getBoundingClientRect calls instead of the O(n) per-character scan the
// original implementation performed.
//
// `axis` selects the block-progression axis along which the run wraps:
//   - 'y' (horizontal writing modes): lines stack downward, so the per-line
//     rects arrive in increasing `top` order and the bisection key is a
//     character's mid-Y.
//   - 'x-rl' (vertical-rl): columns stack leftward, so the rects arrive in
//     DECREASING `left` order (rightmost column first, matching document
//     order); the bisection key is the NEGATED mid-X so it stays monotonically
//     increasing like the 'y' case.
//   - 'x-lr' (vertical-lr): columns stack rightward, rects in increasing
//     `left`; key is mid-X.
// Treating all three as "a monotonically increasing key along the block axis"
// lets one bisection serve every writing mode.
//
// The host element's `white-space` controls whether embedded whitespace is
// collapsed (`normal`, `nowrap`) or preserved (`pre`, `pre-wrap`, `pre-line`).
// Collapsing in preserve modes would mangle code snippets, indentation, and
// other pre-formatted content.
function splitTextNodeIntoLines(textNode, whiteSpace, axis) {
  const blockAxis = axis || 'y';
  // Map a rect to its block-axis LEADING edge (the side a column/line starts
  // from in progression order) and a character's mid-point to the same
  // monotonically-increasing key, so "first char whose key >= the next rect's
  // leading-edge key" lands exactly on that rect's first character.
  //   - 'y': lines advance downward, leading edge = rect.top, char key = mid-Y.
  //   - 'x-lr': columns advance rightward, leading edge = rect.left, char key
  //     = mid-X.
  //   - 'x-rl': columns advance leftward; once X is negated to make the order
  //     ascending, the leading edge becomes the column's RIGHT edge
  //     -(left + width), NOT -left. Using -left would place the threshold at
  //     the column's trailing edge, half a column past the glyph centres, so
  //     every column's characters would be misassigned to the column before it.
  const rectKey = (r) => {
    if (blockAxis === 'x-rl') return -(r.left + r.width);
    if (blockAxis === 'x-lr') return r.left;
    return r.top;
  };
  const charKey = (r) => {
    if (blockAxis === 'x-rl') return -(r.left + r.width / 2);
    if (blockAxis === 'x-lr') return r.left + r.width / 2;
    return r.top + r.height / 2;
  };
  const text = textNode.nodeValue || '';
  if (!text.trim()) return [];
  const preserve = typeof whiteSpace === 'string' && whiteSpace.startsWith('pre');
  // A preserve-mode slice can be bounded by the forced break characters that
  // visually terminate the line above/below it: a trailing run on the line it
  // ends, and (only for the whole text's leading blank lines, which collapse to
  // a single rect) a leading run. Strip both runs of `\r` / `\n` so the emitted
  // (nowrap) span carries no stray hard break; leading/internal spaces — i.e.
  // indentation — stay intact. The line's vertical offset is already encoded in
  // its rect, so dropping the break characters never loses positioning.
  // Collapsing modes fold + trim all whitespace as before.
  const cleanLine = (s) => {
    const stripped = s.replace(/^[\r\n]+/, '').replace(/[\r\n]+$/, '');
    return preserve ? stripped : stripped.replace(/\s+/g, ' ').trim();
  };
  const range = document.createRange();
  range.selectNodeContents(textNode);
  // Chromium emits a spurious zero-width client rect at a forced `\n` break that
  // shares the SAME visual line (and `top`) as the preceding text. The
  // bisection below treats every rect as a distinct line, so that
  // zero-width rect would split one line's text at an arbitrary character offset
  // (e.g. `Hello\nWorld` → `H` + `ello`). Drop zero-area rects up front: they
  // paint nothing, and a genuinely empty line is discarded later anyway because
  // its sliced text is whitespace-only.
  const lineRects = Array.from(range.getClientRects()).filter(
    (r) => r.width > 0 && r.height > 0,
  );
  if (lineRects.length === 0) {
    range.detach && range.detach();
    return [];
  }
  if (lineRects.length === 1) {
    const out = [{
      text: cleanLine(text),
      rect: lineRects[0],
    }];
    range.detach && range.detach();
    return out.filter((b) => b.text);
  }

  // Binary-search the first character index whose block-axis mid-key is >=
  // `lineKey` within [lo, hi). When the candidate char has an empty rect
  // (whitespace collapsed at a wrap point) we treat it as before the boundary
  // and step right; this keeps the search O(log n) at the cost of binding
  // collapsed whitespace to whichever side wins the bisection (visually
  // invisible either way).
  //
  // `loStart` lets the caller pass the previously-found boundary as the
  // lower bound, since boundaries are strictly increasing across lines.
  // Without this, every line's search starts from index 0 and re-flushes
  // layout for the leading characters that we already know belong to a
  // higher line — for a 1000-char × 20-line block that's the difference
  // between log2(1000)·20 ≈ 200 layout flushes and roughly half that.
  function findFirstCharAtOrBelow(lineKey, loStart) {
    let lo = loStart;
    let hi = text.length;
    while (lo < hi) {
      const mid = (lo + hi) >>> 1;
      range.setStart(textNode, mid);
      range.setEnd(textNode, mid + 1);
      const r = range.getBoundingClientRect();
      const empty = r.width === 0 && r.height === 0;
      const key = empty ? -Infinity : charKey(r);
      if (empty || key < lineKey) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    return lo;
  }

  // Boundary i = first char index of line i (boundaries[0] = 0).
  const boundaries = [0];
  for (let k = 1; k < lineRects.length; k++) {
    boundaries.push(findFirstCharAtOrBelow(rectKey(lineRects[k]), boundaries[k - 1] + 1));
  }
  boundaries.push(text.length);
  range.detach && range.detach();

  const out = [];
  for (let i = 0; i < lineRects.length; i++) {
    const slice = text.slice(boundaries[i], boundaries[i + 1]);
    const cleaned = cleanLine(slice);
    if (cleaned) out.push({ text: cleaned, rect: lineRects[i] });
  }
  return out;
}

// Emit one absolutely-positioned, nowrap <span> per line of `textNode`,
// positioned relative to `parentRect`. Chromium's `Range.getClientRects()`
// reports glyph INK bounds, not line-box bounds — and the two diverge in
// both directions:
//
//   - Latin block text: ink is SHORTER than `line-height` (`getClientRects`
//     gives a 19px rect for `font-size:16; line-height:24`). Expand the
//     span to the full line-height and re-centre it around the original
//     glyph mid-line. Without this, PAGX's text engine — whose
//     baseline/leading model differs from Chromium's by sub-pixel amounts —
//     renders the final line a hair below the span's declared bottom,
//     clipping descenders inside a `line-clamp` wrapper whose height was
//     pinned to exactly N × line-height.
//
//   - CJK italic / synthetic-italic text: ink is TALLER than `line-height`
//     because the font's ascent + descent exceeds the chosen leading
//     (`font-size:72; line-height:82.8` measures as a 104px rect, with the
//     ink extending ~11px above and ~10px below the line-box). Using
//     `rect.top` as the span's top would shift the rendered line-box up by
//     the same ~11px because both Chromium and PAGX anchor the line-box at
//     the span's top edge — making the rendered text bottom drift far
//     enough above the original to misalign every following sibling (e.g.
//     a `mt-24` divider that suddenly sits further from the visible text
//     than the source CSS asked for). Clamp the span back to line-box
//     geometry: width is unchanged (skewed glyphs still need the wider
//     bounding box), but height collapses to line-height and the top
//     shifts down by half the overflow so the line-box centre stays where
//     Chromium painted it. The ink that escapes the wrapper is fine in
//     practice — siblings position themselves against the line-box edge in
//     the source CSS, so matching the line-box position is exactly what
//     preserves the inter-element spacing.
//
// The per-line stride (top-to-top) is line-height in both branches, so
// consecutive spans tile cleanly without overlap.
function emitTextSpans(textNode, parentRect, computed) {
  const wm = String(computed.getPropertyValue('writing-mode') || '').trim().toLowerCase();
  if (wm === 'vertical-rl' || wm === 'vertical-lr') {
    return emitVerticalTextSpans(textNode, parentRect, computed, wm);
  }
  const ws = computed.getPropertyValue('white-space');
  const lines = splitTextNodeIntoLines(textNode, ws, 'y');
  const lineHeightPx = readNum(computed, 'line-height');
  const out = [];
  for (const line of lines) {
    let lt = line.rect.top;
    let lh = line.rect.height;
    if (lineHeightPx > lh + 0.1) {
      const delta = (lineHeightPx - lh) / 2;
      lt -= delta;
      lh = lineHeightPx;
    } else if (lineHeightPx > 0 && lh > lineHeightPx + 0.1) {
      const delta = (lh - lineHeightPx) / 2;
      lt += delta;
      lh = lineHeightPx;
    }
    const tl = line.rect.left - parentRect.left;
    const tt = lt - parentRect.top;
    const base = buildStyle(tl, tt, line.rect.width, lh, computed, {
      box: false, text: true,
    });
    // Force nowrap on every line span so PAGX's text engine never re-breaks.
    const transformed = applyTextTransform(line.text, computed);
    out.push(`<span style="${withNowrap(base)}">${escapeHtml(transformed)}</span>`);
  }
  return out;
}

// Emit one absolutely-positioned, nowrap <span> per COLUMN of a vertical
// writing-mode (`vertical-rl` / `vertical-lr`) text node — the column-axis
// mirror of `emitTextSpans`'s line loop. The earlier implementation handed the
// whole fragment to PAGX as one un-split column-shaped span and let
// `TextLayout::layoutColumns` re-break it by column height; but PAGX's vertical
// glyph metrics (rotated Latin advances, `line-height: normal` interpretation)
// diverge from Chromium's by sub-pixel amounts, so a column Chromium fit in one
// pass would spill its last glyph into an extra column under PAGX, shifting the
// whole run sideways. Splitting here — exactly as the horizontal path splits by
// line — pins every column to Chromium's break decisions and forces `nowrap` so
// PAGX never re-breaks, isolating the metric divergence entirely.
//
// `line-height` in a vertical writing mode sets each column's WIDTH (the inline
// cross-size). `getClientRects()` reports the column's ink width, which can be
// narrower; expand each column to the line-height width and re-centre it around
// the measured mid-X so consecutive columns tile on the line-height stride —
// the direct analogue of the horizontal line-height re-centring above.
function emitVerticalTextSpans(textNode, parentRect, computed, wm) {
  const axis = wm === 'vertical-lr' ? 'x-lr' : 'x-rl';
  const ws = computed.getPropertyValue('white-space');
  const columns = splitTextNodeIntoLines(textNode, ws, axis);
  const lineHeightPx = readNum(computed, 'line-height');
  const out = [];
  for (const col of columns) {
    let cl = col.rect.left;
    let cw = col.rect.width;
    if (lineHeightPx > cw + 0.1) {
      const delta = (lineHeightPx - cw) / 2;
      cl -= delta;
      cw = lineHeightPx;
    } else if (lineHeightPx > 0 && cw > lineHeightPx + 0.1) {
      const delta = (cw - lineHeightPx) / 2;
      cl += delta;
      cw = lineHeightPx;
    }
    const tl = cl - parentRect.left;
    const tt = col.rect.top - parentRect.top;
    const base = buildStyle(tl, tt, cw, col.rect.height, computed, {
      box: false, text: true,
    });
    const transformed = applyTextTransform(col.text, computed);
    out.push(`<span style="${withNowrap(base)}">${escapeHtml(transformed)}</span>`);
  }
  return out;
}

// ===== Flex container detection =====

// Derives the subset-allowed gap value for a flex container along its main
// axis. CSS 'gap' is a 2-axis property; for `flex-direction: row` we want the
// column-gap (between siblings horizontally), for `column` we want the
// row-gap. Values that can't be parsed back into px are returned as ''.
function flexMainGapPx(computed, direction) {
  const colRaw = computed.getPropertyValue('column-gap');
  const rowRaw = computed.getPropertyValue('row-gap');
  const main = direction === 'column' ? rowRaw : colRaw;
  const v = parseFloat(main);
  if (!isFinite(v) || v <= 0) return '';
  return `${roundPx(v)}px`;
}

// Reads the live flex configuration of `el` and serialises it to a
// subset-shaped CSS string. Only the properties documented in
// spec/html_subset.md §4.1 are emitted — anything outside the allowed value
// set is silently dropped so the importer doesn't have to log warnings for
// normal-default values like `align-items: normal`.
function collectFlexProps(computed, hasMultipleVisibleItems) {
  const direction = computed.getPropertyValue('flex-direction').trim() || 'row';
  const dirOut = direction === 'column' || direction === 'column-reverse' ? 'column' : 'row';

  const align = computed.getPropertyValue('align-items').trim();
  const justify = computed.getPropertyValue('justify-content').trim();

  const pad = readPadding(computed);

  const parts = [];
  parts.push('display: flex');
  parts.push(`flex-direction: ${dirOut}`);
  if (hasMultipleVisibleItems) {
    const gap = flexMainGapPx(computed, dirOut);
    if (gap) parts.push(`gap: ${gap}`);
  }
  if (pad.top > 0 || pad.right > 0 || pad.bottom > 0 || pad.left > 0) {
    parts.push(`padding: ${paddingShorthand(pad.top, pad.right, pad.bottom, pad.left)}`);
  }
  const alignNorm = ALIGN_ITEMS_ALIAS.get(align) || align;
  if (alignNorm && ALIGN_ITEMS_OK.has(alignNorm) && alignNorm !== 'stretch') {
    // `stretch` is the subset's flex default; emitting it adds noise without
    // changing behaviour (and lets the importer's flex-hint validator focus
    // on declarations that actually shift layout).
    parts.push(`align-items: ${alignNorm}`);
  }
  const justifyNorm = JUSTIFY_CONTENT_ALIAS.get(justify) || justify;
  if (justifyNorm && JUSTIFY_CONTENT_OK.has(justifyNorm) && justifyNorm !== 'flex-start') {
    // Same rationale: `flex-start` is the subset default.
    parts.push(`justify-content: ${justifyNorm}`);
  }
  return parts.join('; ');
}

// Returns true when `el` is a pure-text leaf (no element children, has direct
// text) whose rendered text spans more than one line. Such a leaf cannot be
// emitted as a clean flex item: under absolute positioning it would emit one
// <span> per line, but inside a flex item those abs spans can't anchor to the
// wrapper (the subset has no `position: relative`), so they would escape the
// wrapper. The cleaner alternative — collapsing all lines into a single
// <span> — lets PAGX rewrap on its own metrics, which diverges visibly from
// Chromium's wrap point. Bailing at the *parent* flex level keeps the
// subtree's visual layout intact while only sacrificing flex on this one
// container.
function isMultiLineTextLeafItem(el) {
  let textNode = null;
  for (const c of el.childNodes) {
    if (c.nodeType === Node.ELEMENT_NODE) return false;
    if (c.nodeType === Node.TEXT_NODE && c.nodeValue && c.nodeValue.trim()) {
      if (!textNode) textNode = c;
    }
  }
  if (!textNode) return false;
  const ws = getComputedStyle(el).whiteSpace || '';
  const range = document.createRange();
  range.selectNodeContents(textNode);
  const rects = range.getClientRects();
  range.detach && range.detach();
  if (rects.length <= 1) return false;
  // 'pre*' whitespace modes preserve line breaks regardless of container
  // width so multi-line is intentional, not a wrap; still bail since we
  // can't emit multiple inline lines without abs spans.
  return ws !== '' || rects.length > 1;
}

// Returns the visible children of `el` that would render as flex items, or
// null when the element cannot safely be emitted as a flex container under
// the PAGX subset. Each returned entry is either
//   { kind: 'element', node: HTMLElement, computed: CSSStyleDeclaration }
//     — normal flex item (computed cached so render() can reuse it), or
//   { kind: 'text', node: TextNode, rect: DOMRect }
//     — an anonymous flex item that wraps a bare text-node child (the CSS
//     spec wraps these in an anonymous box; PAGX has no concept of anonymous
//     boxes, so we emit an explicit <span> flex item for the text and use
//     its measured rect for sizing).
// Bail (return null) when:
//   - a visible child is `position: absolute|fixed|sticky` (subset bans
//     abs+flex mixing — the importer downgrades flex to absolute on the
//     parent),
//   - a bare text-node child wraps onto multiple lines (we can't express
//     "this single flex item is multi-line text" with one <span>),
//   - a visible child is `display: contents` (its grandchildren would need
//     to be flattened into the flex flow; deferred to a future iteration),
//   - a visible child is a multi-line pure-text leaf (see comment above).
// An empty result is also rejected (no point declaring flex on a leaf).
function flexItemChildren(el) {
  const out = [];
  for (const c of el.childNodes) {
    if (c.nodeType === Node.TEXT_NODE) {
      if (!c.nodeValue || !c.nodeValue.trim()) continue;
      // Bail unless the text fits on a single line: a multi-line anonymous
      // flex item would need a sized wrapper plus per-line spans, which
      // pushes us back into the absolute world (PAGX flex items can't
      // host absolutely-positioned line spans).
      const range = document.createRange();
      range.selectNodeContents(c);
      const rects = range.getClientRects();
      range.detach && range.detach();
      if (rects.length !== 1) return null;
      const r = rects[0];
      if (r.width <= 0 || r.height <= 0) continue;
      out.push({ kind: 'text', node: c, rect: r });
      continue;
    }
    if (c.nodeType !== Node.ELEMENT_NODE) continue;
    const tag = c.tagName.toLowerCase();
    if (DROP_TAGS.has(tag)) continue;
    const cs = getComputedStyle(c);
    if (!isVisible(cs)) continue;
    if (cs.display === 'contents') return null;
    const pos = cs.position;
    if (pos === 'absolute' || pos === 'fixed' || pos === 'sticky') return null;
    if (isMultiLineTextLeafItem(c)) return null;
    // Stash the computed style and rect alongside the node so render() and
    // isFlexLayoutFaithful() can reuse them instead of asking the engine
    // again. The rect is captured in the same layout-pristine state as the
    // text-kind branch above, so callers see consistent measurements
    // regardless of which kind they're processing.
    out.push({ kind: 'element', node: c, computed: cs, rect: c.getBoundingClientRect() });
  }
  return out.length ? out : null;
}

// Verify that re-stacking `children` with the declared `gap` along the main
// axis would land them where the browser actually painted them. If not, the
// live layout depends on something we drop on import — typically per-child
// `margin` (subset has no margin model), `flex` shorthand growing/shrinking,
// or a residual offset from a positioned ancestor — and emitting flex would
// shift the children visibly. Bailing here keeps the absolute positioning
// (which preserves the exact pixels) at the cost of one fewer flex container.
//
// Tolerance is tight (1.5px) to catch real margin/shrink mismatches but
// loose enough to absorb sub-pixel rounding from `getBoundingClientRect`.
function isFlexLayoutFaithful(children, computed) {
  if (children.length < 2) return true;
  const direction = computed.getPropertyValue('flex-direction').trim() || 'row';
  const isRow = !direction.startsWith('column');
  const justify = computed.getPropertyValue('justify-content').trim();
  // space-between / space-around / space-evenly distribute leftover space
  // between items, so the per-pair gap is whatever the engine computes — not
  // the declared `gap`. Trust those layouts on faith: the visible cluster
  // structure is what flex preserves.
  if (justify === 'space-between' || justify === 'space-around' || justify === 'space-evenly') {
    return true;
  }
  const gapRaw = isRow
    ? computed.getPropertyValue('column-gap')
    : computed.getPropertyValue('row-gap');
  const declaredGap = parseFloat(gapRaw) || 0;
  // Both kinds carry a cached rect (text from getClientRects, element from
  // getBoundingClientRect inside flexItemChildren). Reusing the cache avoids
  // a second forced layout flush per element child — for a typical 8-item
  // flex row that's 8 saved sync-layout traversals.
  const TOLERANCE = 1.5;
  for (let i = 0; i + 1 < children.length; i++) {
    const a = children[i].rect;
    const b = children[i + 1].rect;
    const measuredGap = isRow ? (b.left - a.right) : (b.top - a.bottom);
    if (Math.abs(measuredGap - declaredGap) > TOLERANCE) return false;
  }
  return true;
}

// Returns a curated list of flex children when `el`'s computed style declares
// a subset-friendly flex configuration AND its border/wrapping/child shape
// don't require absolute positioning. `null` means "render as absolute".
function classifyFlexContainer(el, computed) {
  const display = computed.display;
  if (display !== 'flex' && display !== 'inline-flex') return null;
  const wrap = computed.getPropertyValue('flex-wrap').trim();
  if (wrap && wrap !== 'nowrap') return null;
  const rect = el.getBoundingClientRect();
  if (!nonZero(rect)) return null;
  // Asymmetric borders are baked into per-side overlay rectangles by
  // borderOverlayHTML(). Those rectangles use `position: absolute` and would
  // not anchor correctly inside a non-positioned flex parent, so bail.
  if (!isUniformBorder(computed) && hasAnyBorder(computed)) return null;
  const children = flexItemChildren(el);
  if (!children) return null;
  if (!isFlexLayoutFaithful(children, computed)) return null;
  return children;
}

// ===== Render functions =====

// SVG icon: emit a wrapper carrying only `color` (so currentColor strokes/
// fills resolve correctly) plus the frozen SVG markup.
//
// The wrapper has explicit width/height pinned to the SVG's measured rect
// and `overflow: hidden`. Without further styling the inner <svg> renders as
// an inline replaced element on a line box whose height is the inherited
// `line-height` (e.g. 21px for `text-[14px]` ancestors). When line-height
// exceeds the wrapper height (typical for 12px status-bar glyphs in a 14px
// text context) the SVG anchors to the line's baseline rather than the
// wrapper rect, which shifts it down and clips the bottom (visible as the
// signal/wifi/battery icons sliding off the 9:41 baseline). Promote the
// wrapper to a centring flex box so the SVG sits at the rect's centre
// regardless of the inherited inline metrics; when the SVG's measured rect
// matches the wrapper rect (the common case) this is a no-op visually, and
// when the SVG is letterboxed inside its wrapper (e.g. an icon shrunk by a
// `max-w-*` ancestor) the centring matches the original layout better than
// the default top-left baseline placement.
function renderSvg(el, parentRect, rect, left, top, computed, opts) {
  const wrapper = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, colorOnly: true, ...opts,
  });
  const innerLayout = 'display: flex; align-items: center; justify-content: center';
  const composed = joinStyles(wrapper, innerLayout);
  return `<div style="${composed}">${freezeSvg(el, rect)}</div>`;
}

// Element tagged by the `inline-icon-fonts` pre-pass (lib/icon-font.js):
// the live page's `::before` icon-font glyph has been replaced with an
// inline `<svg>` carrying the glyph's vector path. The SVG XML is held in
// `window.__pagxIconSvgs` keyed by the host's `data-snapshot-icon-svg-id`
// (stored separately so the actual XML — quotes, angle brackets, future
// metadata — never round-trips through an HTML attribute value); here we
// wrap it in the usual `colorOnly` flex centring box (so the host's
// inherited `color` flows through `currentColor` fills) and pin its width
// / height to a `font-size` square inside the host's measured rect.
//
// Why `font-size`-sized SVG instead of `rect`-sized? Icon webfonts paint
// each glyph as a `font-size × font-size` em-square box, so an icon set
// via `text-[32px]` always renders the glyph at 32×32. The host's actual
// bounding rect can be larger than that — e.g. `<i class="block">` makes
// the host span its parent's full width (330 px in a Tailwind column), or
// a `line-height: 1.5` inflates the height — and stretching the SVG to
// the rect would distort the icon (a flat 330×32 pen-nib instead of a
// 32×32 one positioned at the left edge). Using the font-size square
// preserves the original glyph aspect ratio; the surrounding wrapper
// still matches the host's rect so siblings positioned alongside the
// icon stay correctly anchored.
//
// Anchoring inside the wrapper mirrors the legacy pseudo-text path: the
// host's `text-align` (typically `left` for an `<i>` block) drives
// `justify-content` so an icon in a left-aligned column lands flush with
// its column edge, while a `text-align: center` host (toolbar buttons,
// centred grid icons) keeps the icon centred. `align-items: center`
// vertically centres the glyph in any line-height-induced extra space.
//
// `<i>` / `<span>` icon-font hosts often have explicit `font-size` /
// `line-height` declarations meant to size the glyph. STYLE_SCHEMA's
// `colorOnly` filter already drops `font-size` / `line-height` from the
// wrapper output (they belong to the `text` scope, which we don't enable
// here), so the SVG's pinned pixel dimensions are the sole source of
// truth for the on-screen icon size.
function renderInlineIconSvg(el, parentRect, rect, left, top, computed, opts) {
  const id = el.getAttribute('data-snapshot-icon-svg-id') || '';
  const dict = (typeof window !== 'undefined' && window.__pagxIconSvgs) || null;
  const raw = (id && dict) ? (dict[id] || '') : '';
  if (!raw) return '';
  // The icon's natural rendered size: a `font-size × font-size` square.
  // Clamp to the wrapper's rect so that hosts with sub-font-size boxes
  // (rare: an icon clipped by a parent's `overflow: hidden` plus an
  // explicit `width`/`height` smaller than `font-size`) still fit
  // visibly inside the wrapper. `parseFloat` of the computed `font-size`
  // returns the resolved pixel value regardless of the source unit.
  const fontSize = readNum(computed, 'font-size');
  let iconW = rect.width;
  let iconH = rect.height;
  if (fontSize > 0) {
    iconW = Math.min(rect.width, fontSize);
    iconH = Math.min(rect.height, fontSize);
  }
  // Map host `text-align` to flex `justify-content` so the icon sits
  // where the original glyph rendered. `start`/`left` collapse to
  // `flex-start` (default), `end`/`right` to `flex-end`, `center` to
  // `center`. Other values (`justify`) fall back to flex-start because
  // they have no analogue for a single child.
  let justify = 'flex-start';
  const textAlign = (computed.getPropertyValue('text-align') || 'left').trim().toLowerCase();
  if (textAlign === 'center') justify = 'center';
  else if (textAlign === 'right' || textAlign === 'end') justify = 'flex-end';
  const wrapperStyle = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, colorOnly: true, ...opts,
  });
  const innerLayout = `display: flex; align-items: center; justify-content: ${justify}`;
  const composed = joinStyles(wrapperStyle, innerLayout);
  // Pin width / height onto the inner <svg>. The pre-pass emits a
  // viewBox-only SVG so the size is decided here against the live
  // measurement; strip any author-emitted dimensions before re-injecting
  // ours so the values stay coherent.
  const w = roundPx(iconW);
  const h = roundPx(iconH);
  const sized = raw.replace(/^<svg([^>]*)>/i, (_, attrs) => {
    const stripped = attrs
      .replace(/\s+width="[^"]*"/i, '')
      .replace(/\s+height="[^"]*"/i, '');
    return `<svg${stripped} width="${w}" height="${h}">`;
  });
  return `<div style="${composed}">${sized}</div>`;
}

// Common wrapper used by every "replaced element" branch (`<img>`,
// `<canvas>`, form-input synthetics): outer `<div>` carrying the box
// style + asymmetric border overlays, inner replaced content, and an
// optional extra style block (e.g. flex-centring rules) joined onto the
// outer wrapper. Centralised so the three callers stay in lockstep on
// border/overlay rendering — adding shadow/outline support, for instance,
// only needs touching this helper.
function renderBoxedReplaced(rect, left, top, computed, opts, inner, extraBoxStyle) {
  const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, ...opts,
  });
  const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
  const composed = extraBoxStyle ? joinStyles(boxStyle, extraBoxStyle) : boxStyle;
  return `<div style="${composed}">${inner}${overlays}</div>`;
}

// <img>: wrapper + nested <img> sized to fill it. Asymmetric borders are
// baked into overlay rectangles painted on top.
function renderImg(el, parentRect, rect, left, top, computed, opts) {
  const src = imgSrc(el);
  const alt = escapeHtml(el.getAttribute('alt') || '');
  const objectFit = computed.objectFit || computed.getPropertyValue('object-fit') || '';
  const imgStyle = imageInnerStyle(rect, opts.flexItem, objectFit);
  const inner = `<img src="${escapeHtml(src)}" alt="${alt}" style="${imgStyle}"/>`;
  return renderBoxedReplaced(rect, left, top, computed, opts, inner);
}

// <canvas>: PAGX has no canvas / scripted-graphics primitive, but the live
// bitmap that the page already painted is a perfectly good static texture.
// `inlineCanvases` (the pre-snapshot pass) walks every <canvas> and stashes
// `canvas.toDataURL('image/png')` on the element as `data-snapshot-canvas-
// src`; here we emit it as an <img> with the same outer wrapper / overlay
// geometry as a real <img>. Charts produced by ECharts, Chart.js, D3 +
// Canvas, etc. round-trip through this path without their JS runtime ever
// touching PAGX's renderer.
function renderCanvas(el, parentRect, rect, left, top, computed, opts) {
  const src = el.getAttribute('data-snapshot-canvas-src') || '';
  const objectFit = computed.objectFit || computed.getPropertyValue('object-fit') || '';
  const imgStyle = imageInnerStyle(rect, opts.flexItem, objectFit);
  const inner = `<img src="${escapeHtml(src)}" alt="" style="${imgStyle}"/>`;
  return renderBoxedReplaced(rect, left, top, computed, opts, inner);
}

// <input> / <textarea> / <select>: outer box for bg/border/radius + inner
// span sitting inside the padding box. `<input>` paints its background
// across the full element rect, but the actual text sits inside the padding
// box; emitting them as one rect would smear text over absolute siblings
// (e.g. the search icon at `left: 16px` inside the same wrapper).
//
// Native form controls vertically centre their value/placeholder inside the
// padding box — that's a UA behaviour, not something we can read off
// computed style. A bare abs-positioned inner span at `top: pad.top`
// therefore drops short single-line text against the top of the wrapper
// (search bars in 40px-tall pills, "Email" labels in 48px login fields).
// Emit the wrapper as a flex container with `align-items: center` and the
// input's padding so the inner span rides centred regardless of
// `opts.flexItem` — the box still anchors via `position: absolute` (or
// `relative` when it's itself a flex item), so absolute overlay siblings
// keep their positioning context. `white-space: nowrap` is mandatory so
// PAGX's text engine doesn't re-wrap a single-line value to two lines when
// its intrinsic width exceeds the wrapper by sub-pixel.
function renderTextInput(el, parentRect, rect, left, top, computed, opts) {
  const text = syntheticText(el);
  const pad = readPadding(computed);
  const placeholderColor = (!el.value && el.getAttribute('placeholder'))
    ? getComputedStyle(el, '::placeholder').getPropertyValue('color').trim()
    : '';
  const padShort = paddingShorthand(pad.top, pad.right, pad.bottom, pad.left);
  const textStyle = buildStyle(0, 0, 0, 0, computed, {
    box: false, text: true, positioned: false,
    textColorOverride: placeholderColor || undefined,
  });
  const finalTextStyle = withNowrap(textStyle);
  const innerLayout = `display: flex; align-items: center` + (padShort === '0px' ? '' : `; padding: ${padShort}`);
  const inner = `<span style="${finalTextStyle}">${escapeHtml(applyTextTransform(text, computed))}</span>`;
  return renderBoxedReplaced(rect, left, top, computed, opts, inner, innerLayout);
}

// True when an element child of a flex container can be safely rendered as a
// bare <span> flex item rather than wrapped in a sized <div>. Bare emission
// lets PAGX measure the text's natural width with its own font metrics, so a
// font-fallback-driven width divergence between Chromium (which produced the
// snapshot's measured wrapper rect) and PAGX (which renders the result)
// cannot bleed past the wrapper edge into the gap with the next flex
// sibling — the bug the wrapped form had on tight-fitting price labels like
// "¥4799" inside a 16 px bold Times row, where the Chromium-measured 40 px
// wrapper had no headroom for PAGX's wider rendering and the strikethrough
// ¥5299 sibling visibly overlapped.
//
// Eligibility is conservative: only inline-by-default tags (their CSS box
// has no independent size, so PAGX recovering content-driven sizing matches
// what the source HTML asked for) with no box visuals, padding,
// border-radius, text-clipped background, or inline transform. Anything
// else keeps the wrapped form so the authored layout semantics (button
// padding, inline-flex cluster shape, rounded badge, …) survive the trip
// to PAGX intact.
//
// `computed.display` cannot be consulted directly: CSS Flexbox blockifies
// every flex item, so a `<span>` flex item reports `display: block` even
// though the source CSS leaves it inline. Falling back to the tag list
// captures the same intent (no author-defined size) without false negatives.
// An inline-style `display: block` / `flex` / `grid` override on the host
// is rejected via `el.style.display` so an author who deliberately turned
// the span into a sized box keeps the wrapper.
//
// Author-defined sizing also disqualifies the bare emission. CSS normally
// ignores `width`/`min-width`/`max-width`/`flex-basis` on inline boxes, but
// flex blockifies the item so those declarations *do* take effect on a span
// flex item — and `getComputedStyle(...).width` on a flex item returns the
// resolved used value (e.g. `70px`), not the specified `auto`, so we can't
// detect author intent from the property string alone. Instead we compare
// the element's host rect against the text content's natural rect: when the
// host is materially wider than its glyphs, the extra space came from a
// declared `width` / `min-width` / `flex-basis` and bare emission would
// collapse the column. The classic case is a fixed-width gutter label like
// `.demo-row .label { width: 70px }` next to `.demo-row .controls`: the
// label visually occupies 70 px in Chromium, but stripping the width to a
// bare <span> collapses it to its content width (~18 px for "iOS"), which
// drags every following sibling 50 px to the left.
function isPureInlineTextLeaf(el, computed) {
  if (!el || !el.tagName) return false;
  if (!INLINE_BY_DEFAULT_TAGS.has(el.tagName.toLowerCase())) return false;
  const inlineDisplay = (el.style && el.style.display ? el.style.display : '').trim().toLowerCase();
  if (inlineDisplay && inlineDisplay !== 'inline') return false;
  if (hasBoxVisualsForInline(computed)) return false;
  const pad = readPadding(computed);
  if (pad.top || pad.right || pad.bottom || pad.left) return false;
  const br = (computed.getPropertyValue('border-radius') || '').trim();
  if (br && br !== '0px' && br !== '0px 0px 0px 0px' && br !== '0') return false;
  const bgClip = (computed.getPropertyValue('background-clip') || '').trim().toLowerCase();
  if (bgClip === 'text') return false;
  if (readInlineTransform(el)) return false;
  if (hasAuthorDefinedFlexSize(el)) return false;
  return true;
}

// True when the element's host rect is materially larger than the glyph rect
// of its text content — meaning a `width` / `min-width` / `flex-basis`
// declaration is reserving extra space that the bare-content rendering would
// not reproduce.
//
// Why measure rather than read `getComputedStyle(...).width`: when the
// element is a flex item, Chromium blockifies the box and `width` resolves
// to the used value (e.g. `70px`) regardless of whether the author wrote
// `auto`, `70px`, or `min-content`. Comparing the bounding rect against the
// inner text rect cuts through the blockification and tells us whether the
// author actually expanded the column.
//
// Cross-axis dimensions (height) are intentionally ignored — line-box height
// always matches `font-size`/`line-height` for single-line text, so a
// height comparison would be noise. Only the main-axis (width) divergence
// indicates an author-reserved gutter.
function hasAuthorDefinedFlexSize(el) {
  if (!el || !el.getBoundingClientRect) return false;
  const hostRect = el.getBoundingClientRect();
  if (!hostRect || hostRect.width <= 0) return false;
  const textNode = firstTextNodeChild(el);
  if (!textNode) return false;
  const range = document.createRange();
  range.selectNodeContents(textNode);
  const textRect = range.getBoundingClientRect();
  if (range.detach) range.detach();
  if (!textRect) return false;
  // 1 px slack absorbs subpixel rounding (Chromium reports rect widths to
  // 0.01 px and the `.label` host typically measures 70 px against an 18.5 px
  // text rect, so any author-defined width shows up as a >1 px gap).
  return hostRect.width - textRect.width > 1;
}

// Inner-layout declaration to apply on a text-leaf wrapper that's emitted as
// a flex item. The outer flexItem branch in renderTextLeaf cannot anchor an
// absolutely-positioned line span (the wrapper itself is no longer
// absolutely positioned), so the text becomes an inline <span> child of the
// wrapper. Without help that span hugs the wrapper's top-left corner and
// loses whatever flex centring / padding the source declared. We re-emit the
// source layout here so the wrapper behaves like the original box:
//   - flex / inline-flex sources forward their full subset-shaped flex
//     configuration (direction, padding, align-items, justify-content) so
//     PAGX treats the inner span as a flex item and recentres it,
//   - non-flex sources whose vertical padding is symmetric (the typical
//     case for buttons / badges / pill labels / count text — `px-N` with
//     no `py`, or no padding at all) emit `display: flex; align-items:
//     center` plus the source padding. This re-creates the implicit
//     vertical centring that the user-agent applies to `<button>` form
//     controls (and that line-height centring delivers for inline text
//     inside a box whose height equals the line height) under PAGX's text
//     engine, which otherwise lays the inner <span> flush against the top
//     of the wrapper. `justify-content` is derived from `text-align` so
//     centred labels (the default for `<button>`) stay centred and
//     end/right-aligned labels stay aligned.
//   - non-flex sources with asymmetric vertical padding fall back to
//     padding-only so the source's deliberately off-centre text isn't
//     re-centred (PAGX's flex centring would override the asymmetry).
function textLeafInnerLayout(computed) {
  const display = computed.display;
  if (display === 'flex' || display === 'inline-flex') {
    return collectFlexProps(computed, false);
  }
  const pad = readPadding(computed);
  const hasPad = pad.top > 0 || pad.right > 0 || pad.bottom > 0 || pad.left > 0;
  const symmetricVPad = Math.abs(pad.top - pad.bottom) < 0.5;
  if (symmetricVPad) {
    const parts = ['display: flex', 'align-items: center'];
    const textAlign = (computed.getPropertyValue('text-align') || '').trim();
    if (textAlign === 'center') {
      parts.push('justify-content: center');
    } else if (textAlign === 'right' || textAlign === 'end') {
      parts.push('justify-content: flex-end');
    }
    if (hasPad) {
      parts.push(`padding: ${paddingShorthand(pad.top, pad.right, pad.bottom, pad.left)}`);
    }
    return parts.join('; ');
  }
  if (hasPad) {
    return `padding: ${paddingShorthand(pad.top, pad.right, pad.bottom, pad.left)}`;
  }
  return '';
}

// Tags the PAGX importer accepts as inline runs inside a text leaf
// (see `IsInlineRunTag` in `src/pagx/html/importer/HTMLDetail.h`). Limiting
// the snapshot-side eligibility check to the same set keeps the inline
// reconstruction reversible by the importer: anything emitted here lands
// in `collectTextFragments` and gets merged into a single TextBox with one
// `<Text>` run per style change.
const INLINE_RUN_TAGS = new Set(['span', 'a']);

const INLINE_BY_DEFAULT_TAGS = new Set([
  'span', 'a', 'b', 'i', 'em', 'strong', 'small', 'big', 'sub', 'sup',
  'mark', 'time', 'abbr', 'cite', 'code', 'kbd', 'samp', 'var', 'q', 's',
  'u', 'del', 'ins', 'dfn', 'bdo', 'bdi', 'wbr', 'ruby', 'rt', 'rp',
]);

// True when `cs` declares any property that paints outside the element's
// text — background, border, shadow, outline, filter, clip. Inline-text-leaf
// reconstruction rejects child elements that carry box visuals because
// PAGX has no per-inline-run box-paint model: the visuals would be
// silently dropped when the run is merged into a `<Text>` element.
//
// Opacity is intentionally NOT rejected: an inline run's opacity has no
// observable effect outside its own text glyphs (the inline box paints no
// background once we've ruled out the rest), so it can be folded into the
// run's text colour alpha during emission — see `multiplyColorAlpha`.
function hasBoxVisualsForInline(cs) {
  const bg = cs.getPropertyValue('background-color').trim();
  if (bg && bg !== 'rgba(0, 0, 0, 0)' && bg !== 'transparent') return true;
  const bgImg = cs.getPropertyValue('background-image').trim();
  if (bgImg && bgImg !== 'none') return true;
  if (borderWidthOf(cs, 'top') > 0) return true;
  if (borderWidthOf(cs, 'right') > 0) return true;
  if (borderWidthOf(cs, 'bottom') > 0) return true;
  if (borderWidthOf(cs, 'left') > 0) return true;
  const shadow = cs.getPropertyValue('box-shadow').trim();
  if (shadow && shadow !== 'none') return true;
  const outline = cs.getPropertyValue('outline-style').trim();
  if (outline && outline !== 'none') return true;
  const filter = cs.getPropertyValue('filter').trim();
  if (filter && filter !== 'none') return true;
  return false;
}

// Multiply the alpha channel of an `rgb()` / `rgba()` colour string by
// `factor` (rounded to 3 fractional digits, trailing zeros trimmed).
// Used to fold an inline run's `opacity` into the emitted `color` so the
// snapshot collapses to a single text-leaf without losing the visual
// composite. Returns the input string unchanged when the format is not
// the standard rgb/rgba serialization (e.g. `transparent`, `currentcolor`,
// or future colour spaces).
function multiplyColorAlpha(colorStr, factor) {
  if (!colorStr) return colorStr;
  if (factor === 1) return colorStr;
  const m = colorStr.trim().match(
      /^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+)\s*)?\)$/);
  if (!m) return colorStr;
  const r = parseInt(m[1], 10);
  const g = parseInt(m[2], 10);
  const b = parseInt(m[3], 10);
  const a = m[4] != null ? parseFloat(m[4]) : 1;
  const newA = Math.max(0, Math.min(1, a * factor));
  const aStr = newA.toFixed(3).replace(/\.?0+$/, '') || '0';
  return `rgba(${r}, ${g}, ${b}, ${aStr})`;
}

// Recursive check: returns true when `el` is a plain inline run whose
// subtree contains only text + further plain inline runs. "Plain" here
// means: tag in INLINE_RUN_TAGS, `display: inline*`, statically positioned,
// no transform, no box visuals, no padding/margin. The eligibility is
// strict by design — anything weirder than `<span>%</span>` or a nested
// styling tag falls back to the existing absolute-positioned container
// path and renders the same as before.
function isInlineRunChild(el) {
  if (el.nodeType !== Node.ELEMENT_NODE) return false;
  const tag = el.tagName.toLowerCase();
  if (!INLINE_RUN_TAGS.has(tag)) return false;
  const cs = getComputedStyle(el);
  if (!isVisible(cs)) return false;
  const display = cs.display;
  if (display !== 'inline' && display !== 'inline-block') return false;
  if (cs.position !== 'static') return false;
  const t = cs.transform;
  if (t && t !== 'none' && t !== 'matrix(1, 0, 0, 1, 0, 0)') return false;
  if (hasBoxVisualsForInline(cs)) return false;
  const pad = readPadding(cs);
  if (pad.top || pad.right || pad.bottom || pad.left) return false;
  // A non-zero margin shifts the run's painted position, but the inline-run
  // emit path (`emitInlineRunMarkup` → `STYLE_SCHEMA_TEXT`) only forwards
  // text-scope properties (color, font-size, letter-spacing, line-height,
  // text-decoration, white-space, writing-mode) and the PAGX HTML subset
  // intentionally drops `margin*` (see HTMLSubsetPropertyTable: "use
  // padding/gap/flex instead"). Inline-merging a margin-bearing run would
  // therefore silently glue it to its sibling. Falling back to the
  // absolute-positioned container path lets the browser-measured `left`
  // encode the margin offset directly, so PAGX renders the gap correctly
  // without needing a margin notion of its own.
  const margin = readMargin(cs);
  if (margin.top || margin.right || margin.bottom || margin.left) return false;
  for (const c of el.childNodes) {
    if (c.nodeType === Node.TEXT_NODE) continue;
    if (c.nodeType !== Node.ELEMENT_NODE) return false;
    if (!isInlineRunChild(c)) return false;
  }
  return true;
}

// Detect the `<p>NN<span class="text-[28px]">unit</span></p>` family: a
// container whose visible content is one single line of inline text mixed
// with inline-styled spans (different font-size / color / weight). The
// default snapshot path would flatten each child into its own absolutely
// positioned sibling, hard-coding the `left` of every run to the browser's
// measured advance width. PAGX then renders each run with its own (possibly
// fallback) font metrics, and any width difference compounds into a
// visible overlap / gap between adjacent runs.
//
// When this predicate fires, `renderInlineTextLeaf` instead emits a single
// text-leaf `<span>` wrapper preserving the inline structure verbatim
// (`<span>NN<span style="font-size:28px">unit</span></span>`). The PAGX
// importer recognises this as a multi-fragment text leaf
// (`HTMLParserContext::collectTextFragments`) and builds one TextBox with
// one `<Text>` run per style change. PAGX's own text engine then computes
// the inter-run advance with whatever font it actually has loaded, so the
// runs always touch correctly regardless of font fallback.
function isInlineTextLeafCandidate(el, computed, precomputedHasChildren) {
  const hasChildren = precomputedHasChildren !== undefined
    ? precomputedHasChildren
    : elementHasChildren(el);
  if (!hasChildren) return false;
  // Box-shape constraints: a wrapper with overflow:hidden / outline / shadow
  // can still be emitted (those live on the wrapper itself and PAGX honours
  // them on the Layer); we only reject what would conflict with delegating
  // text layout to PAGX.
  const wm = String(computed.getPropertyValue('writing-mode') || '').trim().toLowerCase();
  if (wm === 'vertical-rl' || wm === 'vertical-lr') return false;
  const display = computed.display;
  // Flex / grid containers route through the dedicated flex path before this
  // check is reached; reject anything else block-display that we'd be
  // collapsing visible layout for.
  if (display !== 'block' && display !== 'inline' && display !== 'inline-block') return false;
  // All children must be either bare text or further plain inline runs.
  // A single `<br>` (or any disallowed tag) bails out to the absolute path.
  let sawElementChild = false;
  for (const c of el.childNodes) {
    if (c.nodeType === Node.TEXT_NODE) continue;
    if (c.nodeType !== Node.ELEMENT_NODE) return false;
    if (!isInlineRunChild(c)) return false;
    sawElementChild = true;
  }
  if (!sawElementChild) return false;
  // Reject elements whose own padding would shift the inline content: the
  // text-leaf wrapper writes its `width/height` as the bounding rect but
  // PAGX's TextBox renders text from the content-box origin, so padding
  // would visibly move the baseline. Symmetric handling on the wrapper is
  // future work; for now bail.
  const pad = readPadding(computed);
  if (pad.top || pad.right || pad.bottom || pad.left) return false;
  // Must produce a single visual line of inline content.
  //
  // `range.getClientRects()` reports glyph INK bounds, which systematically
  // under-report the line-box height for scripts whose typical glyph ink
  // is shorter than the chosen leading — most notably CJK at 14px / 22.75px
  // line-height, where two wrapped lines union to ~36 px (well under any
  // reasonable `N × line-height` threshold) while the actual border-box is
  // 45.5 px. Slipping past the threshold here would route a multi-line
  // wrapper into `renderInlineTextLeaf`, which then emits a wrapper carrying
  // the layout-truth height plus a forced `white-space: nowrap`; the PAGX
  // importer reads the nowrap and sets `wordWrap="false"`, crushing the
  // wrapped text back onto a single line that overflows its column.
  //
  // Use the element's border-box height as the layout truth. Padding is
  // already pinned to 0 above (`readPadding` reject); subtract the
  // top/bottom border widths so the remaining value is the rendered
  // content/line-box height regardless of script-specific ink quirks.
  // `getClientRects()` is still consulted for the empty-content early-out
  // — an element whose range has no rects is invisible and shouldn't be
  // emitted as an inline text leaf.
  const range = document.createRange();
  range.selectNodeContents(el);
  const rects = Array.from(range.getClientRects()).filter((r) => r.width > 0 && r.height > 0);
  range.detach && range.detach();
  if (rects.length === 0) return false;
  let lineHeightPx = parseFloat(computed.getPropertyValue('line-height'));
  if (!isFinite(lineHeightPx) || lineHeightPx <= 0) {
    const fs = readNum(computed, 'font-size');
    lineHeightPx = fs * 1.2;
  }
  if (lineHeightPx <= 0) return false;
  const borderTop = readBorderSide(computed, 'top').width;
  const borderBottom = readBorderSide(computed, 'bottom').width;
  const layoutHeight = Math.max(0, el.getBoundingClientRect().height - borderTop - borderBottom);
  // Tight `1.5 ×` ratio: the border-box height never overshoots the line
  // box (unlike ink), so the historical `1.8 ×` headroom for tall ink is
  // no longer needed. The remaining slack absorbs one slightly-taller
  // inline-block child — e.g. an emoji rendered at 1.2 em, or a `<sup>`
  // whose own line-height bumps the host's used line-box — without
  // misclassifying a single-line element as wrapped.
  if (layoutHeight > lineHeightPx * 1.5) return false;
  return true;
}

// Resolve the text-scope value for `entry` on `cs`, folding the element's
// own (non-inherited) `opacity` into any colour declaration so the inline
// run can be flattened into a TextBox `<Text>` without losing the visual
// composite. The opacity multiplier is applied here — not at parent
// comparison time — because the wrapper's own opacity is separately
// carried by the box-scope path on the text-leaf wrapper, so an inline
// child inherits a fully opaque rendering context.
function resolveInlineTextValue(cs, entry, opacityFactor) {
  const raw = cs.getPropertyValue(entry.prop).trim();
  const v = entry.normalize ? entry.normalize(raw) : raw;
  if (!v) return v;
  if (opacityFactor !== 1 && (entry.prop === 'color' || entry.prop === 'text-decoration-color')) {
    return multiplyColorAlpha(v, opacityFactor);
  }
  return v;
}

// Emit the inline-run subtree of `el` as a string, preserving text nodes
// verbatim and wrapping each inline-element child in a `<span>` that
// carries only the text-scope style declarations that actually differ
// from the wrapper. The PAGX importer's `appendTextFragment` deduplicates
// styles whose computed value matches the inherited one, but emitting the
// minimum delta keeps the snapshot HTML human-readable and bounded in
// size.
function emitInlineRunMarkup(el, parentComputed) {
  let out = '';
  for (const c of el.childNodes) {
    if (c.nodeType === Node.TEXT_NODE) {
      const t = c.nodeValue;
      if (!t) continue;
      out += escapeHtml(applyTextTransform(t, parentComputed));
      continue;
    }
    if (c.nodeType !== Node.ELEMENT_NODE) continue;
    const cs = getComputedStyle(c);
    const opacityRaw = parseFloat(cs.getPropertyValue('opacity'));
    const opacityFactor = isFinite(opacityRaw) ? opacityRaw : 1;
    const deltaParts = [];
    for (const entry of STYLE_SCHEMA_TEXT) {
      const v = resolveInlineTextValue(cs, entry, opacityFactor);
      if (!v) continue;
      const parentV = resolveInlineTextValue(parentComputed, entry, 1);
      if (v === parentV) continue;
      const outProp = entry.outProp || entry.prop;
      deltaParts.push(`${outProp}: ${v}`);
    }
    // `-webkit-text-stroke` is not in STYLE_SCHEMA_TEXT (it coalesces two
    // longhands rather than mapping one property), so emit it as an explicit
    // delta: forward it only when this run's resolved stroke differs from the
    // wrapper's, matching the minimal-delta strategy used for the schema props.
    const stroke = resolveTextStroke(cs);
    if (stroke && stroke !== resolveTextStroke(parentComputed)) {
      deltaParts.push(`-webkit-text-stroke: ${stroke}`);
    }
    const inner = emitInlineRunMarkup(c, cs);
    if (deltaParts.length === 0) {
      out += inner;
    } else {
      out += `<span style="${deltaParts.join('; ')}">${inner}</span>`;
    }
  }
  return out;
}

// Emit `el` as a single text-leaf wrapper containing inline runs.
// See `isInlineTextLeafCandidate` for the eligibility rationale.
function renderInlineTextLeaf(el, parentRect, rect, left, top, computed, opts) {
  const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, text: true, ...opts,
  });
  const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
  // Force `white-space: nowrap` on the wrapper. The geometry above was
  // taken from a single-line `Range.getClientRects()` measurement, and
  // PAGX's text engine — whose intrinsic glyph metrics differ from
  // Chromium's by a sub-pixel — would otherwise risk introducing a
  // phantom wrap when the natural advance crowds the wrapper width.
  const style = withNowrap(boxStyle);
  const inner = emitInlineRunMarkup(el, computed);
  return `<span style="${style}">${inner}${overlays}</span>`;
}

// Pure-text leaf (no element children): emit one outer <div> carrying the
// box styles plus one nowrap <span> per visually-rendered line. Each line
// span is placed at the text's *actual* rendered rect (from
// Range.getClientRects), not stretched to fill the parent box — otherwise
// the text inherits the parent height and clings to the top of the
// container instead of sitting where the browser's flex/baseline layout
// placed it (e.g. <button>搜索</button> with `inline-flex items-center`:
// button is 56×28, text is 24×16 centered at y=6). The wrapper carries
// background/border/radius/overflow so the visible box still spans the full
// element rect.
function renderTextLeaf(el, parentRect, rect, left, top, computed, directText, opts) {
  const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, ...opts,
  });
  const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');

  // Inside a flex parent the wrapper drops `position: absolute`, so per-line
  // abs spans would lose their anchor. classifyFlexContainer guarantees
  // single-line leaves only at this point (multi-line leaves cause the
  // parent to bail to absolute). Force `white-space: nowrap` so PAGX's text
  // engine doesn't introduce a phantom wrap when its intrinsic glyph
  // metrics differ from Chromium's by a sub-pixel — the measured wrapper
  // rect already matches Chromium's natural line width.
  if (opts.flexItem) {
    const baseTextStyle = buildStyle(0, 0, 0, 0, computed, {
      box: false, text: true, positioned: false,
    });
    const textStyle = withNowrap(baseTextStyle);
    // Pure-inline text leaf (no box visuals, no padding, no rounding, no
    // background-clip:text, no inline transform) ⇒ emit a bare <span> flex
    // item so PAGX measures the text natural width with its own font
    // metrics. Encoding Chromium's measured width into a `width: Npx`
    // wrapper otherwise overflows when PAGX's font fallback renders wider
    // than Chromium's Times metrics — a tight 40 px "¥4799" wrapper has no
    // headroom and visibly overlaps the next strikethrough ¥5299 sibling.
    // The flex container's `gap` and `align-items` continue to drive
    // sibling spacing because PAGX takes the layer's intrinsic content
    // width as its main-axis size when no explicit width is authored.
    const isPure = isPureInlineTextLeaf(el, computed);
    if (isPure) {
      // Pure-inline branch sidesteps `buildStyle`'s flexItem header (no
      // `position: relative`, no `width/height`), so the parent loop's
      // `flex-grow` forwarding doesn't reach this <span>. Read it again
      // here and append `flex: <grow>` directly. This catches the common
      // `.sym { flex: 1 }` shape on text-only flex items (a label that
      // pushes its siblings to the row's far edge); without it, PAGX
      // would see a bare-width <span> and pack everything to the start.
      // `flex-shrink: 0` is dropped when grow is active — `flex: <N>`
      // already implies `1 1 0%`, and stacking `flex-shrink: 0` after
      // would re-pin the item to its content width on the main axis.
      //
      // A main-axis `max-width` / `max-height` on the span caps the grow
      // in the source layout, but PAGX's HTML subset drops those caps
      // (HTMLSubsetPropertyTable.cpp), so forwarding `flex: <grow>` here
      // would let PAGX hand the span the full leftover space. Mirror the
      // capped-axis check from buildStyle and fall back to `flex-shrink:
      // 0` so the measured intrinsic size pins the layout instead.
      const grow = readNum(computed, 'flex-grow');
      const parts = [textStyle];
      const parentMainAxis = opts.flexMainAxis === 'column' ? 'column' : 'row';
      const inlineMaxProp = parentMainAxis === 'column' ? 'max-height' : 'max-width';
      const inlineMax = (computed.getPropertyValue(inlineMaxProp) || '').trim().toLowerCase();
      const inlineCapped = inlineMax !== '' && inlineMax !== 'none';
      if (grow > 0 && !inlineCapped) {
        parts.push(`flex: ${formatFlexGrow(grow)}`);
      } else {
        parts.push('flex-shrink: 0');
      }
      return `<span style="${parts.filter(Boolean).join('; ')}">${escapeHtml(applyTextTransform(directText, computed))}</span>`;
    }
    // Mirror the source's own flex / padding declaration onto the wrapper
    // so the inline span lands where the browser painted it. A text-only
    // flex container — e.g. `<button class="inline-flex items-center
    // justify-center h-7 px-3">+ 关注</button>` or `<span class="inline-flex
    // items-center px-2 py-0.5">实名认证</span>` — measures its bounding
    // rect with the centred padding included, but emitting just
    // `width/height` on the wrapper would push the inline span to the
    // wrapper's top-left corner (the visible "+ 关注" / "实名认证" badge
    // misalignment). Re-emitting flex props makes the inner <span> a flex
    // item that PAGX's flex engine re-centres on import. For non-flex
    // wrappers we still want the source padding so the inline span starts
    // inside the padding box.
    const innerLayout = textLeafInnerLayout(computed);
    const wrapperStyle = joinStyles(boxStyle, innerLayout);
    return `<div style="${wrapperStyle}"><span style="${textStyle}">${escapeHtml(applyTextTransform(directText, computed))}</span>${overlays}</div>`;
  }

  // Inline `transform` on the host (e.g. `<h1 style="transform: skewX(-5deg)">`)
  // re-shapes its bounding rect — `getBoundingClientRect()` and the inner
  // `Range.getClientRects()` both return post-transform geometry — but the
  // PAGX subset has no `transform` property on Layer / generic boxes. When the
  // text leaf carries a transform we therefore measure under
  // `withStrippedTransform` so the wrapper / inner spans capture the
  // unrotated layout, then attach the original transform string to the inner
  // <span> (the actual textLeaf the importer recognises) so PAGX maps the
  // function back onto the TextBox's own skew/rotation/scale fields. Putting
  // the declaration on the outer wrapper would have left the importer's
  // textLeaf path unaware of it (the wrapper is treated as a container, and
  // PAGX Layer has no transform fields). Multi-line transforms degrade
  // gracefully: the per-line spans would each rotate around their own
  // centre instead of the host's, so we drop the transform with a warning.
  const inlineTransform = readInlineTransform(el);
  if (inlineTransform) {
    let preRect = rect;
    let preSpans = [];
    withStrippedTransform(el, () => {
      preRect = el.getBoundingClientRect();
      const tn = firstTextNodeChild(el);
      preSpans = tn ? emitTextSpans(tn, paddingBoxOrigin(preRect, computed), computed) : [];
    });
    const preLeft = preRect.left - parentRect.left;
    const preTop = preRect.top - parentRect.top;
    const preBoxStyle = buildStyle(preLeft, preTop, preRect.width, preRect.height, computed, {
      box: true, ...opts,
    });
    const preOverlays = borderOverlayHTML(computed, preRect.width, preRect.height).join('');
    if (preSpans.length === 1) {
      let transformDecl = `transform: ${inlineTransform.transform}`;
      if (inlineTransform.transformOrigin) {
        transformDecl += `; transform-origin: ${inlineTransform.transformOrigin}`;
      }
      // Inject the transform into the single line span. emitTextSpans returns a fully
      // formed `<span style="...">text</span>`; insert the declaration before the closing
      // quote so the existing styles are preserved verbatim (joinStyles can't be reused
      // here without re-parsing the span markup).
      const spanWithTransform = preSpans[0].replace(
        /style="([^"]*)"/,
        (_, s) => `style="${s.replace(/;\s*$/, '')}; ${transformDecl}"`);
      return `<div style="${preBoxStyle}">${spanWithTransform}${preOverlays}</div>`;
    }
    // Multi-line: emitting per-line spans each carrying the same transform would
    // pivot every line around its own centre, not the host's. PAGX has no model
    // for that today; degrade by dropping the transform with a diagnostic-friendly
    // attribute the importer will warn about.
    return `<div style="${preBoxStyle}" data-pagx-transform-dropped="${escapeHtml(inlineTransform.transform)}">${preSpans.join('')}${preOverlays}</div>`;
  }

  const textNode = firstTextNodeChild(el);
  const lineSpans = textNode
    ? emitTextSpans(textNode, paddingBoxOrigin(rect, computed), computed)
    : [];
  return `<div style="${boxStyle}">${lineSpans.join('')}${overlays}</div>`;
}

// Emit an element whose only visible content comes from its ::before /
// ::after pseudo-element(s) — typically an icon-font glyph such as
// `<i class="ph ph-pen-nib"></i>`. The host has no DOM children and no
// direct text node, so neither the text-leaf nor the container path
// produces any visible output today; this renderer mirrors them by
// wrapping the host as a flex box and emitting one inner <span> per
// non-empty pseudo, styled with the pseudo's own computed font / color
// (so icon-font CSS that sets `font-family: 'Phosphor'` only on the
// pseudo still resolves correctly even when the host itself inherits a
// different font from the body cascade).
//
// Pseudo nodes have no DOM presence, so `Range.getClientRects` can't
// measure them; we centre the inner spans inside the host rect (vertical
// centring is the right default for inline-level pseudos riding on the
// baseline of a fixed-height box; horizontal alignment is derived from
// the host's `text-align` so left-aligned block icons stay against the
// left edge of their parent column).
function renderPseudoTextLeaf(el, parentRect, rect, left, top, hostComputed, opts) {
  const boxStyle = buildStyle(left, top, rect.width, rect.height, hostComputed, {
    box: true, ...opts,
  });
  const overlays = borderOverlayHTML(hostComputed, rect.width, rect.height).join('');

  const spans = [];
  for (const pseudo of ['::before', '::after']) {
    const text = pseudoText(el, pseudo);
    if (!text) continue;
    const pseudoComputed = getComputedStyle(el, pseudo);
    const textStyle = withNowrap(buildStyle(0, 0, 0, 0, pseudoComputed, {
      box: false, text: true, positioned: false,
    }));
    spans.push(`<span style="${textStyle}">${escapeHtml(applyTextTransform(text, pseudoComputed))}</span>`);
  }
  if (spans.length === 0) {
    return `<div style="${boxStyle}">${overlays}</div>`;
  }

  let innerLayout;
  if (opts.flexItem) {
    innerLayout = textLeafInnerLayout(hostComputed);
  } else {
    const textAlign = (hostComputed.getPropertyValue('text-align') || 'left').trim();
    let justify = 'flex-start';
    if (textAlign === 'center') justify = 'center';
    else if (textAlign === 'right' || textAlign === 'end') justify = 'flex-end';
    innerLayout = `display: flex; align-items: center; justify-content: ${justify}`;
  }
  const composed = joinStyles(boxStyle, innerLayout);
  return `<div style="${composed}">${spans.join('')}${overlays}</div>`;
}

// Emit a bare text-node child of a flex container as a sized <span> flex
// item. The text inherits color/font-* from the container's computed style;
// we forward the inherited text props plus an explicit height (so the flex
// engine's cross-axis centring lands on the line-box), but deliberately
// omit `width` so PAGX measures the glyph natural width with its own font
// metrics. Encoding Chromium's `Range.getClientRects` width into the span
// otherwise overflows when PAGX's font fallback renders wider, eating the
// gap to adjacent siblings (the same divergence that motivates
// `isPureInlineTextLeaf` for element-kind children — anonymous text-nodes
// hit the bug too because their measured rect is also Chromium-driven).
// `withNowrap` is mandatory: the measured rect was for a single line and
// PAGX's text engine must not rewrap at a sub-pixel divergence.
//
// Height is expanded to `line-height` when Chromium's `getClientRects`
// reports a shorter glyph-bounds rect than the inherited line-height (e.g.
// "首页" at font-size 16 / line-height 24 measures ~19px tall). Without the
// expansion the flex container's `align-items: center` would centre the
// 19px span box, but the inline text inside still renders against a 24px
// line-box anchored at the span's top — pushing the visible glyph ~2.5px
// below the icon sibling that was centred against its own box. Setting
// height = line-height aligns the span's outer box with the line-box that
// actually drives glyph placement, mirroring how the source HTML's
// anonymous text flex item participates in centring. See `emitTextSpans`
// for the same expansion applied along the absolute-positioning path.
function renderFlexTextItem(child, parentComputed) {
  const r = child.rect;
  const text = (child.node.nodeValue || '').replace(/\s+/g, ' ').trim();
  const lineHeightPx = readNum(parentComputed, 'line-height');
  const height = lineHeightPx > r.height + 0.1 ? lineHeightPx : r.height;
  const baseStyle = buildStyle(0, 0, 0, 0, parentComputed, {
    box: false, text: true, positioned: false,
  });
  const flexBits = `height: ${px(height)}; flex-shrink: 0`;
  const style = withNowrap([baseStyle, flexBits].filter(Boolean).join('; '));
  return `<span style="${style}">${escapeHtml(applyTextTransform(text, parentComputed))}</span>`;
}

// Render an element + its subtree as a flex container. The container itself
// is either positioned absolutely against `parentRect` (default) or sized as
// a flex item against an outer flex parent (when `opts.flexItem` is true).
// Element children recurse through `render` with `flexItem: true`; anonymous
// text-node children are wrapped in a sized `<span>` flex item — the text
// inherits the container's `color`/`font-*` cascade, so the wrapper only
// needs the explicit dimensions to participate in flex layout (PAGX's flex
// engine then aligns it against its siblings via the container's
// `align-items` setting).
function renderFlexContainer(el, parentRect, computed, flexChildren, opts) {
  const rect = el.getBoundingClientRect();
  const left = rect.left - parentRect.left;
  const top = rect.top - parentRect.top;
  const wrapperBase = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, ...opts,
  });
  const flexStyle = collectFlexProps(computed, flexChildren.length > 1);
  const style = joinStyles(wrapperBase, flexStyle);
  // Resolve the parent's main axis once and forward it to every child so
  // `buildStyle`'s flex-grow branch can drop the right (main-axis) dimension
  // when a child's `flex-grow > 0`. Reading `flex-direction` here keeps the
  // child path purely state-less.
  const direction = computed.getPropertyValue('flex-direction').trim() || 'row';
  const mainAxis = direction === 'column' || direction === 'column-reverse' ? 'column' : 'row';
  const childParts = [];
  for (const child of flexChildren) {
    if (child.kind === 'text') {
      childParts.push(renderFlexTextItem(child, computed));
    } else {
      // Pass the cached computed style so render() does not have to ask
      // the engine again for the same node.
      childParts.push(render(child.node, rect, { flexItem: true, flexMainAxis: mainAxis }, child.computed));
    }
  }
  return `<div style="${style}">${childParts.join('')}</div>`;
}

// Container with element children (and optionally direct text). Per direct
// text node we measure the actual rendered rect via Range, so the text
// lands where the browser put it (next to an <svg>, inside a flex row, …)
// rather than being smeared across the whole parent and inheriting the
// parent's text-align.
//
// Paint order: browsers paint static-flow descendants first, then
// stackable descendants on top — positioned children, plus flex/grid
// items whose z-index participates in stacking even at `position:
// static` (CSS Flexbox / Grid rule). Since we flatten every box to
// `position: absolute`, we must replay that order ourselves: flow
// children + direct text runs in DOM order, then stackable children
// sorted by z-index (DOM order as tie-break). Otherwise an `<input
// bg-gray>` declared after an `<svg>` would paint over the icon, an
// inline-flex bg-pill <button>'s positioned label would render under
// the bg, a `.center-node { z-index: 20 }` flex item would sort
// underneath its `.satellite-card { z-index: 10 }` siblings, etc.
// Asymmetric border overlays paint *on top* of all children to match
// CSS, which renders borders after content.
function renderContainer(el, parentRect, rect, left, top, computed, opts) {
  const childHTML = renderChildrenInto(el, paddingBoxOrigin(rect, computed), computed);
  const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
  const style = buildStyle(left, top, rect.width, rect.height, computed, {
    box: true, ...opts,
  });
  return `<div style="${style}">${childHTML}${overlays}</div>`;
}

// Emit every visible child of `el` (elements + non-empty text nodes) into a
// single HTML string, replaying browser paint order (flow-then-positioned,
// both in DOM order). The host's computed style is passed in so we don't
// have to ask the engine for it again.
function renderChildrenInto(el, parentRect, hostComputed) {
  const items = [];
  let domIndex = 0;
  const computedFor = hostComputed || getComputedStyle(el);
  // CSS Flexbox / Grid both let an item paint by its own `z-index`
  // regardless of `position`. Detect that on the parent so a
  // `position: static` flex/grid child with a non-default z-index
  // still participates in z-index sorting alongside its positioned
  // siblings, matching what the browser actually draws.
  const parentDisplay = computedFor.display;
  const flexOrGridParent =
    parentDisplay === 'flex' ||
    parentDisplay === 'inline-flex' ||
    parentDisplay === 'grid' ||
    parentDisplay === 'inline-grid';
  for (const n of el.childNodes) {
    if (n.nodeType === Node.ELEMENT_NODE) {
      const childComputed = getComputedStyle(n);
      const isPositioned = childComputed.position !== 'static';
      const isStackable = isPositioned || flexOrGridParent;
      items.push({
        stackable: isStackable,
        zIndex: isStackable ? resolveZIndex(childComputed) : 0,
        domIndex: domIndex++,
        // Forward childComputed so render() can skip its own
        // getComputedStyle call on this node.
        html: render(n, parentRect, undefined, childComputed),
      });
    } else if (n.nodeType === Node.TEXT_NODE && n.nodeValue && n.nodeValue.trim()) {
      const spans = emitTextSpans(n, parentRect, computedFor);
      if (spans.length > 0) {
        items.push({
          stackable: false,
          zIndex: 0,
          domIndex: domIndex++,
          html: spans.join(''),
        });
      }
    }
  }
  items.sort(paintOrder);
  return items.map((it) => it.html).join('');
}

// Render a subtree rooted at `el` into a string. parentRect is its parent's
// bounding rect (used to compute relative left/top). `precomputed`, when
// supplied, is the result of a prior `getComputedStyle(el)` call upstream;
// reusing it avoids a redundant style query on every visited element.
function render(el, parentRect, opts, precomputed) {
  opts = opts || {};
  const computed = precomputed || getComputedStyle(el);

  // `display: contents` makes the element generate no box, but its children
  // still participate in the parent's layout. The element's own
  // `getBoundingClientRect()` is 0×0 at its DOM-tree position, which would
  // break every descendant's relative offset if we used it as their parent.
  // Instead, render the children directly into `parentRect` (the element's
  // CSS parent's rect) and drop the host entirely.
  if (computed.display === 'contents') {
    return renderChildrenInto(el, parentRect, computed);
  }

  // Generic CSS `transform` preservation. Any non-identity computed
  // transform (rotate/skew/scale/translate, or any `matrix(...)` form
  // including the one Tailwind synthesises from its utility classes) must
  // be forwarded to the wrapper this element emits — otherwise a
  // `<div class="rotate-45">` lays down as an axis-aligned square instead
  // of the diamond Chromium painted.
  //
  // Strategy: temporarily clear the inline `transform` slot (which
  // out-specifies any class-level rule) so all downstream measurements —
  // this element's `getBoundingClientRect()`, every descendant's rect,
  // every text-line `Range.getClientRects()` — see the unrotated layout.
  // The dispatched renderer then emits an absolutely-positioned wrapper
  // sized to the unrotated box, with the original computed transform
  // string injected back via `buildStyle(opts.transform)`. Reopening the
  // snapshot in a browser reproduces the same visual transform, and the
  // PAGX importer reads the `matrix(...)` form into `Layer.matrix` so
  // `pagx render` agrees pixel-for-pixel.
  //
  // Skipped for `_strippedTransform` re-entries (the recursive call below)
  // and for the CSS-triangle path (`renderBorderTriangle` already bakes
  // the transform into its polygon coordinates) — handled by re-checking
  // the flag rather than guarding here so the shared flow stays simple.
  if (!opts._strippedTransform) {
    const boxTransform = readBoxTransform(computed);
    if (boxTransform) {
      return withStrippedTransform(el, () => render(el, parentRect, {
        ...opts, _strippedTransform: true, transform: boxTransform,
      }, getComputedStyle(el)));
    }
  }

  // Direct flex preservation: when the live computed style says this
  // element is a flex container and its children can all participate as
  // flex items, emit it as `display: flex` rather than baking everything
  // into `position: absolute`. This keeps the author's flex-direction/gap/
  // padding/align-items/justify-content verbatim instead of forcing the
  // C++ AbsoluteToFlexInferencePass to re-derive them from box geometry.
  // Layouts that don't qualify (mixed abs+flex children, flex-wrap,
  // asymmetric borders, stray text, display:contents middlemen) fall
  // through to the absolute path below — the inference pass remains a
  // backstop.
  const flexChildren = classifyFlexContainer(el, computed);
  if (flexChildren) {
    return renderFlexContainer(el, parentRect, computed, flexChildren, opts);
  }

  const kind = classify(el, computed);
  if (!kind) return '';

  const rect = el.getBoundingClientRect();
  // An empty leaf with a degenerate rect would render as nothing, so skip
  // it. Flex items are always preserved here (even when both axes collapse
  // to zero) because they still count as siblings for the parent's `gap`
  // accounting and for `justify-content` distribution. Two concrete cases:
  //   - `<div style="flex: 1"></div>` under `align-items: center` reports a
  //     non-zero main-axis size but `height: 0` on the cross axis — keep it
  //     so it can keep pushing its siblings along the main axis.
  //   - An empty `<i>` whose icon-font CSS failed to load reports `0x0`.
  //     The parent flex's `gap: Npx` adds N px between this empty box and
  //     every later sibling; dropping it would silently shift those
  //     siblings, breaking alignment with the source paint.
  // Visibility (`display: none` / `visibility: hidden`) is filtered
  // upstream in `flexItemChildren`, so anything that reaches here is
  // layout-active.
  const rectIsDegenerate = !opts.flexItem && !nonZero(rect);
  if (rectIsDegenerate && !elementHasChildren(el) && !gatherDirectText(el) &&
      !syntheticText(el)) {
    return '';
  }

  const left = rect.left - parentRect.left;
  const top = rect.top - parentRect.top;

  // Pre-pass-tagged icon-font hosts: the inline-icon-fonts pass
  // (lib/icon-font.js) replaced the live `::before` glyph with an inline
  // `<svg>` payload, indexed by id under `data-snapshot-icon-svg-id` /
  // `window.__pagxIconSvgs`. Route to the dedicated renderer ahead of
  // every other branch so we never accidentally fall through to the
  // legacy pseudo-text path (which would emit a font-named `<span>` with
  // the original codepoint alongside the SVG, double-painting the icon).
  // This check is cheap enough — `getAttribute` on a missing attribute
  // returns null without touching the attribute table — to leave outside
  // the kind dispatch.
  if (el.getAttribute && el.getAttribute('data-snapshot-icon-svg-id')) {
    return renderInlineIconSvg(el, parentRect, rect, left, top, computed, opts);
  }

  // Dispatch table for the three kinds with dedicated renderers; the `box`
  // kind has its own bifurcation (text-leaf vs container) handled below.
  const handler = KIND_DISPATCH[kind];
  if (handler) return handler(el, parentRect, rect, left, top, computed, opts);

  // CSS triangle hack (`width:0; height:0` + asymmetric solid borders) doesn't
  // fit any of the box renderers — the host has zero content area but its
  // paint box is filled by border triangles. Emit it as an inline <svg> with
  // one <polygon> per visible side so the shape (and any `transform: rotate`
  // baked into the polygon coordinates) survives the trip to PAGX.
  if (isCssBorderTrianglePattern(el, computed)) {
    return renderBorderTriangle(el, parentRect, rect, left, top, computed, opts);
  }

  // Single-pass childNodes scan: the four branches below all consult
  // `elementHasChildren` and/or `gatherDirectText`. Calling them
  // independently re-walks `childNodes` 3–4 times for every box element on
  // the page. The dispatch and triangle branches above don't need either
  // value (and KIND_DISPATCH covers the most common early-return shapes),
  // so the scan is placed here where every remaining branch will actually
  // use the result.
  const { hasElementChild, directText } = scanChildNodes(el);
  if (!hasElementChild && directText) {
    return renderTextLeaf(el, parentRect, rect, left, top, computed, directText, opts);
  }
  // Icon-font / decorative pseudo case: the host has no DOM children and
  // no direct text, but the stylesheet supplies a `::before`/`::after`
  // content string. Emit that as inner spans so the glyph actually shows
  // up; the container path would otherwise return an empty <div>.
  if (!hasElementChild && hasPseudoContent(el)) {
    return renderPseudoTextLeaf(el, parentRect, rect, left, top, computed, opts);
  }
  // Mixed-style inline text leaf (`<p>NN<span class="text-[28px]">unit</span></p>`
  // and friends). Reconstructs the original inline flow so PAGX's text
  // engine — rather than the snapshot's hard-coded `left` values — owns
  // the inter-run spacing. Without this, font-fallback width drift
  // between the snapshot browser and `pagx render` overlaps adjacent
  // runs (e.g. "22.25" eating its trailing "%"). See
  // `isInlineTextLeafCandidate` for the full rationale.
  if (isInlineTextLeafCandidate(el, computed, hasElementChild)) {
    return renderInlineTextLeaf(el, parentRect, rect, left, top, computed, opts);
  }
  return renderContainer(el, parentRect, rect, left, top, computed, opts);
}

// ===== Main snapshot entry =====

function snapshotMain() {
  const body = document.body;
  body.style.margin = '0';
  body.style.padding = '0';
  // If the page (or the user) scrolled before snapshot, every
  // `getBoundingClientRect` call returns viewport-relative offsets shifted
  // by the scroll position. Reset to (0,0) so that the body's own rect
  // lands at the origin and every nested rect is measured against the
  // document, not the current viewport.
  if (typeof window.scrollTo === 'function') {
    try { window.scrollTo(0, 0); } catch (_) { /* ignore */ }
  }
  // Force layout flush.
  void body.offsetHeight;

  // Pick canvas size from the body itself. We deliberately do NOT consult
  // `document.documentElement.scrollWidth/scrollHeight` here — the root
  // element is sized to the viewport whenever the body is smaller, so a
  // phone-sized mock (`<body style="width: 375px; height: 812px">`) would
  // otherwise be inflated to the puppeteer viewport (1400x900) and rendered
  // as a tiny island in the top-left corner. `body.scrollWidth/scrollHeight`
  // already includes any child content that overflows past the body's
  // declared size, so it correctly captures both fixed-size mocks and fluid
  // pages whose content extends past the viewport.
  const bodyRect = body.getBoundingClientRect();
  const canvasWidth = Math.max(body.scrollWidth, Math.round(bodyRect.width));
  const canvasHeight = Math.max(body.scrollHeight, Math.round(bodyRect.height));

  const parts = [];
  for (const c of body.children) {
    parts.push(render(c, bodyRect));
  }

  const title = (document.title || '').trim();
  const bodyComputed = getComputedStyle(body);
  const bodyBg = bodyComputed.getPropertyValue('background-color').trim();
  const bodyBgImage = normalizeBackgroundImage(
    bodyComputed.getPropertyValue('background-image').trim(),
  );
  const bodyStyle = [
    `width: ${canvasWidth}px`,
    `height: ${canvasHeight}px`,
  ];
  if (bodyBg && !['rgba(0, 0, 0, 0)', 'transparent'].includes(bodyBg)) {
    bodyStyle.push(`background-color: ${bodyBg}`);
  }
  // Body-level gradient (e.g. a full-page hero backdrop) would otherwise be
  // lost because the snapshot's <body> only emits sizing + background-color.
  if (bodyBgImage) {
    bodyStyle.push(`background-image: ${bodyBgImage}`);
  }

  // The <style> block centralises four rendering invariants that the
  // user-agent doesn't supply by default — without them, opening the
  // snapshot in a browser diverges from both the original page and `pagx
  // render`'s output:
  //   - `box-sizing: border-box` on every box matches the semantics of
  //     `getBoundingClientRect()` (the source for our `width`/`height`):
  //     we record padded/bordered extents, so the browser default
  //     `content-box` would re-add padding on top and inflate every
  //     wrapper. The pagx importer already treats border-box as the
  //     default (HTMLStyleResolver.cpp), so this is purely browser-side
  //     parity.
  //   - `margin: 0` / `padding: 0` on <body> cancel the user-agent 8px
  //     body margin. The snapshot script already zeroes them on the live
  //     DOM before measuring; without echoing that into the output, every
  //     absolute child reads 8 px short on both axes when re-opened.
  //   - `position: relative` on <body> makes it a containing block.
  //     Otherwise our `position: absolute` children resolve to the initial
  //     containing block (the viewport) and escape the body's coordinate
  //     system.
  //   - `<html>` becomes a flex container that centres <body> horizontally
  //     and pads it vertically. Without this, opening the snapshot in a
  //     viewport wider/taller than the captured canvas left the body at
  //     (0, 0) — any `box-shadow` extending above/left of the body (e.g. a
  //     phone-frame's outer ring drawn at y = -12) would be clipped by the
  //     browser viewport. Using flex on <html> doesn't change <body>'s
  //     measured rect, so the pagx importer (which roots at <body> and
  //     ignores <html>) still sees the same canvas.
  // Element selectors inside a single `<style>` block are inside the
  // subset (`spec/html_subset.md` §3.3); the pagx importer parses them,
  // applies the (no-op) declarations, and drops the `<style>` element.
  const styleBlock =
    'div,span,img,svg,body{box-sizing:border-box}' +
    'html{min-height:100vh;display:flex;justify-content:center;' +
    'align-items:flex-start;padding:32px 0}' +
    'body{margin:0;padding:0;position:relative;flex-shrink:0}';

  return {
    html:
`<!DOCTYPE html>
<html>
  <head>
    <title>${title.replace(/</g, '&lt;').replace(/&/g, '&amp;')}</title>
    <style>${styleBlock}</style>
  </head>
  <body style="${bodyStyle.join('; ')}">
${parts.join('')}
  </body>
</html>
`,
    width: canvasWidth,
    height: canvasHeight,
  };
}

// ===== Browser-side preamble (constants embedded as raw JS source) =====

// All values that can't be cleanly JSON-serialised — Sets, Maps, the
// SIDE_OVERLAY object full of arrow functions, the STYLE_SCHEMA table whose
// `normalize` slots reference helper functions, and the KIND_DISPATCH
// table — are emitted as JS source. Function names referenced here resolve
// via JavaScript's function-declaration hoisting inside the IIFE: the
// helpers are concatenated into the same scope and become visible at the
// top of execution regardless of their textual position.
const PAYLOAD_CONSTANTS_SRC = `
const DROP_TAGS = new Set(${DROP_TAG_NAMES_JSON});

const INLINE_RUN_TAGS = new Set(['span', 'a']);

const INLINE_BY_DEFAULT_TAGS = new Set([
  'span', 'a', 'b', 'i', 'em', 'strong', 'small', 'big', 'sub', 'sup',
  'mark', 'time', 'abbr', 'cite', 'code', 'kbd', 'samp', 'var', 'q', 's',
  'u', 'del', 'ins', 'dfn', 'bdo', 'bdi', 'wbr', 'ruby', 'rt', 'rp',
]);

const NON_TEXT_INPUT_TYPES = new Set([
  'checkbox', 'radio', 'file', 'range', 'color', 'image', 'hidden',
  'date', 'datetime-local', 'month', 'time', 'week',
]);

const SIDES = ['top', 'right', 'bottom', 'left'];

const SIDE_OVERLAY = {
  top:    (w, h, t) => ({ left: 0,                  top: 0,                  w: w, h: t }),
  bottom: (w, h, t) => ({ left: 0,                  top: Math.max(0, h - t), w: w, h: t }),
  left:   (w, h, t) => ({ left: 0,                  top: 0,                  w: t, h: h }),
  right:  (w, h, t) => ({ left: Math.max(0, w - t), top: 0,                  w: t, h: h }),
};

const ALIGN_ITEMS_OK = new Set(['stretch', 'center', 'flex-start', 'flex-end']);
const JUSTIFY_CONTENT_OK = new Set([
  'flex-start', 'flex-end', 'center',
  'space-between', 'space-around', 'space-evenly',
]);
const ALIGN_ITEMS_ALIAS = new Map([
  ['start', 'flex-start'],
  ['end', 'flex-end'],
  ['self-start', 'flex-start'],
  ['self-end', 'flex-end'],
  ['normal', 'stretch'],
]);
const JUSTIFY_CONTENT_ALIAS = new Map([
  ['start', 'flex-start'],
  ['end', 'flex-end'],
  ['left', 'flex-start'],
  ['right', 'flex-end'],
  ['normal', 'flex-start'],
]);

const STYLE_SCHEMA = [
  { prop: 'background-color', scope: 'box',  defaults: ['rgba(0, 0, 0, 0)', 'transparent'] },
  { prop: 'background-image', scope: 'box',  defaults: ['none'], normalize: normalizeBackgroundImage },
  { prop: 'background-clip',  scope: 'box',  defaults: ['border-box'], normalize: normalizeBackgroundClip },
  { prop: 'border-radius',    scope: 'box',  defaults: ['0px', '0px 0px 0px 0px'], normalize: normalizeBorderRadius },
  { prop: 'overflow',         scope: 'box',  defaults: ['visible'], normalize: normalizeOverflow },
  { prop: 'opacity',          scope: 'box',  defaults: ['1'] },
  { prop: 'filter',           scope: 'box',  defaults: ['none'] },
  { prop: 'backdrop-filter',  scope: 'box',  defaults: ['none'] },
  { prop: 'mix-blend-mode',   scope: 'box',  defaults: ['normal'] },
  // Alpha / luminance masks (mask-image: url(data:image/svg+xml,...)) and the
  // mask-mode / mask-size / mask-position / mask-repeat descriptors the HTML
  // exporter emits alongside them. The importer rebuilds a PAGX mask layer from
  // the data-URI SVG (the inverse of HTMLWriter::writeMaskCSS), so the whole
  // group must survive the snapshot. mask-image defaults to none and is dropped;
  // the descriptors are forwarded only when the element actually carries a mask,
  // gated by appendMaskFitting below (they are noise on unmasked boxes).
  { prop: 'mask-image',       scope: 'box',  defaults: ['none'] },
  // clip-path: url(#id) references a hidden clipPath def the snapshot keeps as an
  // inline svg; the importer resolves the def into a contour mask layer (the
  // inverse of HTMLWriter::writeClipDef). Geometric clip-path forms (inset/ellipse)
  // are out of subset and collapse to '' so the defaults filter drops them.
  { prop: 'clip-path',        scope: 'box',  defaults: ['none'], normalize: normalizeClipPath },
  { prop: 'color',                 scope: 'text', defaults: ['rgb(0, 0, 0)'] },
  { prop: 'font-family',           scope: 'text', normalize: normalizeFontFamily },
  { prop: 'font-size',             scope: 'text' },
  { prop: 'font-weight',           scope: 'text', defaults: ['400', 'normal'] },
  { prop: 'font-style',            scope: 'text', defaults: ['normal'] },
  { prop: 'letter-spacing',        scope: 'text', defaults: ['normal', '0px'] },
  { prop: 'line-height',           scope: 'text' },
  { prop: 'text-align',            scope: 'text', defaults: ['start', 'left'] },
  { prop: 'text-decoration-line',  scope: 'text', defaults: ['none'], outProp: 'text-decoration' },
  { prop: 'text-decoration-color', scope: 'text', defaults: ['currentcolor'], skipIfEqualsTextColor: true },
  { prop: 'white-space',           scope: 'text', defaults: ['normal'] },
  { prop: 'writing-mode',          scope: 'text', defaults: ['horizontal-tb'], normalize: normalizeWritingMode },
];
const STYLE_SCHEMA_BY_PROP = new Map(STYLE_SCHEMA.map((e) => [e.prop, e]));
const STYLE_SCHEMA_BOX = STYLE_SCHEMA.filter((e) => e.scope === 'box');
const STYLE_SCHEMA_TEXT = STYLE_SCHEMA.filter((e) => e.scope === 'text');

const KIND_DISPATCH = { svg: renderSvg, img: renderImg, canvas: renderCanvas, text: renderTextInput };
`;

// ===== Payload assembly =====

// Order matters only for runtime visibility of CONST initialisers; the
// helper function declarations are hoisted to the top of the IIFE scope, so
// references from inside the constants block (e.g. STYLE_SCHEMA's
// `normalize: normalizeFontFamily`) resolve correctly even though the
// helpers' source text appears below the constants in the concatenated
// payload.
const HELPER_FNS = [
  normalizeFontFamily,
  normalizeOverflow,
  normalizeBorderRadius,
  normalizeBackgroundImage,
  normalizeBackgroundClip,
  normalizeClipPath,
  normalizeWritingMode,
  roundPx,
  px,
  formatFlexGrow,
  escapeHtml,
  joinStyles,
  withNowrap,
  paddingShorthand,
  nonZero,
  isVisible,
  readNum,
  readBox,
  readPadding,
  readMargin,
  borderWidthOf,
  readBorderSide,
  paddingBoxOrigin,
  isUniformBorder,
  hasAnyBorder,
  resolveZIndex,
  paintOrder,
  gatherDirectText,
  elementHasChildren,
  scanChildNodes,
  firstTextNodeChild,
  pseudoText,
  hasPseudoContent,
  imgSrc,
  syntheticText,
  applyTextTransform,
  classify,
  appendStyleProp,
  appendBorder,
  appendBoxShadow,
  resolveTextStroke,
  appendTextStroke,
  appendBackgroundImageFitting,
  appendMaskFitting,
  buildStyle,
  readCornerRadii,
  roundedUniformBorderSvg,
  borderOverlayHTML,
  colorAlpha,
  transformOriginXY,
  transformMatrix,
  readInlineTransform,
  readBoxTransform,
  withStrippedTransform,
  isCssBorderTrianglePattern,
  renderBorderTriangle,
  imageInnerStyle,
  freezeSvg,
  splitTextNodeIntoLines,
  emitTextSpans,
  emitVerticalTextSpans,
  hasBoxVisualsForInline,
  multiplyColorAlpha,
  resolveInlineTextValue,
  isInlineRunChild,
  isInlineTextLeafCandidate,
  emitInlineRunMarkup,
  hasAuthorDefinedFlexSize,
  isPureInlineTextLeaf,
  renderInlineTextLeaf,
  flexMainGapPx,
  collectFlexProps,
  isMultiLineTextLeafItem,
  flexItemChildren,
  isFlexLayoutFaithful,
  classifyFlexContainer,
  renderSvg,
  renderInlineIconSvg,
  renderBoxedReplaced,
  renderImg,
  renderCanvas,
  renderTextInput,
  textLeafInnerLayout,
  renderTextLeaf,
  renderPseudoTextLeaf,
  renderFlexTextItem,
  renderFlexContainer,
  renderContainer,
  renderChildrenInto,
  render,
  snapshotMain,
];

const HELPERS_SRC = HELPER_FNS.map((fn) => fn.toString()).join('\n\n');

// Single-shot browser-side payload. `page.evaluate` accepts a string and
// will return whatever the expression evaluates to, so the IIFE simply
// returns `snapshotMain()`'s result.
//
// Kept as the legacy "self-contained" form so the standalone browser
// bundle (build-browser-bundle.js) and any external embedder can keep
// calling it without an init-script step. The puppeteer driver in
// snapshot.js prefers the split-payload route (SNAPSHOT_INIT_SCRIPT +
// TAKE_SNAPSHOT_EXPR below) so the ~80 KB helper source is shipped to the
// browser exactly once at navigation time instead of on every evaluate.
const takeSnapshot = `(() => {\n${HELPERS_SRC}\n${PAYLOAD_CONSTANTS_SRC}\nreturn snapshotMain();\n})()`;

// Init-script form: install every helper / constant onto a fresh
// `window.__pagxSnapshot` namespace so subsequent evaluate calls only ship
// the entry expression (`TAKE_SNAPSHOT_EXPR`, ~70 bytes) instead of the
// full helper source. The IIFE runs once per document load (registered via
// `page.evaluateOnNewDocument` / Playwright `addInitScript`), before any
// of the page's own scripts.
const SNAPSHOT_INIT_SCRIPT = `(function() {
${HELPERS_SRC}
${PAYLOAD_CONSTANTS_SRC}
window.__pagxSnapshot = {
  takeSnapshot: snapshotMain,
};
})();`;

// Entry expression matching SNAPSHOT_INIT_SCRIPT. Compact enough that the
// CDP roundtrip per snapshot is bounded by the protocol overhead, not the
// payload size.
const TAKE_SNAPSHOT_EXPR = '(() => window.__pagxSnapshot.takeSnapshot())()';

// ===== Pre-snapshot pass: inline external <img> sources =====

// Walk every <img> in the document and normalise its `src` for `pagx render`,
// which can read `data:` URIs and local files but cannot fetch URLs at render
// time. The result is stashed on `data-snapshot-src`, which the downstream
// snapshot reader (`imgSrc`) prefers over the original attribute:
//
//   - `http(s)://` src  → a `data:` URI of the bytes (self-contained snapshot).
//   - `file://` src      → the absolute filesystem path. A relative <img src>
//     on a local page resolves to a `file://` URL the browser computed against
//     the source document; emitting it as an absolute path keeps the subset
//     valid regardless of where it is written, since PAGX otherwise resolves a
//     bare relative `src` against the subset HTML's own directory.
//
// Two byte sources, in priority order:
//
//   1. `cachedMap` — a `{ url: dataURI }` table populated by snapshot.js
//      from `page.on('response')`. The browser already loaded these images
//      to render the page, so reusing those bytes bypasses the second
//      cross-origin round-trip entirely (and the CORS preflight that the
//      old `fetch(url, { mode: 'cors' })` path silently failed at — most
//      business image CDNs do not return ACAO headers, so the legacy path
//      lost essentially every cross-origin image).
//
//   2. In-page `fetch()` fallback — used when the cache misses (e.g. the
//      browser-bundle entry point in build-browser-bundle.js, which has no
//      Node side to capture responses; or images that loaded before the
//      response listener attached). Limited to 8 concurrent requests so
//      pages with hundreds of icons don't trip same-origin connection
//      caps or upstream 429 throttling.
//
// Failures (network, 404, tainted cache hit) are logged and left in place:
// the snapshot will then fall back to the original URL and render as an
// empty box rather than aborting the pipeline.
async function inlineExternalImages(cachedMap) {
  // Convert a Blob into a `data:` URI without the FileReader callback dance.
  // Reading bytes via `arrayBuffer()` + chunked btoa avoids the "Maximum
  // call stack size" trap that String.fromCharCode(...big) hits on large
  // images.
  async function blobToDataUri(blob) {
    const buf = await blob.arrayBuffer();
    const bytes = new Uint8Array(buf);
    let binary = '';
    const CHUNK = 0x8000;
    for (let i = 0; i < bytes.length; i += CHUNK) {
      binary += String.fromCharCode.apply(null, bytes.subarray(i, i + CHUNK));
    }
    const mime = blob.type || 'application/octet-stream';
    return `data:${mime};base64,${btoa(binary)}`;
  }

  // Convert a `file://` URL (the absolute form the browser resolves a local /
  // relative <img src> into) back to a plain filesystem path. PAGX's importer
  // treats a leading `/` (POSIX) or `C:/` (Windows) as absolute and reads the
  // file directly, but does not understand the `file://` scheme, so we strip
  // it and percent-decode the path. Returns '' for anything that isn't a
  // file URL so the caller can fall through to its other branches.
  function fileUrlToPath(fileUrl) {
    try {
      const u = new URL(fileUrl);
      if (u.protocol !== 'file:') return '';
      let p = decodeURIComponent(u.pathname);
      // Windows: `file:///C:/dir/x.png` → pathname `/C:/dir/x.png`; drop the
      // leading slash so the drive letter starts the path.
      if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
      return p;
    } catch (_) {
      return '';
    }
  }

  const cache = cachedMap || {};
  const imgs = Array.from(document.querySelectorAll('img'));
  const pending = [];
  for (const img of imgs) {
    const src = img.currentSrc || img.src || img.getAttribute('src') || '';
    if (!src) continue;
    if (src.startsWith('data:')) continue;
    // Local image referenced by a relative (or absolute-but-relative-to-the-
    // source) path: the browser has already resolved it to an absolute
    // `file://` URL. Emit that as a plain absolute filesystem path so the
    // subset HTML keeps working no matter which directory it is written to —
    // PAGX otherwise resolves a bare relative `src` against the subset's own
    // location, which breaks the moment the subset and the image files no
    // longer share a directory (e.g. the eval pipeline writes the subset under
    // out/<label>/<case>/ while the images live elsewhere).
    if (/^file:/i.test(src)) {
      const fsPath = fileUrlToPath(src);
      if (fsPath) img.setAttribute('data-snapshot-src', fsPath);
      continue;
    }
    if (!/^https?:/i.test(src)) continue;
    const cached = cache[src];
    if (cached) {
      img.setAttribute('data-snapshot-src', cached);
      continue;
    }
    pending.push({ img, src });
  }

  async function processOne(entry) {
    try {
      const res = await fetch(entry.src, { credentials: 'omit' });
      if (!res.ok) {
        console.warn(`html-snapshot: image fetch ${res.status} for ${entry.src}`);
        return;
      }
      const dataUri = await blobToDataUri(await res.blob());
      entry.img.setAttribute('data-snapshot-src', dataUri);
    } catch (err) {
      console.warn(`html-snapshot: failed to inline ${entry.src}: ${err && err.message}`);
    }
  }

  // Same-origin browsers cap parallel connections at ~6, and many CDNs
  // throttle aggressively past that. Keeping the in-flight window at 8
  // lets us saturate the bottleneck without queueing surprises.
  const FETCH_CHUNK_SIZE = 8;
  for (let i = 0; i < pending.length; i += FETCH_CHUNK_SIZE) {
    const slice = pending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(processOne));
  }

  // Pass 2: remote CSS `background-image: url(...)`. Chromium reports the url already resolved
  // to an absolute form. Local `file://` urls are handled at emit time by
  // `normalizeBackgroundImage` (stripped to a plain path the importer loads directly), so only
  // remote `http(s)` bytes need inlining here — fetch them and overwrite the element's inline
  // `background-image` with a `data:` URI so the walker emits a self-contained reference. Only
  // the first layer of a comma-separated stack is handled: that is the single-image case the
  // exporter produces and the only one the importer recovers.
  function firstCssUrl(value) {
    const m = /url\(\s*(['"]?)([^'")]+)\1\s*\)/i.exec(value || '');
    return m ? m[2] : '';
  }

  const bgPending = [];
  const allEls = Array.from(document.querySelectorAll('*'));
  for (const el of allEls) {
    const bg = getComputedStyle(el).getPropertyValue('background-image').trim();
    if (!bg || !/^url\(/i.test(bg)) continue;
    const url = firstCssUrl(bg);
    if (!url || url.startsWith('data:') || /^file:/i.test(url)) continue;
    if (!/^https?:/i.test(url)) continue;
    const cached = cache[url];
    if (cached) {
      el.style.backgroundImage = `url("${cached.replace(/"/g, '\\"')}")`;
      continue;
    }
    bgPending.push({ el, url });
  }

  async function processOneBg(entry) {
    try {
      const res = await fetch(entry.url, { credentials: 'omit' });
      if (!res.ok) {
        console.warn(`html-snapshot: bg-image fetch ${res.status} for ${entry.url}`);
        return;
      }
      const dataUri = await blobToDataUri(await res.blob());
      entry.el.style.backgroundImage = `url("${dataUri.replace(/"/g, '\\"')}")`;
    } catch (err) {
      console.warn(`html-snapshot: failed to inline bg ${entry.url}: ${err && err.message}`);
    }
  }

  for (let i = 0; i < bgPending.length; i += FETCH_CHUNK_SIZE) {
    const slice = bgPending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(processOneBg));
  }
}

// ===== Pre-snapshot pass: snapshot live <canvas> bitmaps =====

// Walk every <canvas> in the document and stash a `data:image/png;base64,...`
// URI of its current bitmap on `data-snapshot-canvas-src`. The snapshot
// renderer treats a canvas with that attribute as if it were an <img>; charts
// drawn at runtime by ECharts, Chart.js, D3-canvas, etc. therefore survive
// the trip to PAGX as ordinary static images, even though PAGX has no canvas
// model.
//
// Failure modes left as silent drops (snapshot walker treats canvases without
// the attribute as if they were still in DROP_TAGS):
//   - SecurityError ("tainted canvas"): the page drew a cross-origin image
//     into the canvas without CORS, so the browser refuses to serialise it.
//   - WebGL contexts created without `preserveDrawingBuffer: true` may have
//     an empty back buffer by the time we read it. Workaround: pages can
//     preserve the buffer themselves, or call `drawImage` into a 2D canvas
//     before snapshot.
//   - Zero-sized canvas (`width=0` or `height=0`): toDataURL returns the
//     trivial `"data:,"`, which we skip so the renderer drops the element.
async function inlineCanvases() {
  const canvases = Array.from(document.querySelectorAll('canvas'));
  for (const canvas of canvases) {
    if (!canvas.width || !canvas.height) continue;
    let dataUri = '';
    try {
      dataUri = canvas.toDataURL('image/png');
    } catch (err) {
      console.warn(`html-snapshot: canvas.toDataURL failed: ${err && err.message}`);
      continue;
    }
    if (!dataUri || dataUri === 'data:,') continue;
    canvas.setAttribute('data-snapshot-canvas-src', dataUri);
  }
}

// ===== Pre-snapshot pass: materialise decorative ::before / ::after pseudo-elements =====

// CSS pseudo-elements have no DOM presence, so the snapshot walker (which
// reads boxes off `getBoundingClientRect`) sees nothing for a pseudo whose
// rendering is purely decorative — the white slider thumb generated by
// `.ios-switch::after { content: ""; width: 27px; ... }`, the dot inside a
// custom radio, the divider produced by a `::before { content: ""; height:
// 1px; background: #eee; }`. The existing `renderPseudoTextLeaf` covers
// *text*-bearing pseudos (icon-font glyphs whose `content` carries a
// codepoint) but emits nothing for pseudos whose `content` resolves to an
// empty string and whose visible box comes from width/height/background.
//
// This pass walks every element on the live page, asks Chromium for the
// resolved style of each pseudo, and — when the pseudo carries a
// `content` value but that value is the empty string — appends a real
// `<div>` child to the host whose inline style mirrors the pseudo's own
// box. The synthetic node carries `data-snapshot-pseudo` so authors can
// trace the emission, and a `data-snapshot-pseudo-host=""` attribute is
// added to the host to suppress the `renderPseudoTextLeaf` fallback (which
// would otherwise still try to emit the now-empty pseudo as inline text).
//
// Two scopes deliberately stay out:
//
//   - text-bearing pseudos (`content: "→"`, `content: "\\e123"`): handled
//     by `renderPseudoTextLeaf`, which sizes the glyph against its own
//     font metrics. Materialising those would double-paint the text.
//
//   - in-flow decorative pseudos (`position: static / relative / sticky`):
//     pseudos in the host's flow take main/cross-axis space; injecting a
//     real sibling would shift the host's other children. The pseudos that
//     actually need this fix in practice are absolutely / fixed-positioned
//     boxes (toggles, radios, checkboxes, dividers anchored with `inset`),
//     so we limit materialisation to that case and skip the rest with a
//     `data-snapshot-pseudo-skipped=in-flow` marker for diagnosability.
//
// The properties copied onto the synthetic div cover the ones the snapshot
// walker (`STYLE_SCHEMA_BOX` + the `position/inset/transform/box-shadow`
// reads scattered across `buildStyle`/`borderOverlayHTML`/`transformMatrix`)
// actually consults; copying every CSS property would balloon the inline
// style attribute without adding visible information. Border longhands are
// emitted per side so asymmetric borders survive.
async function materializeDecorativePseudoElements() {
  // Helper functions duplicated locally so this function stays self-contained
  // when shipped through `page.evaluate`. The Node-side helpers are not in
  // scope inside the browser context — Puppeteer/Playwright serialise the
  // function source and only the lexical body travels across the boundary,
  // so any Node-side identifier used here would throw `ReferenceError` in
  // the page. Mirror the helper from this file's top section verbatim.
  function readNum(computed, prop) {
    return parseFloat(computed.getPropertyValue(prop)) || 0;
  }
  const PSEUDO_TYPES = ['::before', '::after'];
  // Properties whose computed value the snapshot walker reads from the
  // element. Listing them explicitly keeps the synthetic style attribute
  // bounded and deterministic. Border longhands are expanded so asymmetric
  // borders (`border-top: 1px solid ...`) round-trip.
  const COPY_PROPS = [
    'position', 'left', 'right', 'top', 'bottom',
    'width', 'height',
    'margin-top', 'margin-right', 'margin-bottom', 'margin-left',
    'padding-top', 'padding-right', 'padding-bottom', 'padding-left',
    'background-color', 'background-image', 'background-clip',
    'background-size', 'background-repeat', 'background-position',
    'border-top-width', 'border-right-width', 'border-bottom-width', 'border-left-width',
    'border-top-style', 'border-right-style', 'border-bottom-style', 'border-left-style',
    'border-top-color', 'border-right-color', 'border-bottom-color', 'border-left-color',
    'border-top-left-radius', 'border-top-right-radius',
    'border-bottom-left-radius', 'border-bottom-right-radius',
    'box-shadow', 'opacity', 'mix-blend-mode', 'filter', 'backdrop-filter',
    'overflow',
    'transform', 'transform-origin',
    'z-index',
    'box-sizing',
    'color', 'font-family', 'font-size', 'font-weight', 'font-style',
    'line-height', 'letter-spacing', 'text-align',
  ];

  // Defaults emitted by Chromium for properties that don't change anything.
  // Suppressing them keeps the inline style readable and lines up with the
  // snapshot walker's own default-skipping logic.
  const DEFAULTS = new Map([
    ['position', 'static'],
    ['left', 'auto'], ['right', 'auto'], ['top', 'auto'], ['bottom', 'auto'],
    ['margin-top', '0px'], ['margin-right', '0px'],
    ['margin-bottom', '0px'], ['margin-left', '0px'],
    ['padding-top', '0px'], ['padding-right', '0px'],
    ['padding-bottom', '0px'], ['padding-left', '0px'],
    ['background-color', 'rgba(0, 0, 0, 0)'],
    ['background-image', 'none'],
    ['background-clip', 'border-box'],
    ['background-size', 'auto'],
    ['background-repeat', 'repeat'],
    ['background-position', '0% 0%'],
    ['border-top-width', '0px'], ['border-right-width', '0px'],
    ['border-bottom-width', '0px'], ['border-left-width', '0px'],
    ['border-top-style', 'none'], ['border-right-style', 'none'],
    ['border-bottom-style', 'none'], ['border-left-style', 'none'],
    ['border-top-left-radius', '0px'], ['border-top-right-radius', '0px'],
    ['border-bottom-left-radius', '0px'], ['border-bottom-right-radius', '0px'],
    ['box-shadow', 'none'], ['opacity', '1'], ['mix-blend-mode', 'normal'],
    ['filter', 'none'], ['backdrop-filter', 'none'], ['overflow', 'visible'],
    ['transform', 'none'],
    ['z-index', 'auto'],
    ['box-sizing', 'content-box'],
  ]);

  // A pseudo's `content` value reaches us with quotes preserved
  // ("\"\"" for content: ""). Strip surrounding ASCII quotes so the
  // empty-vs-text branch can be decided on the inner string.
  function unquoteContent(raw) {
    const trimmed = (raw || '').trim();
    if (!trimmed) return '';
    if (trimmed === 'none' || trimmed === 'normal') return null;
    // Concatenate every quoted run; ignore counter() / attr() / url() forms
    // (they don't contribute a stable visible string, and a non-empty
    // catch-all `out` still routes through the `renderPseudoTextLeaf` path
    // for any glyph contribution).
    const re = /"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'/g;
    let out = '';
    let m;
    while ((m = re.exec(trimmed)) !== null) {
      out += (m[1] !== undefined ? m[1] : m[2]);
    }
    return out;
  }

  // Decide whether a pseudo with the given resolved style should be
  // materialised. Out-of-flow position is required so the synthetic sibling
  // doesn't push the host's real children around. A pseudo with no visible
  // box (no width / height / background / border / shadow / transform) is
  // skipped — there's nothing to render anyway.
  function shouldMaterialise(cs, pseudoText) {
    if (pseudoText !== '') {
      return { ok: false, reason: 'text-content' };
    }
    const position = (cs.getPropertyValue('position') || '').trim();
    if (position !== 'absolute' && position !== 'fixed') {
      return { ok: false, reason: 'in-flow' };
    }
    const widthPx = readNum(cs, 'width');
    const heightPx = readNum(cs, 'height');
    if (widthPx <= 0 && heightPx <= 0) {
      return { ok: false, reason: 'zero-size' };
    }
    return { ok: true };
  }

  function emitInlineStyle(cs) {
    const parts = [];
    for (const prop of COPY_PROPS) {
      const raw = (cs.getPropertyValue(prop) || '').trim();
      if (!raw) continue;
      const def = DEFAULTS.get(prop);
      if (def !== undefined && raw === def) continue;
      // Rewrite embedded double quotes (e.g. `filter: url("#blur")`,
      // `font-family: "Inter"`) as single quotes — CSS treats them equivalently
      // — so the value embeds safely inside the surrounding style="…" attribute
      // and doesn't break the importer's XML parse. Mirrors appendStyleProp on
      // the main snapshot path.
      parts.push(`${prop}: ${raw.replace(/"/g, "'")}`);
    }
    return parts.join('; ');
  }

  // `<svg>` subtrees are an opaque resolver target downstream — leaving
  // pseudo-elements declared on inline SVG markup alone matches how the
  // snapshot already passes the SVG through verbatim.
  const all = document.querySelectorAll('*');
  for (let i = 0; i < all.length; i++) {
    const el = all[i];
    if (!(el instanceof Element)) continue;
    const tag = el.tagName.toLowerCase();
    if (tag === 'script' || tag === 'style' || tag === 'meta' ||
        tag === 'link' || tag === 'title' || tag === 'head' || tag === 'html') {
      continue;
    }
    // Don't descend into SVG content; stylesheets rarely target ::before/
    // ::after on SVG elements and the snapshot ships SVG opaquely.
    if (el.closest('svg') && tag !== 'svg') continue;
    // Skip our own synthetic nodes from a previous pass (defensive — the
    // pipeline today calls this exactly once per page, but the in-page
    // helpers can also be invoked manually).
    if (el.hasAttribute('data-snapshot-pseudo')) continue;

    // Two-phase decision: first read both pseudos so we know whether the
    // host carries a text-bearing pseudo. If it does, leave the host alone
    // — `renderPseudoTextLeaf` already produces the correct emission, and
    // appending a real child would knock that path out by flipping
    // `hasElementChild` to true.
    const pseudoData = [];
    let hasTextPseudo = false;
    for (const pseudo of PSEUDO_TYPES) {
      const cs = getComputedStyle(el, pseudo);
      const contentRaw = (cs.getPropertyValue('content') || '').trim();
      const pseudoText = unquoteContent(contentRaw);
      if (pseudoText === null) {
        // content: none / normal — pseudo not generated.
        pseudoData.push(null);
        continue;
      }
      if (pseudoText !== '') hasTextPseudo = true;
      pseudoData.push({ cs, pseudoText, pseudo });
    }
    if (hasTextPseudo) continue;

    let materialisedAny = false;
    for (const slot of pseudoData) {
      if (!slot) continue;
      const decision = shouldMaterialise(slot.cs, slot.pseudoText);
      if (!decision.ok) {
        if (decision.reason !== 'text-content') {
          el.setAttribute('data-snapshot-pseudo-skipped', decision.reason);
        }
        continue;
      }

      const div = document.createElement('div');
      div.setAttribute('data-snapshot-pseudo', slot.pseudo);
      const style = emitInlineStyle(slot.cs);
      if (style) div.setAttribute('style', style);
      // `::before` is painted before the host's children, `::after` after.
      // Mirror that order in the DOM so the natural document order matches.
      if (slot.pseudo === '::before') {
        if (el.firstChild) {
          el.insertBefore(div, el.firstChild);
        } else {
          el.appendChild(div);
        }
      } else {
        el.appendChild(div);
      }
      materialisedAny = true;
    }
    if (materialisedAny) {
      el.setAttribute('data-snapshot-pseudo-host', '');
    }
  }
}

/* eslint-enable no-undef, no-inner-declarations */

// `HELPERS_SRC` / `PAYLOAD_CONSTANTS_SRC` are exposed for the browser-bundle
// build script (see build-browser-bundle.js): the bundle inlines them into a
// UMD wrapper so the same snapshot logic runs without puppeteer when loaded
// via `<script>`. The puppeteer driver only needs `takeSnapshot` /
// `inlineExternalImages`.
export {
  takeSnapshot,
  SNAPSHOT_INIT_SCRIPT,
  TAKE_SNAPSHOT_EXPR,
  inlineExternalImages,
  inlineCanvases,
  materializeDecorativePseudoElements,
  HELPERS_SRC,
  PAYLOAD_CONSTANTS_SRC,
};
