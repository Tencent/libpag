#!/usr/bin/env node
/**
 * html-snapshot
 *
 * Render an arbitrary HTML page (typically a React/Tailwind/lucide app that materialises
 * the DOM at runtime) in a headless browser and emit a flat, absolute-positioned snapshot
 * that conforms to spec/html_subset.md.
 *
 * Pipeline:
 *   1. Launch headless Chromium, navigate to the source file.
 *   2. Wait for the page to settle (networkidle + selector + a short delay).
 *   3. Walk the rendered DOM. For every kept element emit a flat box that:
 *        - is `position: absolute` with `left/top/width/height` taken from
 *          getBoundingClientRect(), relative to its parent box;
 *        - keeps only the visual CSS properties allowed by the subset
 *          (background-color, color, font-family, font-size, font-weight, ...);
 *        - drops Tailwind/utility classes;
 *        - rewrites <button>, <input>, etc. into <div>/<span>;
 *        - keeps inline <svg> verbatim;
 *        - resolves <img> src to its filesystem-relative path.
 *   4. Write the resulting <!DOCTYPE html>…</html> string next to the input.
 *
 * The output is intended to be fed directly to `pagx import --format html`. The libpag
 * HTMLSubsetTransformer (src/pagx/html/HTMLSubsetTransformer.cpp) handles the final
 * cleanup (unit conversion, dropping any leftover unsupported properties, etc.).
 */

const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer');

//--------------------------------------------------------------------------------------------------
// CLI
//--------------------------------------------------------------------------------------------------

function parseArgs(argv) {
  const opts = {
    input: '',
    output: '',
    viewportWidth: 1400,
    viewportHeight: 900,
    waitMs: 800,
    selector: '',
  };
  const positional = [];
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-o' || a === '--output') {
      opts.output = argv[++i];
    } else if (a === '--viewport-width') {
      opts.viewportWidth = Number(argv[++i]);
    } else if (a === '--viewport-height') {
      opts.viewportHeight = Number(argv[++i]);
    } else if (a === '--wait-ms') {
      opts.waitMs = Number(argv[++i]);
    } else if (a === '--selector') {
      opts.selector = argv[++i];
    } else if (a === '-h' || a === '--help') {
      printUsage();
      process.exit(0);
    } else if (a.startsWith('-')) {
      console.error(`html-snapshot: unknown option '${a}'`);
      process.exit(2);
    } else {
      positional.push(a);
    }
  }
  if (positional.length === 0) {
    printUsage();
    process.exit(2);
  }
  opts.input = path.resolve(positional[0]);
  if (!opts.output) {
    const dir = path.dirname(opts.input);
    const base = path.basename(opts.input, path.extname(opts.input));
    opts.output = path.join(dir, `${base}.subset.html`);
  } else {
    opts.output = path.resolve(opts.output);
  }
  return opts;
}

function printUsage() {
  console.log(`Usage: html-snapshot <input.html> [-o <out.html>] [options]

Render <input.html> in a headless browser and emit a flat, absolute-positioned
snapshot suitable for 'pagx import --format html'.

Options:
  -o, --output <file>        Output path (default: <input>.subset.html)
  --viewport-width <px>      Headless viewport width (default 1400)
  --viewport-height <px>     Headless viewport height (default 900)
  --wait-ms <ms>             Extra settle delay after networkidle (default 800)
  --selector <css>           Wait for this selector before snapshotting`);
}

//--------------------------------------------------------------------------------------------------
// In-page snapshot logic
//
// Everything inside takeSnapshot() runs in the page context (no Node APIs available).
//--------------------------------------------------------------------------------------------------

/* eslint-disable no-undef, no-inner-declarations */
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

  // Box-level (background) visual properties. Emitted on container/leaf wrappers.
  const BOX_PROPS = [
    'background-color',
    'background-image',
    'border-radius',
    'overflow',
    'opacity',
    'filter',
    'backdrop-filter',
    'mix-blend-mode',
  ];

  // Text-level visual properties. Emitted on text leaves only (or on inline text
  // children of mixed containers).
  const TEXT_PROPS = [
    'color',
    'font-family',
    'font-size',
    'font-weight',
    'font-style',
    'letter-spacing',
    'line-height',
    'text-align',
    'text-decoration-line',
    'text-decoration-color',
    'white-space',
  ];

  const DEFAULT_VALUES = new Map([
    ['background-color', ['rgba(0, 0, 0, 0)', 'transparent']],
    ['background-image', ['none']],
    // Chromium's computed `border-radius` is the long-form `T R B L` even when authored
    // as a single value; allow both shapes through the default filter.
    ['border-radius',    ['0px', '0px 0px 0px 0px']],
    ['color',            ['rgb(0, 0, 0)']],
    ['font-style',       ['normal']],
    ['font-weight',      ['400', 'normal']],
    ['letter-spacing',   ['normal', '0px']],
    ['text-align',       ['start', 'left']],
    ['text-decoration-line', ['none']],
    // Computed `text-decoration-color` defaults to `currentColor`; we forward it only
    // when it differs from the element's `color`, so the default check is sufficient.
    ['text-decoration-color', ['currentcolor']],
    ['white-space',      ['normal']],
    ['overflow',         ['visible']],
    ['opacity',          ['1']],
    ['filter',           ['none']],
    ['backdrop-filter',  ['none']],
    ['mix-blend-mode',   ['normal']],
  ]);

  // Properties we strip from text leaves whose value matches the default — keeps the
  // output close to the hand-authored subset shape.
  function isDefault(prop, value) {
    const defaults = DEFAULT_VALUES.get(prop);
    return defaults ? defaults.includes(value) : false;
  }

  function px(n) {
    if (!isFinite(n)) return '0px';
    return `${Math.round(n)}px`;
  }

  function escapeHtml(s) {
    return s
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  // Strip quotes and take only the first family. Tailwind/system defaults expand to
  // 5+ families with embedded "double quotes" which would otherwise break the
  // surrounding style="…" attribute.
  function simplifyFontFamily(value) {
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

  function normalizeValue(prop, value) {
    if (prop === 'font-family') return simplifyFontFamily(value);
    if (prop === 'overflow') return normalizeOverflow(value);
    if (prop === 'background-image') return normalizeBackgroundImage(value);
    if (prop === 'border-radius') return normalizeBorderRadius(value);
    return value;
  }

  // Walk an SVG subtree and rewrite `currentColor` / `context-fill` / `context-stroke`
  // to the computed colour at that node so the snapshot is self-contained. The clone is
  // detached from the document, so `getComputedStyle` can't resolve it directly; instead
  // we walk the *original* subtree (where the cascade is live) and the clone in
  // lockstep, propagating per-node `color` down the tree.
  function freezeSvg(svgEl) {
    const clone = svgEl.cloneNode(true);
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
  // positioned relative to `parentRect`.
  function emitTextSpans(textNode, parentRect, computed) {
    const ws = computed.getPropertyValue('white-space');
    const lines = splitTextNodeIntoLines(textNode, ws);
    const out = [];
    for (const line of lines) {
      const tl = line.rect.left - parentRect.left;
      const tt = line.rect.top - parentRect.top;
      const base = buildStyle(tl, tt, line.rect.width, line.rect.height, computed, {
        box: false, text: true,
      });
      // Force nowrap on every line span so PAGX's text engine never re-breaks.
      const style = /white-space:/.test(base) ? base : `${base}; white-space: nowrap`;
      out.push(`<span style="${style}">${escapeHtml(line.text)}</span>`);
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
    const sides = ['top', 'right', 'bottom', 'left'];
    const w0 = computed.getPropertyValue('border-top-width');
    const s0 = computed.getPropertyValue('border-top-style');
    const c0 = computed.getPropertyValue('border-top-color');
    if (s0 !== 'solid') return false;
    if ((parseFloat(w0) || 0) <= 0) return false;
    for (const side of sides) {
      if (computed.getPropertyValue(`border-${side}-width`) !== w0) return false;
      if (computed.getPropertyValue(`border-${side}-style`) !== s0) return false;
      if (computed.getPropertyValue(`border-${side}-color`) !== c0) return false;
    }
    return true;
  }

  // Returns an array of HTML strings — one absolutely-positioned <div> per non-zero
  // border side — to overlay onto the host box. Used when the element has an
  // asymmetric border (e.g. `border-bottom: 1px solid #e5e7eb` on a list row).
  function borderOverlayHTML(computed, width, height) {
    if (isUniformBorder(computed)) return [];
    const out = [];
    const sides = [
      { name: 'top',    horizontal: true,  position: 'top' },
      { name: 'right',  horizontal: false, position: 'right' },
      { name: 'bottom', horizontal: true,  position: 'bottom' },
      { name: 'left',   horizontal: false, position: 'left' },
    ];
    for (const side of sides) {
      const w = parseFloat(computed.getPropertyValue(`border-${side.name}-width`)) || 0;
      if (w <= 0) continue;
      const style = computed.getPropertyValue(`border-${side.name}-style`);
      // The subset has no model for dashed/dotted/double per-side borders. Downgrade
      // them to solid: it's an approximation but preserves the visual divider, which
      // is what the page actually wanted.
      if (style === 'none' || style === 'hidden') continue;
      const color = computed.getPropertyValue(`border-${side.name}-color`).trim();
      let left = 0;
      let top = 0;
      let w2 = 0;
      let h2 = 0;
      if (side.name === 'top') {
        left = 0; top = 0; w2 = width; h2 = w;
      } else if (side.name === 'bottom') {
        left = 0; top = Math.max(0, height - w); w2 = width; h2 = w;
      } else if (side.name === 'left') {
        left = 0; top = 0; w2 = w; h2 = height;
      } else /* right */ {
        left = Math.max(0, width - w); top = 0; w2 = w; h2 = height;
      }
      out.push(`<div style="position: absolute; left: ${px(left)}; top: ${px(top)}; ` +
        `width: ${px(w2)}; height: ${px(h2)}; background-color: ${color}"></div>`);
    }
    return out;
  }

  // Build the style string for a kept element. `opts.box` includes background/border/
  // shadow/etc.; `opts.text` includes color/font-*; `opts.positioned` toggles the
  // absolute-positioning header (off for inline text children that ride on their
  // parent's box).
  function buildStyle(left, top, width, height, computed, opts) {
    const parts = [];
    if (opts.positioned !== false) {
      parts.push(`position: absolute`);
      parts.push(`left: ${px(left)}`);
      parts.push(`top: ${px(top)}`);
      parts.push(`width: ${px(width)}`);
      parts.push(`height: ${px(height)}`);
    }

    if (opts.box) {
      for (const prop of BOX_PROPS) {
        const raw = computed.getPropertyValue(prop).trim();
        const v = normalizeValue(prop, raw);
        if (!v) continue;
        if (isDefault(prop, v)) continue;
        parts.push(`${prop}: ${v}`);
      }
      // Border: only forward as a single CSS `border` when all four sides are the
      // same width / style / color (the only form the subset accepts). Asymmetric
      // borders — divider lines under a row, accent strips on a card — are emitted as
      // overlay rectangles by emitBorderOverlays() and added to the child list, so
      // they survive into PAGX even though the importer can't model per-side borders.
      if (isUniformBorder(computed)) {
        const bw = parseFloat(computed.getPropertyValue('border-top-width')) || 0;
        if (bw > 0) {
          const bc = computed.getPropertyValue('border-top-color').trim();
          parts.push(`border: ${px(bw)} solid ${bc}`);
        }
      }
      // box-shadow: pass through verbatim if non-none.
      const shadow = computed.getPropertyValue('box-shadow').trim();
      if (shadow && shadow !== 'none') {
        parts.push(`box-shadow: ${shadow}`);
      }
    }

    if (opts.text) {
      const textColor = computed.getPropertyValue('color').trim();
      for (const prop of TEXT_PROPS) {
        const raw = computed.getPropertyValue(prop).trim();
        const v = normalizeValue(prop, raw);
        if (!v) continue;
        if (isDefault(prop, v)) continue;
        // Only forward `text-decoration-color` when it actually differs from the
        // text color; CSS treats them as the same when the value resolves to the
        // initial `currentColor`, and emitting the duplicate adds noise.
        if (prop === 'text-decoration-color' && v === textColor) continue;
        const outProp = prop === 'text-decoration-line' ? 'text-decoration' : prop;
        parts.push(`${outProp}: ${v}`);
      }
    } else if (opts.colorOnly) {
      // Icon wrappers (host of an inline <svg>) only need `color` so that any
      // currentColor stroke/fill picks up the right tint.
      const raw = computed.getPropertyValue('color').trim();
      const v = normalizeValue('color', raw);
      if (v && !isDefault('color', v)) {
        parts.push(`color: ${v}`);
      }
    }

    return parts.join('; ');
  }

  // Re-resolve a relative <img> src to a path relative to the *output* file. Since the
  // output sits next to the input, we just keep the original relative path verbatim.
  function imgSrc(el) {
    const raw = el.getAttribute('src') || '';
    return raw;
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
    if (computed.display === 'none') return null;
    if (computed.visibility === 'hidden') return null;
    if (parseFloat(computed.opacity) === 0) return null;
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
          domIndex: domIndex++,
          html: render(n, parentRect),
        });
      } else if (n.nodeType === Node.TEXT_NODE && n.nodeValue && n.nodeValue.trim()) {
        const spans = emitTextSpans(n, parentRect, hostComputed);
        if (spans.length > 0) {
          items.push({
            positioned: false,
            domIndex: domIndex++,
            html: spans.join(''),
          });
        }
      }
    }
    items.sort((a, b) => {
      if (a.positioned !== b.positioned) return a.positioned ? 1 : -1;
      return a.domIndex - b.domIndex;
    });
    return items.map((it) => it.html).join('');
  }

  // Render a subtree rooted at `el` into a string. parentRect is its parent's
  // bounding rect (used to compute relative left/top).
  function render(el, parentRect) {
    const computed = getComputedStyle(el);

    // `display: contents` makes the element generate no box, but its children still
    // participate in the parent's layout. The element's own `getBoundingClientRect()`
    // is 0×0 at its DOM-tree position, which would break every descendant's relative
    // offset if we used it as their parent. Instead, render the children directly into
    // `parentRect` (the element's CSS parent's rect) and drop the host entirely.
    if (computed.display === 'contents') {
      return renderChildrenInto(el, parentRect);
    }

    const kind = classify(el, computed);
    if (!kind) return '';

    const rect = el.getBoundingClientRect();
    if (!nonZero(rect) && !elementHasChildren(el) && !gatherDirectText(el) && !syntheticText(el)) {
      return '';
    }

    const left = rect.left - parentRect.left;
    const top = rect.top - parentRect.top;

    if (kind === 'svg') {
      // The wrapper carries the icon tint via `color`. We emit color only — emitting
      // the full text-prop block would inject font-family / line-height noise from
      // the surrounding <button>'s defaults.
      const wrapper = buildStyle(left, top, rect.width, rect.height, computed, {
        box: true, colorOnly: true,
      });
      return `<div style="${wrapper}">${freezeSvg(el)}</div>`;
    }

    if (kind === 'img') {
      const src = imgSrc(el);
      const alt = escapeHtml(el.getAttribute('alt') || '');
      const imgStyle = `position: absolute; left: 0px; top: 0px; width: ${px(rect.width)}; height: ${px(rect.height)}`;
      const wrapperStyle = buildStyle(left, top, rect.width, rect.height, computed, { box: true });
      const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
      return `<div style="${wrapperStyle}"><img src="${escapeHtml(src)}" alt="${alt}" style="${imgStyle}"/>${overlays}</div>`;
    }

    if (kind === 'text') {
      const text = syntheticText(el);
      // `<input>` paints its background across the full element rect, but the actual
      // text sits inside the padding box. Split the output into an outer box <div>
      // (carries the bg/border/radius) and an inner text <span> indented by the
      // element's padding so it doesn't overlap absolute siblings (e.g. the search
      // icon at `left: 16px` inside the same wrapper).
      const padL = parseFloat(computed.getPropertyValue('padding-left')) || 0;
      const padR = parseFloat(computed.getPropertyValue('padding-right')) || 0;
      const padT = parseFloat(computed.getPropertyValue('padding-top')) || 0;
      const padB = parseFloat(computed.getPropertyValue('padding-bottom')) || 0;

      const placeholderColor = (!el.value && el.getAttribute('placeholder'))
        ? getComputedStyle(el, '::placeholder').getPropertyValue('color').trim()
        : '';

      const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
        box: true,
      });
      const innerWidth = Math.max(0, rect.width - padL - padR);
      const innerHeight = Math.max(0, rect.height - padT - padB);
      let textStyle = buildStyle(padL, padT, innerWidth, innerHeight, computed, {
        box: false, text: true,
      });
      if (placeholderColor) {
        textStyle = textStyle.replace(/(^|; )color: [^;]+/, `$1color: ${placeholderColor}`);
      }
      const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
      return `<div style="${boxStyle}"><span style="${textStyle}">${escapeHtml(text)}</span>${overlays}</div>`;
    }

    // Container or text-leaf box.
    const hasElementKids = elementHasChildren(el);
    const directText = gatherDirectText(el);

    // Pure-text leaf (no element children): emit one outer <div> carrying the box
    // styles plus one nowrap <span> per visually-rendered line. Each line span is
    // placed at the text's *actual* rendered rect (from Range.getClientRects), not
    // stretched to fill the parent box — otherwise the text inherits the parent
    // height and clings to the top of the container instead of sitting where the
    // browser's flex/baseline layout placed it (e.g. <button>搜索</button> with
    // `inline-flex items-center`: button is 56×28, text is 24×16 centered at y=6).
    // The wrapper carries background/border/radius/overflow so the visible box
    // still spans the full element rect (matches the hand-authored subset baseline
    // for e.g. the <div bg=red><span text>小</span></div> logo pattern).
    if (!hasElementKids && directText) {
      const textNode = (() => {
        for (const n of el.childNodes) if (n.nodeType === Node.TEXT_NODE) return n;
        return null;
      })();
      const lineSpans = textNode ? emitTextSpans(textNode, rect, computed) : [];
      const boxStyle = buildStyle(left, top, rect.width, rect.height, computed, {
        box: true,
      });
      const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');
      return `<div style="${boxStyle}">${lineSpans.join('')}${overlays}</div>`;
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
    const childHTML = renderChildrenInto(el, rect);
    const overlays = borderOverlayHTML(computed, rect.width, rect.height).join('');

    const style = buildStyle(left, top, rect.width, rect.height, computed, { box: true });
    return `<div style="${style}">${childHTML}${overlays}</div>`;
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

  // Pick canvas size: prefer body scrollWidth/Height, fall back to viewport.
  const canvasWidth = Math.max(body.scrollWidth, document.documentElement.scrollWidth);
  const canvasHeight = Math.max(body.scrollHeight, document.documentElement.scrollHeight);

  const bodyRect = body.getBoundingClientRect();

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

  return {
    html:
`<!DOCTYPE html>
<html>
  <head>
    <title>${title.replace(/</g, '&lt;').replace(/&/g, '&amp;')}</title>
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

//--------------------------------------------------------------------------------------------------
// Driver
//--------------------------------------------------------------------------------------------------

async function main() {
  const opts = parseArgs(process.argv);

  if (!fs.existsSync(opts.input)) {
    console.error(`html-snapshot: input not found: ${opts.input}`);
    process.exit(1);
  }

  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--font-render-hinting=none'],
  });
  try {
    const page = await browser.newPage();
    await page.setViewport({
      width: opts.viewportWidth,
      height: opts.viewportHeight,
      deviceScaleFactor: 1,
    });
    page.on('console', (msg) => {
      if (msg.type() === 'error') {
        console.error(`page error: ${msg.text()}`);
      }
    });
    page.on('pageerror', (err) => {
      console.error(`page exception: ${err.message}`);
    });

    const url = 'file://' + opts.input;
    await page.goto(url, { waitUntil: 'networkidle0', timeout: 30000 });

    if (opts.selector) {
      await page.waitForSelector(opts.selector, { timeout: 15000 });
    } else {
      // Heuristic: many React-CDN apps mount into <div id="root">. Wait for at least
      // one child if that root exists.
      try {
        await page.waitForFunction(
          'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true',
          { timeout: 10000 },
        );
      } catch (_) { /* not fatal */ }
    }

    if (opts.waitMs > 0) {
      await new Promise((r) => setTimeout(r, opts.waitMs));
    }

    const result = await page.evaluate(takeSnapshot, { /* config placeholder */ });
    fs.writeFileSync(opts.output, result.html, 'utf8');
    console.log(`html-snapshot: wrote ${opts.output} (${result.width}x${result.height})`);
  } finally {
    await browser.close();
  }
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
