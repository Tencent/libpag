# Authoring HTML that converts cleanly to PAGX

Read this before writing the design HTML in Step 2. The converter renders the page in a real
headless browser and then flattens the rendered result into PAGX, so almost any modern HTML/CSS
works. These rules keep the conversion faithful and the output clean. They are guidelines for the
**source design**, not the strict importer subset (the browser snapshot handles the translation);
the authoritative subset reference is `spec/html_subset.md`.

## TL;DR checklist

- [ ] One self-contained `.html` file (no local relative assets).
- [ ] `body` has an explicit pixel `width` and `height` = the target canvas, and `margin:0`.
- [ ] Web fonts loaded via a Google Fonts `<link>` (then convert with `--embed-fonts`).
- [ ] Icons are inline `<svg>` or an icon webfont — never typed glyphs.
- [ ] Real, selectable text for all copy (not text baked into images).
- [ ] No interactive widgets, no scrolling content, no video/iframe.

---

## 1. Canvas size

The PAGX canvas size comes from the `body` box. Always pin it:

```html
<body style="margin: 0; width: 1080px; height: 1440px;">
```

- Match the converter's `--viewport-width` (and `--viewport-height` for tall designs) to these
  numbers so the page is not clipped or letterboxed.
- Design at 1× (real pixels). For crisp previews, render the PNG at a higher scale instead of
  inflating the canvas.

## 2. Make the file self-contained

For a local `.html` file, relative resources (`<img src="logo.png">`, local CSS) will **not**
resolve when the page is rendered. Inline or absolutize everything:

- **Images**: inline as `data:` URIs, or reference an absolute `https://` URL. The snapshot inlines
  external images into the output automatically.
- **CSS**: put it in a `<style>` block or inline `style=""` attributes. A Tailwind CDN `<script>`
  works because the page runs in a browser, but plain inline CSS is the most predictable.
- **Fonts**: load via a Google Fonts (or other CDN) `<link rel="stylesheet">`. The browser fetches
  them; convert with `--embed-fonts` to bake the actual glyphs into the `.pagx`. Without embedding,
  text falls back to whatever font is installed on the rendering host. URL inputs resolve their own
  relative resources, so this caveat applies mainly to local files.

## 3. Layout

Use ordinary CSS — the converter flattens it to absolute boxes and then re-infers flex:

- Prefer **flexbox** (`display:flex`, `flex-direction`, `gap`, `align-items`, `justify-content`,
  `flex: N`) and `padding` for spacing. These map most directly to PAGX containers.
- `position: absolute` with `left/top/right/bottom` is fine for overlays and precise placement.
- Rounded corners (`border-radius`), shadows (`box-shadow`), opacity, gradients
  (`linear-/radial-/conic-gradient`), and `transform` all convert.
- Avoid CSS Grid where flexbox suffices; grid is not modeled and is approximated from the rendered
  positions only.

## 4. Text

- Write real text in `<h1>`–`<h6>`, `<p>`, `<span>`. It becomes editable PAGX text.
- Set `font-family`, `font-size`, `font-weight`, `color`, `line-height`, `text-align`,
  `letter-spacing` as normal.
- For gradient-filled text use `background: linear-gradient(...)` + `-webkit-background-clip: text`
  + `color: transparent`; this converts to a gradient text fill.
- Do not render text as part of an image — it loses selectability and crispness.

## 5. Icons and illustrations

- **Inline `<svg>`** is the best icon source: it is preserved as vector PAGX. Paste the icon's SVG
  markup directly.
- **Icon webfonts** (Phosphor, Material Icons, Font Awesome, Lucide, Remix, Tabler) used via
  `::before { content: "\eXXX" }` are automatically converted to inline `<svg>` during conversion,
  so the result does not depend on the icon font being installed.
- **Never** use typed characters as icons (`+`, `×`, `→`, `★`); they render as plain text and look
  wrong. Use an SVG or icon font instead.

## 6. Images and photos

- `<img>` with `width`/`height` becomes an image fill. Set `object-fit` to control scaling:
  `cover` → zoom-to-fill, `contain` → letterbox, `fill` → stretch.
- The rounded-avatar pattern (an `<img>` inside a `border-radius` + `overflow:hidden` wrapper)
  converts to a clipped rounded image.

## 7. Charts

- Chart libraries that draw on `<canvas>` (ECharts, Chart.js, D3-canvas) are captured as a static
  bitmap, so charts survive as a picture. If the chart animates in, give the page enough settle time
  by raising the converter's `--wait-ms` option.
- SVG-based charts (many D3 setups) convert as vectors, which is sharper — prefer them when
  available.

## What to avoid

These either drop out or convert poorly. Design around them:

- **Hidden content**: `display:none`, `visibility:hidden`, `opacity:0` elements are dropped (by
  design — PAGX has no hidden nodes). Don't hide-then-reveal; just include what should show.
- **Interactive / stateful widgets**: `<form>`, `<input>`, `<button>`, `<select>`, `<textarea>`
  are reduced to their visible label/value only. Style a `<div>`/`<span>` to *look* like a button
  instead of relying on a real control.
- **Scrolling regions**: only the visible frame is captured. Lay content out to fit the canvas;
  don't rely on overflow scrolling to reveal more.
- **Motion**: animations and hover states are captured in whatever frame the snapshot lands on.
  Design the static end-state.
- **No static visual**: `<video>`, `<audio>`, `<iframe>`, `<dialog>`, `<details>`, `<map>` are
  dropped. Use a poster image (`<img>`) instead of a `<video>`.
- **Tainted/WebGL canvas**: a `<canvas>` drawn from a cross-origin image without CORS, or a WebGL
  canvas without `preserveDrawingBuffer`, captures empty. Use same-origin/CORS-enabled sources.
- **Per-side / dashed-dotted borders**: asymmetric borders become thin overlay rectangles and
  dashed/dotted per-side borders may coerce to solid. Prefer uniform borders.

## Minimal template

```html
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    <link
      rel="stylesheet"
      href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap"
    />
    <style>
      * { box-sizing: border-box; }
    </style>
  </head>
  <body style="margin: 0; width: 640px; height: 400px; font-family: Inter, Arial, sans-serif;">
    <div style="width: 100%; height: 100%; display: flex; flex-direction: column; gap: 16px;
                padding: 32px; background: linear-gradient(135deg, #6366F1, #8B5CF6); color: #fff;">
      <h1 style="margin: 0; font-size: 40px; font-weight: 800;">Headline</h1>
      <p style="margin: 0; font-size: 18px; line-height: 1.5; opacity: 0.92;">
        Supporting copy that explains the offer in one or two short sentences.
      </p>
      <div style="margin-top: auto; align-self: flex-start; padding: 12px 24px; border-radius: 999px;
                  background: #fff; color: #6366F1; font-weight: 600; font-size: 16px;">
        Call to action
      </div>
    </div>
  </body>
</html>
```

Convert it with `tools/html-snapshot/html2pagx page.html --embed-fonts --viewport-width 640`.
