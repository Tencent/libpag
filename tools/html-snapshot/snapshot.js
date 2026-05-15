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
  // Tags we never emit. Their subtree is also dropped (except <head> children that we
  // explicitly preserve elsewhere).
  const DROP_TAGS = new Set([
    'script', 'style', 'link', 'meta', 'noscript',
    'iframe', 'object', 'embed', 'video', 'audio', 'canvas',
    'br', 'hr',
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
    'white-space',
  ];

  const DEFAULT_VALUES = new Map([
    ['background-color', ['rgba(0, 0, 0, 0)', 'transparent']],
    ['background-image', ['none']],
    ['border-radius',    ['0px']],
    ['color',            ['rgb(0, 0, 0)']],
    ['font-style',       ['normal']],
    ['font-weight',      ['400', 'normal']],
    ['letter-spacing',   ['normal', '0px']],
    ['text-align',       ['start', 'left']],
    ['text-decoration-line', ['none']],
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
    return value;
  }

  // Walk an SVG subtree and rewrite stroke="currentColor" / fill="currentColor" to the
  // computed colour so the snapshot is self-contained.
  function freezeSvg(svgEl) {
    const cs = getComputedStyle(svgEl);
    const fallback = cs.color || 'rgb(0, 0, 0)';
    const clone = svgEl.cloneNode(true);
    const walk = (el, inherited) => {
      let stroke = el.getAttribute && el.getAttribute('stroke');
      let fill = el.getAttribute && el.getAttribute('fill');
      const here = (stroke === 'currentColor' || fill === 'currentColor') ? inherited : inherited;
      if (stroke === 'currentColor') el.setAttribute('stroke', here);
      if (fill === 'currentColor') el.setAttribute('fill', here);
      for (const c of el.children || []) walk(c, here);
    };
    walk(clone, fallback);
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
  function splitTextNodeIntoLines(textNode) {
    const text = textNode.nodeValue || '';
    if (!text.trim()) return [];
    const range = document.createRange();
    range.selectNodeContents(textNode);
    const lineRects = Array.from(range.getClientRects());
    if (lineRects.length === 0) {
      range.detach && range.detach();
      return [];
    }
    if (lineRects.length === 1) {
      const out = [{
        text: text.replace(/\s+/g, ' ').trim(),
        rect: lineRects[0],
      }];
      range.detach && range.detach();
      return out;
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
        text: b.chars.join('').replace(/\s+/g, ' ').trim(),
        rect: b.rect,
      }))
      .filter((b) => b.text);
  }

  // Emit one absolutely-positioned, nowrap <span> per line of `textNode`,
  // positioned relative to `parentRect`.
  function emitTextSpans(textNode, parentRect, computed) {
    const lines = splitTextNodeIntoLines(textNode);
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
      // Border: only forward when it's a single, solid, same-on-all-sides border.
      const bw = parseFloat(computed.getPropertyValue('border-top-width')) || 0;
      if (bw > 0 &&
          computed.getPropertyValue('border-top-style') === 'solid' &&
          computed.getPropertyValue('border-right-width') === computed.getPropertyValue('border-top-width') &&
          computed.getPropertyValue('border-bottom-width') === computed.getPropertyValue('border-top-width') &&
          computed.getPropertyValue('border-left-width') === computed.getPropertyValue('border-top-width')) {
        const bc = computed.getPropertyValue('border-top-color').trim();
        parts.push(`border: ${px(bw)} solid ${bc}`);
      }
      // box-shadow: pass through verbatim if non-none.
      const shadow = computed.getPropertyValue('box-shadow').trim();
      if (shadow && shadow !== 'none') {
        parts.push(`box-shadow: ${shadow}`);
      }
    }

    if (opts.text) {
      for (const prop of TEXT_PROPS) {
        const raw = computed.getPropertyValue(prop).trim();
        const v = normalizeValue(prop, raw);
        if (!v) continue;
        if (isDefault(prop, v)) continue;
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
  function syntheticText(el) {
    const tag = el.tagName.toLowerCase();
    if (tag === 'input' || tag === 'textarea') {
      return (el.value || el.getAttribute('placeholder') || '').trim();
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
    if (tag === 'input' || tag === 'textarea') {
      return syntheticText(el) ? 'text' : null;
    }
    return 'box';
  }

  // Render a subtree rooted at `el` into a string. parentRect is its parent's
  // bounding rect (used to compute relative left/top).
  function render(el, parentRect) {
    const computed = getComputedStyle(el);
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
      return `<div style="${wrapperStyle}"><img src="${escapeHtml(src)}" alt="${alt}" style="${imgStyle}"/></div>`;
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
      return `<div style="${boxStyle}"><span style="${textStyle}">${escapeHtml(text)}</span></div>`;
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
      return `<div style="${boxStyle}">${lineSpans.join('')}</div>`;
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
    // the bg, etc.
    const childItems = [];
    let domIndex = 0;
    for (const n of el.childNodes) {
      if (n.nodeType === Node.ELEMENT_NODE) {
        const childComputed = getComputedStyle(n);
        const isPositioned = childComputed.position !== 'static';
        childItems.push({
          kind: 'element',
          positioned: isPositioned,
          domIndex: domIndex++,
          html: render(n, rect),
        });
      } else if (n.nodeType === Node.TEXT_NODE && n.nodeValue && n.nodeValue.trim()) {
        // Emit one nowrap <span> per browser-determined line.
        const spans = emitTextSpans(n, rect, computed);
        if (spans.length > 0) {
          childItems.push({
            kind: 'text',
            positioned: false,
            domIndex: domIndex++,
            html: spans.join(''),
          });
        }
      }
    }
    childItems.sort((a, b) => {
      if (a.positioned !== b.positioned) return a.positioned ? 1 : -1;
      return a.domIndex - b.domIndex;
    });
    const childHTML = childItems.map((it) => it.html).join('');

    const style = buildStyle(left, top, rect.width, rect.height, computed, { box: true });
    return `<div style="${style}">${childHTML}</div>`;
  }

  // -------- entry --------
  const body = document.body;
  const rootRect = { left: 0, top: 0, width: 0, height: 0 };
  body.style.margin = '0';
  body.style.padding = '0';
  // Force layout flush.
  void body.offsetHeight;

  // Pick canvas size: prefer body scrollWidth/Height, fall back to viewport.
  const canvasWidth = Math.max(body.scrollWidth, document.documentElement.scrollWidth);
  const canvasHeight = Math.max(body.scrollHeight, document.documentElement.scrollHeight);

  const bodyRect = body.getBoundingClientRect();
  rootRect.left = bodyRect.left;
  rootRect.top = bodyRect.top;

  const parts = [];
  for (const c of body.children) {
    parts.push(render(c, bodyRect));
  }

  const title = (document.title || '').trim();
  const bodyBg = getComputedStyle(body).getPropertyValue('background-color').trim();
  const bodyStyle = [
    `width: ${canvasWidth}px`,
    `height: ${canvasHeight}px`,
  ];
  if (bodyBg && !['rgba(0, 0, 0, 0)', 'transparent'].includes(bodyBg)) {
    bodyStyle.push(`background-color: ${bodyBg}`);
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
