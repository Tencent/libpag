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

import { DROP_TAG_NAMES } from './dom-tags';

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

// PAGX supports `background-image` only when the value is one of the gradient
// functions (linear/radial/conic). `url(...)` backgrounds — which would
// otherwise originate from CSS-in-JS or stylesheets — must be authored as
// <img> elements; drop them silently here so the importer doesn't have to
// warn for every blob the page paints over its decoration layer.
function normalizeBackgroundImage(value) {
  if (!value) return '';
  if (value === 'none') return '';
  if (/url\(/i.test(value)) return '';
  if (/(?:^|\s|,)(?:repeating-)?(?:linear|radial|conic)-gradient\(/i.test(value)) {
    return value;
  }
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
// Skipped for vertical writing-mode spans: PAGX breaks vertical TextBoxes
// by column height, and a forced `white-space: nowrap` would mis-trigger
// the importer's `hasNoWrap` branch (disabling wordWrap on what should be
// a multi-column vertical layout).
function withNowrap(style) {
  if (!style) return 'white-space: nowrap';
  if (/writing-mode:\s*vertical-(rl|lr)/.test(style)) return style;
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
//
// `opacity: 0` and `visibility: hidden` are treated as hidden EXCEPT when the
// element is animated by the capture pass (animation-capture.ts marks such
// elements with a `pagxAnim*` `animation-name`). An element legitimately reads
// opacity=0 / visibility=hidden at the animation's starting phase; dropping it
// here would erase the very subject of the animation (e.g. a
// `0% { opacity: 0 } 100% { opacity: 1 }` fade-in dot), taking its keyframes
// with it. This is the common shape of JS-driven entrances — GSAP's
// `autoAlpha: 0`, for instance, sets BOTH `opacity: 0` and `visibility: hidden`
// on every element waiting to fly in, so the capture-reset frame (timeline at
// t=0) reports the not-yet-entered text as `visibility: hidden`.
function isVisible(computed) {
  if (computed.display === 'none') return false;
  if (computed.visibility === 'hidden' && !hasPagxAnimation(computed)) return false;
  if (parseFloat(computed.opacity) === 0 && !hasPagxAnimation(computed)) return false;
  return true;
}

// True when the element carries a canonical `pagxAnim*` animation injected by
// animation-capture.ts. The capture pass owns the keyframes, so any zero-valued
// channel on the element (opacity, transform out-of-frame) is part of the
// animation's starting phase rather than a static-hidden state.
function hasPagxAnimation(computed) {
  const name = (computed.getPropertyValue('animation-name') || '').split(',')[0].trim();
  return !!name && name !== 'none' && name.indexOf('pagxAnim') === 0;
}

// Read a 4-sided CSS box (`padding-*`, `margin-*`, `border-*-width`, …) into
// a `{ top, right, bottom, left }` object. Tolerates missing / non-numeric
// values by coercing to 0 — `getComputedStyle` always returns a string but
// `parseFloat('')` is NaN, so the `|| 0` is load-bearing for the callers'
// arithmetic.
function readBox(computed, prefix, suffix) {
  const sfx = suffix ? `-${suffix}` : '';
  return {
    top:    parseFloat(computed.getPropertyValue(`${prefix}-top${sfx}`))    || 0,
    right:  parseFloat(computed.getPropertyValue(`${prefix}-right${sfx}`))  || 0,
    bottom: parseFloat(computed.getPropertyValue(`${prefix}-bottom${sfx}`)) || 0,
    left:   parseFloat(computed.getPropertyValue(`${prefix}-left${sfx}`))   || 0,
  };
}

function readPadding(computed) {
  return readBox(computed, 'padding');
}

function readMargin(computed) {
  return readBox(computed, 'margin');
}

function borderWidthOf(computed, side) {
  return parseFloat(computed.getPropertyValue(`border-${side}-width`)) || 0;
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
    return syntheticText(el) ? 'text' : null;
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
  parts.push(`${outProp}: ${v}`);
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

// Emit the `animation-*` longhands for an element animated by the capture pass
// (animation-capture.ts). Only `pagxAnim*` names are forwarded: those are the
// canonical animations whose `@keyframes` the capture pass injected into the
// `<style id="__pagx_anim_keyframes">` block (which snapshotMain copies into
// the output <style>). Any other animation-name refers to a `@keyframes` that
// only existed in the page's original stylesheet — which the snapshot rebuilds
// from scratch and therefore does not carry — so emitting it would dangle.
// The longhands are read straight from computed style; the importer folds them
// back together (spec/html_subset.md §13).
function appendAnimation(parts, computed) {
  const name = (computed.getPropertyValue('animation-name') || '').split(',')[0].trim();
  if (!name || name === 'none' || name.indexOf('pagxAnim') !== 0) return;
  parts.push(`animation-name: ${name}`);
  const longhands = [
    'animation-duration',
    'animation-timing-function',
    'animation-iteration-count',
    'animation-direction',
    'animation-delay',
  ];
  for (const prop of longhands) {
    const v = (computed.getPropertyValue(prop) || '').trim();
    if (v) parts.push(`${prop}: ${v}`);
  }
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
    parts.push(`width: ${px(width)}`);
    parts.push(`height: ${px(height)}`);
    parts.push(`flex-shrink: 0`);
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
  }

  if (opts.text) {
    const textColor = computed.getPropertyValue('color').trim();
    const ctx = { textColor, textColorOverride: opts.textColorOverride };
    for (const entry of STYLE_SCHEMA_TEXT) {
      appendStyleProp(parts, computed, entry, ctx);
    }
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

  // Forward a canonical `pagxAnim*` animation (set by animation-capture.ts) so
  // the importer can map it onto a PAGX animation. Emitted for every element
  // kind so animated containers, text leaves and replaced elements all carry
  // their declaration.
  appendAnimation(parts, computed);

  return parts.join('; ');
}

// Returns an array of HTML strings — one absolutely-positioned <div> per
// non-zero border side — to overlay onto the host box. Used when the element
// has an asymmetric border (e.g. `border-bottom: 1px solid #e5e7eb` on a list
// row).
function borderOverlayHTML(computed, width, height) {
  if (isUniformBorder(computed)) return [];
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

// Walk an SVG subtree and rewrite `currentColor` / `context-fill` /
// `context-stroke` to the computed colour at that node so the snapshot is
// self-contained. The clone is detached from the document, so
// `getComputedStyle` can't resolve it directly; instead we walk the
// *original* subtree (where the cascade is live) and the clone in lockstep,
// propagating per-node `color` down the tree.
function freezeSvg(svgEl, rect) {
  const clone = svgEl.cloneNode(true);
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
  const walk = (orig, dst, inheritedColor) => {
    // SVG elements inherit `color` from their CSS parent. We only re-query
    // getComputedStyle if the node is actually an element (text nodes inside
    // <text> don't carry style on their own).
    let here = inheritedColor;
    if (orig && orig.nodeType === 1) {
      const cs = getComputedStyle(orig);
      if (cs && cs.color) here = cs.color.trim() || here;
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
    }
    const origKids = (orig && orig.children) || [];
    const dstKids = (dst && dst.children) || [];
    const n = Math.min(origKids.length, dstKids.length);
    for (let i = 0; i < n; i++) walk(origKids[i], dstKids[i], here);
  };
  walk(svgEl, clone, fallback);
  return clone.outerHTML;
}

// ===== Multi-line text emission =====

// Split a text node into one entry per visually-rendered line. We use
// Range.getClientRects() to obtain the per-line bounding boxes from the
// browser's own layout, then binary-search the character offsets at which one
// line gives way to the next. This matches Chromium's line-break decisions
// exactly while keeping the work to O(lines · log n) getBoundingClientRect
// calls instead of the O(n) per-character scan the original implementation
// performed.
//
// The host element's `white-space` controls whether embedded whitespace is
// collapsed (`normal`, `nowrap`) or preserved (`pre`, `pre-wrap`, `pre-line`).
// Collapsing in preserve modes would mangle code snippets, indentation, and
// other pre-formatted content.
function splitTextNodeIntoLines(textNode, whiteSpace) {
  const text = textNode.nodeValue || '';
  if (!text.trim()) return [];
  const preserve = typeof whiteSpace === 'string' && whiteSpace.startsWith('pre');
  const cleanLine = (s) => (preserve ? s : s.replace(/\s+/g, ' ').trim());
  const range = document.createRange();
  range.selectNodeContents(textNode);
  const lineRects = Array.from(range.getClientRects());
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

  // Binary-search the first character index whose mid-Y is >= `lineTop`
  // within [lo, hi). When the candidate char has an empty rect (whitespace
  // collapsed at a wrap point) we treat it as below the boundary and step
  // right; this keeps the search O(log n) at the cost of binding collapsed
  // whitespace to whichever side wins the bisection (visually invisible
  // either way).
  //
  // `loStart` lets the caller pass the previously-found boundary as the
  // lower bound, since boundaries are strictly increasing across lines.
  // Without this, every line's search starts from index 0 and re-flushes
  // layout for the leading characters that we already know belong to a
  // higher line — for a 1000-char × 20-line block that's the difference
  // between log2(1000)·20 ≈ 200 layout flushes and roughly half that.
  function findFirstCharAtOrBelow(lineTop, loStart) {
    let lo = loStart;
    let hi = text.length;
    while (lo < hi) {
      const mid = (lo + hi) >>> 1;
      range.setStart(textNode, mid);
      range.setEnd(textNode, mid + 1);
      const r = range.getBoundingClientRect();
      const empty = r.width === 0 && r.height === 0;
      const midY = empty ? -Infinity : r.top + r.height / 2;
      if (empty || midY < lineTop) {
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
    boundaries.push(findFirstCharAtOrBelow(lineRects[k].top, boundaries[k - 1] + 1));
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
    return emitVerticalTextSpan(textNode, parentRect, computed);
  }
  const ws = computed.getPropertyValue('white-space');
  const lines = splitTextNodeIntoLines(textNode, ws);
  const lineHeightPx = parseFloat(computed.getPropertyValue('line-height')) || 0;
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

// Emit a single absolutely-positioned <span> for a text node whose host
// element uses `writing-mode: vertical-rl` or `vertical-lr`. The horizontal
// `splitTextNodeIntoLines` path can't be reused because Chromium's
// `getClientRects()` reports glyph rects in column order under vertical
// writing modes, not line order, so its Y-axis bisection would produce
// nonsense boundaries. Instead we hand the entire text fragment to PAGX as
// one column-shaped span and let `TextLayout::layoutColumns` perform the
// vertical glyph layout itself. The span's bounding box (from the union
// `Range.getBoundingClientRect()`) gives PAGX the column width / total
// height it needs; `writing-mode` propagates through STYLE_SCHEMA's
// `text` scope; `white-space: nowrap` is intentionally NOT forced because
// vertical TextBoxes break columns by height, and a forced nowrap would
// mis-trigger the importer's `hasNoWrap` path.
function emitVerticalTextSpan(textNode, parentRect, computed) {
  const text = textNode.nodeValue || '';
  if (!text.trim()) return [];
  const ws = computed.getPropertyValue('white-space');
  const preserve = typeof ws === 'string' && ws.startsWith('pre');
  const cleaned = preserve ? text : text.replace(/\s+/g, ' ').trim();
  if (!cleaned) return [];
  const range = document.createRange();
  range.selectNodeContents(textNode);
  const rect = range.getBoundingClientRect();
  range.detach && range.detach();
  if (!rect || (rect.width === 0 && rect.height === 0)) return [];
  const tl = rect.left - parentRect.left;
  const tt = rect.top - parentRect.top;
  const style = buildStyle(tl, tt, rect.width, rect.height, computed, {
    box: false, text: true,
  });
  return [`<span style="${style}">${escapeHtml(applyTextTransform(cleaned, computed))}</span>`];
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
  const fontSize = parseFloat(computed.getPropertyValue('font-size')) || 0;
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
  return true;
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
    const fs = parseFloat(computed.getPropertyValue('font-size')) || 0;
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
      const parts = [textStyle, 'flex-shrink: 0'].filter(Boolean);
      return `<span style="${parts.join('; ')}">${escapeHtml(applyTextTransform(directText, computed))}</span>`;
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
  const lineHeightPx = parseFloat(parentComputed.getPropertyValue('line-height')) || 0;
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
  const childParts = [];
  for (const child of flexChildren) {
    if (child.kind === 'text') {
      childParts.push(renderFlexTextItem(child, computed));
    } else {
      // Pass the cached computed style so render() does not have to ask
      // the engine again for the same node.
      childParts.push(render(child.node, rect, { flexItem: true }, child.computed));
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
  // Cancel every running CSS animation so the snapshot measures the element
  // in its base (un-animated) state. Without this, `direction: reverse` and
  // any other non-zero starting phase would shift the element away from its
  // CSS-declared position by the time `getBoundingClientRect()` runs — and
  // the same offset would also surface through `getComputedStyle(.).transform`,
  // so both the `top/left` and the static `transform: matrix(...)` written
  // into the subset would carry the animation offset twice (once baked into
  // the rect, once forwarded as a static transform). The PAGX importer then
  // adds the keyframe channel on top, tripling the offset. CSS animations
  // surface as WAAPI `Animation` objects via `getAnimations()`; calling
  // `cancel()` on each removes the effect from computed style without
  // touching the stylesheet rules, so longhand reads (`animation-name`,
  // `-direction`, `-duration`, …) still resolve correctly when the snapshot
  // walker emits them.
  //
  // The canonical `pagxAnim*` animations the capture layer just installed are
  // cancelled too: the shorthand was written into the element's *inline*
  // `style.animation` (animation-capture.ts), and `cancel()` only stops the
  // running WAAPI effect — it does not erase the inline declaration, so
  // `getComputedStyle(.).animation-name/-direction/-duration/…` still resolve
  // to the captured longhand when `appendAnimation()` later serialises the
  // element. Skipping them here was the source of a "double-write" bug:
  // `direction: reverse` keeps the element pinned at its 100% keyframe (e.g.
  // `translateY(120px)` + `opacity: 0`), so `getBoundingClientRect()` and
  // `getComputedStyle().transform` both reported the offset position and the
  // subset emitted the same translation twice (once as `top`, once as
  // `transform: matrix(...)`).
  try {
    if (typeof document.getAnimations === 'function') {
      const all = document.getAnimations();
      for (let i = 0; i < all.length; i++) {
        try {
          all[i].cancel();
        } catch (_) { /* ignore per-anim failure */ }
      }
    }
  } catch (_) { /* ignore */ }
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
  let styleBlock =
    'div,span,img,svg,body{box-sizing:border-box}' +
    'html{min-height:100vh;display:flex;justify-content:center;' +
    'align-items:flex-start;padding:32px 0}' +
    'body{margin:0;padding:0;position:relative;flex-shrink:0}';

  // animation-capture.ts (run before this snapshot) stashes the canonical
  // `@keyframes` it synthesised in a dedicated <style id="__pagx_anim_keyframes">.
  // Fold them into the single output <style> so the importer's
  // StyleSheetCollectorPass picks them up alongside the `animation-*` longhands
  // that appendAnimation() forwarded onto each element. The block is appended
  // (not prepended) so element selectors above keep their precedence.
  const animKeyframes = document.getElementById('__pagx_anim_keyframes');
  if (animKeyframes && animKeyframes.textContent) {
    styleBlock += animKeyframes.textContent.trim();
  }

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
const DROP_TAGS = new Set(${JSON.stringify(DROP_TAG_NAMES)});

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
  normalizeWritingMode,
  roundPx,
  px,
  escapeHtml,
  joinStyles,
  withNowrap,
  paddingShorthand,
  nonZero,
  hasPagxAnimation,
  isVisible,
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
  appendAnimation,
  buildStyle,
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
  emitVerticalTextSpan,
  hasBoxVisualsForInline,
  multiplyColorAlpha,
  resolveInlineTextValue,
  isInlineRunChild,
  isInlineTextLeafCandidate,
  emitInlineRunMarkup,
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
  HELPERS_SRC,
  PAYLOAD_CONSTANTS_SRC,
};
