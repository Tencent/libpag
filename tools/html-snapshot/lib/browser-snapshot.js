// Browser-side payload for html-snapshot.
//
// Both functions exported here are serialised by Puppeteer's `page.evaluate`
// and run inside the headless Chromium context. They MUST remain
// self-contained closures: any reference to a Node module, a binding from
// this file's outer scope, or another exported helper would arrive in the
// browser as `undefined`. Treat the function bodies as a single in-page
// program that happens to live in two top-level functions for readability.

'use strict';

/* eslint-disable no-undef, no-inner-declarations */

// Walk every <img> in the document and, for any whose `src` is an absolute
// `http(s)://` URL, fetch the bytes and stash a `data:` URI on
// `data-snapshot-src`. The downstream snapshot reader (`imgSrc`) prefers that
// attribute, so the emitted HTML becomes self-contained — required because
// `pagx render` cannot fetch URLs at render time. Failures (CORS, network,
// 404) are logged and left in place: the snapshot will then fall back to the
// original URL and render as an empty box rather than aborting the pipeline.
async function inlineExternalImages() {
  // Convert a Blob into a `data:` URI without the FileReader callback dance.
  // Reading bytes via `arrayBuffer()` + chunked btoa avoids the "Maximum call
  // stack size" trap that String.fromCharCode(...big) hits on large images.
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

  const imgs = Array.from(document.querySelectorAll('img'));
  await Promise.all(imgs.map(async (img) => {
    const src = img.currentSrc || img.src || img.getAttribute('src') || '';
    if (!src) return;
    if (src.startsWith('data:')) return;
    if (!/^https?:/i.test(src)) return;
    try {
      const res = await fetch(src, { mode: 'cors', credentials: 'omit' });
      if (!res.ok) {
        console.warn(`html-snapshot: image fetch ${res.status} for ${src}`);
        return;
      }
      const dataUri = await blobToDataUri(await res.blob());
      img.setAttribute('data-snapshot-src', dataUri);
    } catch (err) {
      console.warn(`html-snapshot: failed to inline ${src}: ${err && err.message}`);
    }
  }));
}

function takeSnapshot(config) {
  // Tags we never emit. Their subtree is also dropped. The list mirrors the subset spec
  // (anything outside `spec/html_subset.md` §2 has no PAGX mapping) plus the obvious
  // non-visual / metadata tags. `<br>` and `<hr>` are dropped because the visual line
  // breaks they introduce are captured by `Range.getClientRects()` on the surrounding
  // text nodes — re-emitting them would either duplicate the break or create stray
  // 0-height boxes.
  const DROP_TAGS = new Set([
    'script', 'style', 'link', 'meta', 'noscript',
    'iframe', 'object', 'embed', 'video', 'audio', 'canvas',
    'br', 'hr', 'wbr',
    'head', 'title', 'base',
    'template', 'slot', 'dialog', 'details', 'summary',
    'map', 'area', 'source', 'track', 'param',
    'form',  // visual styling lives on its children; <form> itself is structural
  ]);

  // <input> types that have no static text representation. Anything else (`text`,
  // `email`, `password`, `search`, `url`, `tel`, `number`, `submit`, `button`, `reset`,
  // empty/missing → defaults to `text`) is kept and surfaces its value/placeholder/label.
  const NON_TEXT_INPUT_TYPES = new Set([
    'checkbox', 'radio', 'file', 'range', 'color', 'image', 'hidden',
    'date', 'datetime-local', 'month', 'time', 'week',
  ]);

  // Border / padding sides in the canonical CSS order. Iterated in this exact order
  // by isUniformBorder, hasAnyBorder, and borderOverlayHTML; do not reorder.
  const SIDES = ['top', 'right', 'bottom', 'left'];

  // Per-side overlay rectangle factory: given the host box (w, h) and the border
  // thickness (t) on this side, return the (left, top, w, h) of the 1-axis stripe
  // that paints the border. Used by borderOverlayHTML to replace a 4-way `if`.
  const SIDE_OVERLAY = {
    top:    (w, h, t) => ({ left: 0,                     top: 0,                     w: w, h: t }),
    bottom: (w, h, t) => ({ left: 0,                     top: Math.max(0, h - t),    w: w, h: t }),
    left:   (w, h, t) => ({ left: 0,                     top: 0,                     w: t, h: h }),
    right:  (w, h, t) => ({ left: Math.max(0, w - t),    top: 0,                     w: t, h: h }),
  };

  // Subset-allowed values for flex container properties. Match
  // spec/html_subset.md §4.1 strictly: anything outside these is silently dropped
  // so the flex container still wins. NOTE: emitting an unsupported value
  // (e.g. `align-items: baseline`) makes the importer's flex-hint validator
  // strip the entire `display: flex` declaration, which cascades into the
  // children collapsing to (0,0). Be conservative.
  const ALIGN_ITEMS_OK = new Set([
    'stretch', 'center', 'flex-start', 'flex-end',
  ]);
  const JUSTIFY_CONTENT_OK = new Set([
    'flex-start', 'flex-end', 'center',
    'space-between', 'space-around', 'space-evenly',
  ]);
  // Computed `align-items` values that map cleanly to a subset value. CSS
  // specifies that `normal` resolves to `stretch` for flex containers, so the
  // visible behaviour matches even though the property name differs.
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

  function simplifyFontFamily(value) {
    // Strip quotes and take only the first family. Tailwind/system defaults expand to
    // 5+ families with embedded "double quotes" which would otherwise break the
    // surrounding style="…" attribute.
    if (!value) return '';
    const first = value.split(',')[0].trim();
    return first.replace(/^["']|["']$/g, '');
  }

  function normalizeOverflow(value) {
    if (!value) return '';
    // PAGX only models `overflow: hidden`. `clip`, `auto`, and `scroll` all map to
    // hidden (closest visual approximation — no scrollbars / no painting outside).
    if (value === 'clip' || value === 'auto' || value === 'scroll') return 'hidden';
    return value;
  }

  // Chromium reports `border-radius` in its long-form (`T R B L`). The subset only
  // accepts a single value, and authoring the long form (especially `0px 0px 0px 0px`)
  // pollutes the diff. Collapse `T R B L` → single value when all four lengths agree,
  // and drop entirely when they all collapse to zero. Mixed values are passed through
  // verbatim and left to the importer to handle.
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

  // Master list of visual properties forwarded onto kept elements. The schema is
  // walked in declaration order, so the emit order in the output style attribute
  // matches the schema order — keep box-scope entries first, then text-scope
  // entries. Each entry can carry:
  //   - defaults:                values that match a CSS default and should be skipped;
  //   - normalize:               value transformer (return '' to drop);
  //   - outProp:                 declaration name to emit (defaults to `prop`);
  //   - skipIfEqualsTextColor:   suppress when the value resolves to the element's color
  //                              (used to drop redundant text-decoration-color).
  const STYLE_SCHEMA = [
    // Box-scope (background / decoration). Emitted on container & leaf wrappers.
    { prop: 'background-color', scope: 'box',  defaults: ['rgba(0, 0, 0, 0)', 'transparent'] },
    { prop: 'background-image', scope: 'box',  defaults: ['none'], normalize: normalizeBackgroundImage },
    // Chromium's computed `border-radius` is the long-form `T R B L` even when
    // authored as a single value; allow both shapes through the default filter.
    { prop: 'border-radius',    scope: 'box',  defaults: ['0px', '0px 0px 0px 0px'], normalize: normalizeBorderRadius },
    { prop: 'overflow',         scope: 'box',  defaults: ['visible'], normalize: normalizeOverflow },
    { prop: 'opacity',          scope: 'box',  defaults: ['1'] },
    { prop: 'filter',           scope: 'box',  defaults: ['none'] },
    { prop: 'backdrop-filter',  scope: 'box',  defaults: ['none'] },
    { prop: 'mix-blend-mode',   scope: 'box',  defaults: ['normal'] },
    // Text-scope. Emitted on text leaves and on inline text children of mixed containers.
    { prop: 'color',                 scope: 'text', defaults: ['rgb(0, 0, 0)'] },
    { prop: 'font-family',           scope: 'text', normalize: simplifyFontFamily },
    { prop: 'font-size',             scope: 'text' },
    { prop: 'font-weight',           scope: 'text', defaults: ['400', 'normal'] },
    { prop: 'font-style',            scope: 'text', defaults: ['normal'] },
    { prop: 'letter-spacing',        scope: 'text', defaults: ['normal', '0px'] },
    { prop: 'line-height',           scope: 'text' },
    { prop: 'text-align',            scope: 'text', defaults: ['start', 'left'] },
    { prop: 'text-decoration-line',  scope: 'text', defaults: ['none'], outProp: 'text-decoration' },
    // text-decoration-color defaults to `currentColor`; we additionally suppress it
    // when its computed value already matches the element's `color`, since CSS
    // would render them identically and the duplicate adds noise.
    { prop: 'text-decoration-color', scope: 'text', defaults: ['currentcolor'], skipIfEqualsTextColor: true },
    { prop: 'white-space',           scope: 'text', defaults: ['normal'] },
  ];

  const STYLE_SCHEMA_BY_PROP = new Map(STYLE_SCHEMA.map((e) => [e.prop, e]));
  const STYLE_SCHEMA_BOX = STYLE_SCHEMA.filter((e) => e.scope === 'box');
  const STYLE_SCHEMA_TEXT = STYLE_SCHEMA.filter((e) => e.scope === 'text');

  // Strip from text leaves whose computed value equals the CSS default — keeps the
  // output close to the hand-authored subset shape.
  function isDefault(prop, value) {
    const e = STYLE_SCHEMA_BY_PROP.get(prop);
    return e && e.defaults ? e.defaults.includes(value) : false;
  }

  function normalizeValue(prop, value) {
    const e = STYLE_SCHEMA_BY_PROP.get(prop);
    return e && e.normalize ? e.normalize(value) : value;
  }

  // Append a single schema-driven CSS declaration to `parts` if its computed
  // value isn't a default and survives normalisation. `ctx.textColor` (when
  // provided) is consulted by entries flagged `skipIfEqualsTextColor`.
  function appendStyleProp(parts, computed, entry, ctx) {
    const raw = computed.getPropertyValue(entry.prop).trim();
    const v = entry.normalize ? entry.normalize(raw) : raw;
    if (!v) return;
    if (entry.defaults && entry.defaults.includes(v)) return;
    if (entry.skipIfEqualsTextColor && ctx && v === ctx.textColor) return;
    const outProp = entry.outProp || entry.prop;
    parts.push(`${outProp}: ${v}`);
  }

  // Forward a uniform `border` shorthand. Asymmetric borders are emitted as
  // overlay rectangles by borderOverlayHTML(); see that comment for the trade-off.
  function appendBorder(parts, computed) {
    if (!isUniformBorder(computed)) return;
    const bw = borderWidthOf(computed, 'top');
    if (bw <= 0) return;
    const bc = computed.getPropertyValue('border-top-color').trim();
    parts.push(`border: ${px(bw)} solid ${bc}`);
  }

  // Pass `box-shadow` through verbatim if non-none.
  function appendBoxShadow(parts, computed) {
    const shadow = computed.getPropertyValue('box-shadow').trim();
    if (shadow && shadow !== 'none') parts.push(`box-shadow: ${shadow}`);
  }

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

  // Join non-empty CSS declarations with `; ` so callers don't have to guard against
  // empty fragments (e.g. when the box style is empty under a flex parent).
  function joinStyles(...parts) {
    return parts.filter(Boolean).join('; ');
  }

  // Append `white-space: nowrap` to a text style unless it already declares
  // white-space. Used in every spot where a single-line text span sits inside
  // a flex item or rides on a wrapper whose width was measured by Chromium —
  // prevents PAGX's text engine from re-wrapping at a sub-pixel divergence.
  function withNowrap(style) {
    if (!style) return 'white-space: nowrap';
    return /white-space:/.test(style) ? style : `${style}; white-space: nowrap`;
  }

  // Centralised visibility check. CSS `display: contents` is intentionally NOT
  // considered hidden here — it generates no box of its own but its children
  // still render, and the snapshot walker handles the pass-through separately.
  function isVisible(computed) {
    if (computed.display === 'none') return false;
    if (computed.visibility === 'hidden') return false;
    if (parseFloat(computed.opacity) === 0) return false;
    return true;
  }

  function readPadding(computed) {
    return {
      top:    parseFloat(computed.getPropertyValue('padding-top'))    || 0,
      right:  parseFloat(computed.getPropertyValue('padding-right'))  || 0,
      bottom: parseFloat(computed.getPropertyValue('padding-bottom')) || 0,
      left:   parseFloat(computed.getPropertyValue('padding-left'))   || 0,
    };
  }

  function borderWidthOf(computed, side) {
    return parseFloat(computed.getPropertyValue(`border-${side}-width`)) || 0;
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

  function firstTextNodeChild(el) {
    for (const n of el.childNodes) {
      if (n.nodeType === Node.TEXT_NODE) return n;
    }
    return null;
  }

  // Style of the inner <img> element. Under absolute mode it pins itself to the
  // wrapper's (0,0) and inherits the wrapper's size; under flex mode the wrapper
  // is no longer absolutely positioned, so the image fills the wrapper via flow
  // sizing instead.
  function imageInnerStyle(rect, flexItem) {
    if (flexItem) return `width: 100%; height: 100%`;
    return `position: absolute; left: 0px; top: 0px; ` +
      `width: ${px(rect.width)}; height: ${px(rect.height)}`;
  }

  // Walk an SVG subtree and rewrite `currentColor` / `context-fill` / `context-stroke`
  // to the computed colour at that node so the snapshot is self-contained. The clone is
  // detached from the document, so `getComputedStyle` can't resolve it directly; instead
  // we walk the *original* subtree (where the cascade is live) and the clone in
  // lockstep, propagating per-node `color` down the tree.
  function freezeSvg(svgEl, rect) {
    const clone = svgEl.cloneNode(true);
    // Inline SVGs in real pages usually rely on CSS classes (e.g. Tailwind `w-4 h-4`)
    // to set their on-screen size and leave the `<svg>` element itself without
    // `width`/`height` attributes. PAGX's import pipeline does not interpret those
    // classes, so the resulting SVG would fall back to its viewBox extents (typically
    // 24×24) and get clipped by its parent wrapper. Pin the dimensions here from the
    // browser's measured rect so the importer scales the viewBox into the visible box.
    if (rect && Number.isFinite(rect.width) && Number.isFinite(rect.height)) {
      clone.setAttribute('width', String(Math.round(rect.width * 1000) / 1000));
      clone.setAttribute('height', String(Math.round(rect.height * 1000) / 1000));
    }
    const fallback = (getComputedStyle(svgEl).color || 'rgb(0, 0, 0)').trim();
    const walk = (orig, dst, inheritedColor) => {
      // SVG elements inherit `color` from their CSS parent. We only re-query
      // getComputedStyle if the node is actually an element (text nodes inside <text>
      // don't carry style on their own).
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

  function gatherDirectText(el) {
    let s = '';
    for (const n of el.childNodes) {
      if (n.nodeType === Node.TEXT_NODE) s += n.nodeValue;
    }
    return s.replace(/\s+/g, ' ').trim();
  }

  // Split a text node into one entry per visually-rendered line. We use
  // Range.getClientRects() to obtain the per-line bounding boxes from the browser's
  // own layout, then walk char-by-char and assign each char to the line whose
  // vertical band contains it. This matches Chromium's line-break decisions
  // exactly, so PAGX never has to re-wrap (which would diverge by a sub-pixel and
  // either overflow `overflow: hidden` or wrap "1.5w" into two lines).
  //
  // The host element's `white-space` controls whether embedded whitespace is collapsed
  // (`normal`, `nowrap`) or preserved (`pre`, `pre-wrap`, `pre-line`). Collapsing in
  // preserve modes would mangle code snippets, indentation, and other pre-formatted
  // content.
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
    const buckets = lineRects.map((r) => ({ rect: r, chars: [] }));
    for (let i = 0; i < text.length; i++) {
      range.setStart(textNode, i);
      range.setEnd(textNode, i + 1);
      const r = range.getBoundingClientRect();
      // Empty rect = whitespace collapsed at a line edge — skip.
      if (r.width === 0 && r.height === 0) continue;
      const midY = r.top + r.height / 2;
      let target = 0;
      for (let j = 0; j < buckets.length; j++) {
        if (midY >= buckets[j].rect.top && midY <= buckets[j].rect.bottom) {
          target = j;
          break;
        }
      }
      buckets[target].chars.push(text[i]);
    }
    range.detach && range.detach();
    return buckets
      .map((b) => ({
        text: cleanLine(b.chars.join('')),
        rect: b.rect,
      }))
      .filter((b) => b.text);
  }

  // Emit one absolutely-positioned, nowrap <span> per line of `textNode`,
  // positioned relative to `parentRect`. When the line's measured glyph rect
  // is shorter than the computed `line-height` (Chromium's `getClientRects`
  // reports glyph bounds for inline text, not the line box), expand each
  // line span vertically to the full line-height and re-centre it around
  // the original glyph mid-line. Without this PAGX's text engine, whose
  // baseline / leading model differs from Chromium's by sub-pixel amounts,
  // ends up rendering the final line a hair below the span's declared
  // bottom — invisible on Latin block text but clipping descenders and
  // bottom-heavy CJK strokes inside a `line-clamp` wrapper whose own height
  // was pinned to exactly N × line-height. Same shift is applied to the
  // line's top so the visible glyph centre still lines up with Chromium's
  // measurement; the per-line stride (top-to-top) is already line-height,
  // so consecutive expanded spans tile cleanly without overlap.
  function emitTextSpans(textNode, parentRect, computed) {
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
      }
      const tl = line.rect.left - parentRect.left;
      const tt = lt - parentRect.top;
      const base = buildStyle(tl, tt, line.rect.width, lh, computed, {
        box: false, text: true,
      });
      // Force nowrap on every line span so PAGX's text engine never re-breaks.
      out.push(`<span style="${withNowrap(base)}">${escapeHtml(line.text)}</span>`);
    }
    return out;
  }

  function elementHasChildren(el) {
    for (const c of el.children) {
      if (c.nodeType === Node.ELEMENT_NODE) return true;
    }
    return false;
  }

  function nonZero(rect) {
    return rect.width > 0 && rect.height > 0;
  }

  // True iff all four borders have the same non-zero width, the same `solid` style,
  // and the same color — i.e. the only case where the subset's single `border`
  // shorthand can losslessly express the page's border.
  function isUniformBorder(computed) {
    const w0 = computed.getPropertyValue('border-top-width');
    const s0 = computed.getPropertyValue('border-top-style');
    const c0 = computed.getPropertyValue('border-top-color');
    if (s0 !== 'solid') return false;
    if ((parseFloat(w0) || 0) <= 0) return false;
    for (const side of SIDES) {
      if (computed.getPropertyValue(`border-${side}-width`) !== w0) return false;
      if (computed.getPropertyValue(`border-${side}-style`) !== s0) return false;
      if (computed.getPropertyValue(`border-${side}-color`) !== c0) return false;
    }
    return true;
  }

  function hasAnyBorder(computed) {
    for (const side of SIDES) {
      if (borderWidthOf(computed, side) > 0) return true;
    }
    return false;
  }

  // Returns an array of HTML strings — one absolutely-positioned <div> per non-zero
  // border side — to overlay onto the host box. Used when the element has an
  // asymmetric border (e.g. `border-bottom: 1px solid #e5e7eb` on a list row).
  function borderOverlayHTML(computed, width, height) {
    if (isUniformBorder(computed)) return [];
    const out = [];
    for (const side of SIDES) {
      const t = borderWidthOf(computed, side);
      if (t <= 0) continue;
      const style = computed.getPropertyValue(`border-${side}-style`);
      // The subset has no model for dashed/dotted/double per-side borders. Downgrade
      // them to solid: it's an approximation but preserves the visual divider, which
      // is what the page actually wanted.
      if (style === 'none' || style === 'hidden') continue;
      const color = computed.getPropertyValue(`border-${side}-color`).trim();
      const r = SIDE_OVERLAY[side](width, height, t);
      out.push(`<div style="position: absolute; left: ${px(r.left)}; top: ${px(r.top)}; ` +
        `width: ${px(r.w)}; height: ${px(r.h)}; background-color: ${color}"></div>`);
    }
    return out;
  }

  // Build the style string for a kept element. `opts.box` includes background/border/
  // shadow/etc.; `opts.text` includes color/font-*; `opts.positioned` toggles the
  // absolute-positioning header (off for inline text children that ride on their
  // parent's box). `opts.flexItem` swaps the absolute header for plain `width/height`
  // so the element can participate in a parent flex flow without being taken out of
  // it (PAGX's flex engine ignores items with `position: absolute` by spec; mixing
  // them with flex siblings collapses the parent back to absolute on import).
  //
  // Flex items also receive `position: relative`. Their subtree contains
  // `position: absolute` descendants (every non-flex-item kept element is emitted
  // as absolute), and without a positioned ancestor those descendants would
  // escape the flex item entirely when the snapshot is opened in a real browser
  // — they would anchor to whichever positioned ancestor lives higher up the
  // tree, collapsing every card's content into a single overlapping cluster.
  // The PAGX importer treats `position: relative` as a no-op (PAGX has no
  // containing-block concept; all children anchor to their direct parent), so
  // the declaration is dropped during import and the flex layout is preserved.
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
      // Border / shadow are emitted between the box-scope and text-scope blocks so
      // the resulting declaration order matches the historical hand-written layout.
      // See appendBorder / appendBoxShadow for the per-helper rationale.
      appendBorder(parts, computed);
      appendBoxShadow(parts, computed);
    }

    if (opts.text) {
      const textColor = computed.getPropertyValue('color').trim();
      for (const entry of STYLE_SCHEMA_TEXT) {
        appendStyleProp(parts, computed, entry, { textColor });
      }
    } else if (opts.colorOnly) {
      // Icon wrappers (host of an inline <svg>) only need `color` so that any
      // currentColor stroke/fill picks up the right tint.
      appendStyleProp(parts, computed, STYLE_SCHEMA_BY_PROP.get('color'));
    }

    return parts.join('; ');
  }

  // Resolve the <img>'s source for the snapshot output. We prefer the inlined data
  // URI produced by the pre-snapshot `inlineExternalImages` pass: PAGX's renderer
  // only knows how to load local files and `data:` URIs, so leaving an `http(s)://`
  // src would render as an empty box. Local/relative paths and `data:` URIs already
  // work out of the box and are kept verbatim.
  function imgSrc(el) {
    const inlined = el.getAttribute('data-snapshot-src');
    if (inlined) return inlined;
    return el.getAttribute('src') || '';
  }

  // Synthesize text content for form elements (placeholder / value / button label).
  // For checkboxes / radios / file pickers / colour swatches / date pickers there is
  // no meaningful text to surface; their `value` attribute is metadata (`"on"`,
  // hidden file path, ISO date) that would only confuse downstream rendering, so we
  // return empty and let `classify` drop the element.
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

  // Decide whether to keep an element. Returns null to drop it.
  function classify(el, computed) {
    const tag = el.tagName.toLowerCase();
    if (DROP_TAGS.has(tag)) return null;
    if (!isVisible(computed)) return null;
    if (tag === 'svg') return 'svg';
    if (tag === 'img') return 'img';
    if (tag === 'input' || tag === 'textarea' || tag === 'select') {
      return syntheticText(el) ? 'text' : null;
    }
    return 'box';
  }

  // Emit every visible child of `el` (elements + non-empty text nodes) into a single
  // HTML string, replaying browser paint order (flow-then-positioned, both in DOM
  // order). Used by both the regular container path and by `display: contents` passes
  // where the host element itself is suppressed.
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

  // Replay CSS paint order: non-positioned (flow) children first, then
  // positioned children sorted by z-index ascending, with DOM order as
  // tie-break. Ignoring z-index lets a sibling that was authored earlier
  // in the DOM but assigned a higher z-index (e.g. an iOS notch with
  // `z-index: 50`) be hidden by a later sibling at `z-index: auto`.
  function paintOrder(a, b) {
    if (a.positioned !== b.positioned) return a.positioned ? 1 : -1;
    if (a.positioned && a.zIndex !== b.zIndex) return a.zIndex - b.zIndex;
    return a.domIndex - b.domIndex;
  }

  function renderChildrenInto(el, parentRect) {
    const items = [];
    let domIndex = 0;
    const hostComputed = getComputedStyle(el);
    for (const n of el.childNodes) {
      if (n.nodeType === Node.ELEMENT_NODE) {
        const childComputed = getComputedStyle(n);
        const isPositioned = childComputed.position !== 'static';
        items.push({
          positioned: isPositioned,
          zIndex: isPositioned ? resolveZIndex(childComputed) : 0,
          domIndex: domIndex++,
          html: render(n, parentRect),
        });
      } else if (n.nodeType === Node.TEXT_NODE && n.nodeValue && n.nodeValue.trim()) {
        const spans = emitTextSpans(n, parentRect, hostComputed);
        if (spans.length > 0) {
          items.push({
            positioned: false,
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

  // Derives the subset-allowed gap value for a flex container along its main axis.
  // CSS 'gap' is a 2-axis property; for `flex-direction: row` we want the column-gap
  // (between siblings horizontally), for `column` we want the row-gap. Values that
  // can't be parsed back into px are returned as ''.
  function flexMainGapPx(computed, direction) {
    const colRaw = computed.getPropertyValue('column-gap');
    const rowRaw = computed.getPropertyValue('row-gap');
    const main = direction === 'column' ? rowRaw : colRaw;
    const v = parseFloat(main);
    if (!isFinite(v) || v <= 0) return '';
    return `${roundPx(v)}px`;
  }

  // Reads the live flex configuration of `el` and serialises it to a subset-shaped
  // CSS string. Only the properties documented in spec/html_subset.md §4.1 are
  // emitted — anything outside the allowed value set is silently dropped so the
  // importer doesn't have to log warnings for normal-default values like
  // `align-items: normal`.
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
    // 'pre*' whitespace modes preserve line breaks regardless of container width
    // so multi-line is intentional, not a wrap; still bail since we can't emit
    // multiple inline lines without abs spans.
    return ws !== '' || rects.length > 1;
  }

  // Returns the visible children of `el` that would render as flex items, or
  // null when the element cannot safely be emitted as a flex container under
  // the PAGX subset. Each returned entry is either
  //   { kind: 'element', node: HTMLElement } — normal flex item, or
  //   { kind: 'text', node: TextNode, rect: DOMRect } — an anonymous flex
  //     item that wraps a bare text-node child (the CSS spec wraps these in
  //     an anonymous box; PAGX has no concept of anonymous boxes, so we
  //     emit an explicit <span> flex item for the text and use its measured
  //     rect for sizing).
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
      out.push({ kind: 'element', node: c });
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
  // Tolerance is tight (1.5px) to catch real margin/shrink mismatches but loose
  // enough to absorb sub-pixel rounding from `getBoundingClientRect`.
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
    const rects = children.map((c) => (
      c.kind === 'text' ? c.rect : c.node.getBoundingClientRect()
    ));
    const TOLERANCE = 1.5;
    for (let i = 0; i + 1 < rects.length; i++) {
      const a = rects[i];
      const b = rects[i + 1];
      const measuredGap = isRow ? (b.left - a.right) : (b.top - a.bottom);
      if (Math.abs(measuredGap - declaredGap) > TOLERANCE) return false;
    }
    return true;
  }

  // Returns a curated list of flex children when `el`'s computed style declares a
  // subset-friendly flex configuration AND its border/wrapping/child shape don't
  // require absolute positioning. `null` means "render as absolute".
  function classifyFlexContainer(el, computed) {
    const display = computed.display;
    if (display !== 'flex' && display !== 'inline-flex') return null;
    const wrap = computed.getPropertyValue('flex-wrap').trim();
    if (wrap && wrap !== 'nowrap') return null;
    const rect = el.getBoundingClientRect();
    if (!nonZero(rect)) return null;
    // Asymmetric borders are baked into per-side overlay rectangles by
    // borderOverlayHTML(). Those rectangles use `position: absolute` and would not
    // anchor correctly inside a non-positioned flex parent, so bail.
    if (!isUniformBorder(computed) && hasAnyBorder(computed)) return null;
    const children = flexItemChildren(el);
    if (!children) return null;
    if (!isFlexLayoutFaithful(children, computed)) return null;
    return children;
  }

  // Render an element + its subtree as a flex container. The container itself is
  // either positioned absolutely against `parentRect` (default) or sized as a flex
  // item against an outer flex parent (when `opts.flexItem` is true). Element
  // children recurse through `render` with `flexItem: true`; anonymous text-node
  // children are wrapped in a sized `<span>` flex item — the text inherits the
  // container's `color`/`font-*` cascade, so the wrapper only needs the explicit
  // dimensions to participate in flex layout (PAGX's flex engine then aligns it
  // against its siblings via the container's `align-items` setting).
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
        childParts.push(render(child.node, rect, { flexItem: true }));
      }
    }
    return `<div style="${style}">${childParts.join('')}</div>`;
  }

  // Emit a bare text-node child of a flex container as a sized <span> flex
  // item. The text inherits color/font-* from the container's computed style;
  // we only need to forward the inherited text props plus the explicit
  // dimensions (so the flex engine can place it). `withNowrap` is mandatory:
  // the measured rect was for a single line and PAGX's text engine must not
  // rewrap at a sub-pixel divergence.
  function renderFlexTextItem(child, parentComputed) {
    const r = child.rect;
    const text = (child.node.nodeValue || '').replace(/\s+/g, ' ').trim();
    const baseStyle = buildStyle(0, 0, r.width, r.height, parentComputed, {
      box: false, text: true, flexItem: true,
    });
    return `<span style="${withNowrap(baseStyle)}">${escapeHtml(text)}</span>`;
  }

  // SVG icon: emit a wrapper carrying only `color` (so currentColor strokes/fills
  // resolve correctly) plus the frozen SVG markup.
  //
  // The wrapper has explicit width/height pinned to the SVG's measured rect and
  // `overflow: hidden`. Without further styling the inner <svg> renders as an
  // inline replaced element on a line box whose height is the inherited
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

  // <img>: wrapper + nested <img> sized to fill it. Asymmetric borders are baked
  // into overlay rectangles painted on top.
  function renderImg(el, parentRect, rect, left, top, computed, opts) {
    const src = imgSrc(el);
    const alt = escapeHtml(el.getAttribute('alt') || '');
    const imgStyle = imageInnerStyle(rect, opts.flexItem);
    const wrapperStyle = buildStyle(left, top, rect.width, rect.height, computed, {
      box: true, ...opts,
    });
    const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
    return `<div style="${wrapperStyle}"><img src="${escapeHtml(src)}" alt="${alt}" style="${imgStyle}"/>${overlays}</div>`;
  }

  // <input> / <textarea> / <select>: outer box for bg/border/radius + inner span
  // sitting inside the padding box. `<input>` paints its background across the
  // full element rect, but the actual text sits inside the padding box; emitting
  // them as one rect would smear text over absolute siblings (e.g. the search
  // icon at `left: 16px` inside the same wrapper).
  //
  // Native form controls vertically centre their value/placeholder inside the
  // padding box — that's a UA behaviour, not something we can read off computed
  // style. A bare abs-positioned inner span at `top: pad.top` therefore drops
  // short single-line text against the top of the wrapper (search bars in
  // 40px-tall pills, "Email" labels in 48px login fields). Emit the wrapper as
  // a flex container with `align-items: center` and the input's padding so the
  // inner span rides centred regardless of `opts.flexItem` — the box still
  // anchors via `position: absolute` (or `relative` when it's itself a flex
  // item), so absolute overlay siblings keep their positioning context.
  // `white-space: nowrap` is mandatory so PAGX's text engine doesn't re-wrap a
  // single-line value to two lines when its intrinsic width exceeds the
  // wrapper by sub-pixel.
  function renderTextInput(el, parentRect, rect, left, top, computed, opts) {
    const text = syntheticText(el);
    const pad = readPadding(computed);
    const placeholderColor = (!el.value && el.getAttribute('placeholder'))
      ? getComputedStyle(el, '::placeholder').getPropertyValue('color').trim()
      : '';
    const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
      box: true, ...opts,
    });
    const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
    const padShort = paddingShorthand(pad.top, pad.right, pad.bottom, pad.left);
    let textStyle = buildStyle(0, 0, 0, 0, computed, {
      box: false, text: true, positioned: false,
    });
    if (placeholderColor) {
      textStyle = textStyle.replace(/(^|; )color: [^;]+/, `$1color: ${placeholderColor}`);
    }
    const finalTextStyle = withNowrap(textStyle);
    const inner = `display: flex; align-items: center` + (padShort === '0px' ? '' : `; padding: ${padShort}`);
    const composed = joinStyles(boxStyle, inner);
    return `<div style="${composed}"><span style="${finalTextStyle}">${escapeHtml(text)}</span>${overlays}</div>`;
  }

  // Pure-text leaf (no element children): emit one outer <div> carrying the box
  // styles plus one nowrap <span> per visually-rendered line. Each line span is
  // placed at the text's *actual* rendered rect (from Range.getClientRects), not
  // stretched to fill the parent box — otherwise the text inherits the parent
  // height and clings to the top of the container instead of sitting where the
  // browser's flex/baseline layout placed it (e.g. <button>搜索</button> with
  // `inline-flex items-center`: button is 56×28, text is 24×16 centered at y=6).
  // The wrapper carries background/border/radius/overflow so the visible box
  // still spans the full element rect.
  function renderTextLeaf(el, parentRect, rect, left, top, computed, directText, opts) {
    const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
      box: true, ...opts,
    });
    const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');

    // Inside a flex parent the wrapper drops `position: absolute`, so per-line
    // abs spans would lose their anchor. classifyFlexContainer guarantees
    // single-line leaves only at this point (multi-line leaves cause the parent
    // to bail to absolute). Force `white-space: nowrap` so PAGX's text engine
    // doesn't introduce a phantom wrap when its intrinsic glyph metrics differ
    // from Chromium's by a sub-pixel — the measured wrapper rect already
    // matches Chromium's natural line width.
    if (opts.flexItem) {
      const baseTextStyle = buildStyle(0, 0, 0, 0, computed, {
        box: false, text: true, positioned: false,
      });
      const textStyle = withNowrap(baseTextStyle);
      // Mirror the source's own flex / padding declaration onto the wrapper so
      // the inline span lands where the browser painted it. A text-only flex
      // container — e.g. `<button class="inline-flex items-center
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
      return `<div style="${wrapperStyle}"><span style="${textStyle}">${escapeHtml(directText)}</span>${overlays}</div>`;
    }
    const textNode = firstTextNodeChild(el);
    const lineSpans = textNode
      ? emitTextSpans(textNode, paddingBoxOrigin(rect, computed), computed)
      : [];
    return `<div style="${boxStyle}">${lineSpans.join('')}${overlays}</div>`;
  }

  // Inner-layout declaration to apply on a text-leaf wrapper that's emitted as
  // a flex item. The outer flexItem branch in renderTextLeaf cannot anchor
  // an absolutely-positioned line span (the wrapper itself is no longer
  // absolutely positioned), so the text becomes an inline <span> child of
  // the wrapper. Without help that span hugs the wrapper's top-left corner
  // and loses whatever flex centring / padding the source declared. We
  // re-emit the source layout here so the wrapper behaves like the original
  // box:
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

  // Container with element children (and optionally direct text). Per direct text
  // node we measure the actual rendered rect via Range, so the text lands where
  // the browser put it (next to an <svg>, inside a flex row, …) rather than being
  // smeared across the whole parent and inheriting the parent's text-align.
  //
  // Paint order: browsers paint static-flow descendants first, then positioned
  // descendants on top (z-index:auto → DOM order). Since we flatten every box to
  // `position: absolute`, we must replay that order ourselves: flow children +
  // direct text runs in DOM order, then positioned children (also in DOM order).
  // Otherwise an `<input bg-gray>` declared after an `<svg>` would paint over the
  // icon, an inline-flex bg-pill <button>'s positioned label would render under
  // the bg, etc. Asymmetric border overlays paint *on top* of all children to match
  // CSS, which renders borders after content.
  function renderContainer(el, parentRect, rect, left, top, computed, opts) {
    const childHTML = renderChildrenInto(el, paddingBoxOrigin(rect, computed));
    const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
    const style = buildStyle(left, top, rect.width, rect.height, computed, {
      box: true, ...opts,
    });
    return `<div style="${style}">${childHTML}${overlays}</div>`;
  }

  // Render a subtree rooted at `el` into a string. parentRect is its parent's
  // bounding rect (used to compute relative left/top).
  function render(el, parentRect, opts) {
    opts = opts || {};
    const computed = getComputedStyle(el);

    // `display: contents` makes the element generate no box, but its children still
    // participate in the parent's layout. The element's own `getBoundingClientRect()`
    // is 0×0 at its DOM-tree position, which would break every descendant's relative
    // offset if we used it as their parent. Instead, render the children directly into
    // `parentRect` (the element's CSS parent's rect) and drop the host entirely.
    if (computed.display === 'contents') {
      return renderChildrenInto(el, parentRect);
    }

    // Direct flex preservation: when the live computed style says this element is a
    // flex container and its children can all participate as flex items, emit it as
    // `display: flex` rather than baking everything into `position: absolute`. This
    // keeps the author's flex-direction/gap/padding/align-items/justify-content
    // verbatim instead of forcing the C++ AbsoluteToFlexInferencePass to re-derive
    // them from box geometry. Layouts that don't qualify (mixed abs+flex children,
    // flex-wrap, asymmetric borders, stray text, display:contents middlemen) fall
    // through to the absolute path below — the inference pass remains a backstop.
    const flexChildren = classifyFlexContainer(el, computed);
    if (flexChildren) {
      return renderFlexContainer(el, parentRect, computed, flexChildren, opts);
    }

    const kind = classify(el, computed);
    if (!kind) return '';

    const rect = el.getBoundingClientRect();
    // An empty leaf with a degenerate rect would render as nothing, so skip it.
    // Exception: a flex item like `<div style="flex: 1"></div>` is a spacer that
    // pushes its siblings along the main axis. Under `align-items: center` it
    // reports a non-zero main-axis size but `height: 0` on the cross axis
    // (there is no content to stretch). Dropping it would collapse the gap and
    // shift every following sibling, so keep flex items that have any extent
    // on either axis.
    const rectIsDegenerate = opts.flexItem
      ? (rect.width <= 0 && rect.height <= 0)
      : !nonZero(rect);
    if (rectIsDegenerate && !elementHasChildren(el) && !gatherDirectText(el) &&
        !syntheticText(el)) {
      return '';
    }

    const left = rect.left - parentRect.left;
    const top = rect.top - parentRect.top;

    if (kind === 'svg') return renderSvg(el, parentRect, rect, left, top, computed, opts);
    if (kind === 'img') return renderImg(el, parentRect, rect, left, top, computed, opts);
    if (kind === 'text') return renderTextInput(el, parentRect, rect, left, top, computed, opts);

    const directText = gatherDirectText(el);
    if (!elementHasChildren(el) && directText) {
      return renderTextLeaf(el, parentRect, rect, left, top, computed, directText, opts);
    }
    return renderContainer(el, parentRect, rect, left, top, computed, opts);
  }

  // -------- entry --------
  const body = document.body;
  body.style.margin = '0';
  body.style.padding = '0';
  // If the page (or the user) scrolled before snapshot, every `getBoundingClientRect`
  // call returns viewport-relative offsets shifted by the scroll position. Reset to
  // (0,0) so that the body's own rect lands at the origin and every nested rect is
  // measured against the document, not the current viewport.
  if (typeof window.scrollTo === 'function') {
    try { window.scrollTo(0, 0); } catch (_) { /* ignore */ }
  }
  // Force layout flush.
  void body.offsetHeight;

  // Pick canvas size from the body itself. We deliberately do NOT consult
  // `document.documentElement.scrollWidth/scrollHeight` here — the root element
  // is sized to the viewport whenever the body is smaller, so a phone-sized mock
  // (`<body style="width: 375px; height: 812px">`) would otherwise be inflated
  // to the puppeteer viewport (1400x900) and rendered as a tiny island in the
  // top-left corner. `body.scrollWidth/scrollHeight` already includes any child
  // content that overflows past the body's declared size, so it correctly captures
  // both fixed-size mocks and fluid pages whose content extends past the viewport.
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
  // Body-level gradient (e.g. a full-page hero backdrop) would otherwise be lost
  // because the snapshot's <body> only emits sizing + background-color.
  if (bodyBgImage) {
    bodyStyle.push(`background-image: ${bodyBgImage}`);
  }

  // The <style> block centralises four rendering invariants that the user-agent
  // doesn't supply by default — without them, opening the snapshot in a browser
  // diverges from both the original page and `pagx render`'s output:
  //   - `box-sizing: border-box` on every box matches the semantics of
  //     `getBoundingClientRect()` (the source for our `width`/`height`): we
  //     record padded/bordered extents, so the browser default `content-box`
  //     would re-add padding on top and inflate every wrapper. The pagx
  //     importer already treats border-box as the default
  //     (HTMLStyleResolver.cpp), so this is purely browser-side parity.
  //   - `margin: 0` / `padding: 0` on <body> cancel the user-agent 8px body
  //     margin. The snapshot script already zeroes them on the live DOM
  //     before measuring; without echoing that into the output, every
  //     absolute child reads 8 px short on both axes when re-opened.
  //   - `position: relative` on <body> makes it a containing block. Otherwise
  //     our `position: absolute` children resolve to the initial containing
  //     block (the viewport) and escape the body's coordinate system.
  //   - `<html>` becomes a flex container that centres <body> horizontally
  //     and pads it vertically. Without this, opening the snapshot in a
  //     viewport wider/taller than the captured canvas left the body at
  //     (0, 0) — any `box-shadow` extending above/left of the body (e.g. a
  //     phone-frame's outer ring drawn at y = -12) would be clipped by the
  //     browser viewport. Using flex on <html> doesn't change <body>'s
  //     measured rect, so the pagx importer (which roots at <body> and
  //     ignores <html>) still sees the same canvas.
  // Element selectors inside a single `<style>` block are inside the subset
  // (`spec/html_subset.md` §3.3); the pagx importer parses them, applies the
  // (no-op) declarations, and drops the `<style>` element.
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
/* eslint-enable no-undef, no-inner-declarations */

module.exports = { takeSnapshot, inlineExternalImages };
