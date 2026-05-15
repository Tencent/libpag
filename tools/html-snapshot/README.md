# html-snapshot

Render an arbitrary HTML page in headless Chromium and emit a flat,
absolute-positioned snapshot that conforms to the HTML subset documented in
[`spec/html_subset.md`](../../spec/html_subset.md). The output is intended to be
fed directly to `pagx import --format html`.

## Why

The PAGX `HTMLImporter` (`src/pagx/html/HTMLImporter.cpp`) and its
`HTMLSubsetTransformer` (`src/pagx/html/HTMLSubsetTransformer.cpp`) only parse
static HTML: they do not execute JavaScript. React/JSX apps, Tailwind CDN,
lucide icons, anything that materialises the DOM at runtime, will therefore
look empty to the importer.

This tool bridges that gap. It runs the page in a real browser, waits for it
to settle, then walks the rendered DOM and produces a snapshot in which:

- every box is `position: absolute` with `left/top/width/height` taken from
  `getBoundingClientRect()`, relative to its parent;
- only the visual CSS properties allowed by the subset are forwarded
  (`background-color`, `background-image` for gradients, `border`,
  `border-radius`, `box-shadow`, `color`, `font-*`, `letter-spacing`,
  `line-height`, `text-align`, `text-decoration`, `text-decoration-color`,
  `opacity`, `overflow`, `backdrop-filter`, …);
- Tailwind / utility classes are dropped (their effect is already baked into
  the computed style);
- `<button>`, `<input>` (text-bearing types only), `<textarea>` and `<select>`
  are rewritten to `<div>` / `<span>`; their visible label / value /
  placeholder / selected option is synthesised as the text content;
- non-text inputs (`type="checkbox|radio|file|color|range|date|…"`) are
  dropped, since their visual representation depends on browser/OS chrome
  that PAGX has no model for;
- inline `<svg>` icons are kept verbatim, with `currentColor` (and
  `context-fill` / `context-stroke`) frozen per-element so per-node `color`
  overrides survive;
- `display: contents` elements are flattened to a transparent pass-through —
  their box is suppressed and their children are positioned against the
  element's CSS parent;
- asymmetric borders (e.g. `border-bottom: 1px solid #e5e7eb` row dividers)
  are emitted as 1-pixel overlay rectangles, since the subset's `border`
  property only models same-on-all-sides borders;
- relative `<img src>` paths are preserved so the importer can still resolve
  them.

The output still has to pass through `pagx import --format html` to be turned
into PAGX. The importer's normaliser handles any final cleanup (unit
conversion, dropping properties we missed, etc.).

## Install

```bash
cd tools/html-snapshot
npm install
```

Puppeteer ships its own Chromium build; expect ~150 MB of download on first
install. The browser is cached in `$PUPPETEER_CACHE_DIR` (default
`~/.cache/puppeteer`). If `npm install` ran inside a sandbox that redirected the
cache, the browser may be missing from the default location — install it
manually with:

```bash
PUPPETEER_CACHE_DIR="$HOME/.cache/puppeteer" \
  npx --prefix tools/html-snapshot puppeteer browsers install chrome
```

## Usage

```bash
node snapshot.js <input.html> [-o <out.html>]
```

By default the output is written next to the input with a `.subset.html`
suffix, e.g. `xiaohongshu_react.html` → `xiaohongshu_react.subset.html`.

Options:

| Flag | Default | Description |
|------|---------|-------------|
| `-o, --output <file>` | `<input>.subset.html` | Output path |
| `--viewport-width <px>` | `1400` | Headless viewport width |
| `--viewport-height <px>` | `900` | Headless viewport height |
| `--wait-ms <ms>` | `800` | Extra settle delay after `networkidle0` |
| `--selector <css>` | _(auto)_ | Wait for this selector before snapshotting |

### One-shot pipeline

`html2pagx` wraps snapshot + `pagx import` + `pagx resolve` + `pagx render` in
one call:

```bash
tools/html-snapshot/html2pagx /path/to/page.html
# → page.subset.html  (flat, subset-compliant)
# → page.subset.pagx  (after import + resolve)
# → page.subset.png   (rendered at scale=1)
```

Options:

| Flag | Description |
|------|-------------|
| `-o, --output-dir <dir>` | Write outputs to a different directory |
| `--pagx-bin <path>` | Path to the `pagx` CLI binary (default `$PAGX_BIN` or `cmake-build-debug/pagx`) |
| `--scale <N>` | PNG render scale (default 1) |
| `--no-render` | Stop after `pagx resolve` |
| `--no-resolve` | Stop after `pagx import` |
| `--viewport-width / --viewport-height / --wait-ms / --selector` | Forwarded to `snapshot.js` |

### Manual pipeline (for debugging individual steps)

```bash
node tools/html-snapshot/snapshot.js xiaohongshu_react.html
pagx import --format html --input xiaohongshu_react.subset.html
pagx resolve xiaohongshu_react.subset.pagx
pagx render xiaohongshu_react.subset.pagx -o xiaohongshu_react.subset.png --scale 2
```

## Limitations

- Animations, hover states, and other dynamic effects are captured in whatever
  state the page is in when the snapshot is taken. Use `--wait-ms` or
  `--selector` to land on the desired frame.
- Elements with `display: none`, `visibility: hidden`, or `opacity: 0` are
  dropped, which is intentional: PAGX cannot represent hidden DOM nodes.
- `<canvas>`, `<video>`, `<audio>`, `<iframe>`, `<dialog>`, `<details>`,
  `<template>`, `<slot>`, `<map>`/`<area>`, `<source>`/`<track>`, etc. are
  dropped — they have no static visual representation.
- Asymmetric borders are downgraded to overlay rectangles. Per-side `dashed` /
  `dotted` borders are coerced to `solid` (the closest visual approximation
  available in the subset).
- An inline element (`<span>`, `<a>`) whose text wraps to multiple lines is
  emitted as one bounding box; the per-line backgrounds CSS would paint are
  not reproduced.
- `getBoundingClientRect()` rounds to sub-pixel; the snapshot rounds to integer
  pixels to keep the output diff-friendly.
- The page is force-scrolled to `(0, 0)` before measurement so that scroll
  position never leaks into the captured coordinates.
