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
- `<canvas>` elements are captured via `canvas.toDataURL('image/png')`
  before the DOM walk and emitted as `<img>` tags, so chart libraries
  (ECharts, Chart.js, D3 + Canvas, …) survive the trip to PAGX as static
  bitmaps;
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
node snapshot.js <input.html | http(s)://url> [-o <out.html>]
```

For local files, the output defaults to a sibling with a `.subset.html`
suffix, e.g. `xiaohongshu_react.html` → `xiaohongshu_react.subset.html`.
For URL inputs `-o` is **required** — there is no source path to derive an
output name from.

Options:

| Flag | Default | Description |
|------|---------|-------------|
| `-o, --output <file>` | `<input>.subset.html` (file mode); required for URLs | Output path |
| `--viewport-width <px>` | `1400` | Headless viewport width |
| `--viewport-height <px>` | `900` | Headless viewport height |
| `--wait-ms <ms>` | `800` | Extra settle delay after `networkidle0` |
| `--selector <css>` | _(auto)_ | Wait for this selector before snapshotting |
| `--cookie <name=value>` | — | Cookie scoped to the URL (URL inputs only; repeatable) |
| `--header <Key: Value>` | — | Extra HTTP request header (URL inputs only; repeatable) |

### One-shot pipeline

`html2pagx` wraps snapshot + `pagx import` + `pagx resolve` + `pagx render` in
one call:

```bash
tools/html-snapshot/html2pagx /path/to/page.html
# → page.subset.html  (flat, subset-compliant; suppress with --no-subset-html)
# → page.pagx         (after import + resolve)
# → page.png          (rendered at scale=1)

# URL input: --output-name is required (URLs have no filesystem basename)
tools/html-snapshot/html2pagx https://example.com/demo \
  --output-name demo --output-dir /tmp/out \
  --cookie session=abc123 --header 'X-User: alice'
# → /tmp/out/demo.subset.html / demo.pagx / demo.png
```

Options:

| Flag | Description |
|------|-------------|
| `-o, --output-dir <dir>` | Write outputs to a different directory |
| `--output-name <name>` | Base name for outputs (required when input is a URL) |
| `--pagx-bin <path>` | Path to the `pagx` CLI binary (default `$PAGX_BIN` or `cmake-build-debug/pagx`) |
| `--scale <N>` | PNG render scale (default 1) |
| `--no-render` | Stop after `pagx resolve` |
| `--no-resolve` | Stop after `pagx import` |
| `--no-subset-html` | Do not write `<input>.subset.html`; default keeps it |
| `--cookie <name=value>` / `--header <Key: Value>` | Forwarded to `snapshot.js` (URL inputs only; repeatable) |
| `--viewport-width / --viewport-height / --wait-ms / --selector` | Forwarded to `snapshot.js` |

### Browser bundle (no Node, no puppeteer)

The DOM-walking helpers in `lib/browser-snapshot.js` are pure browser code —
puppeteer is only needed to drive an external page from the command line. The
same logic can be packaged as a standalone browser script that runs against
the live page it is loaded into:

```bash
cd tools/html-snapshot
npm run build:browser
# → dist/html-snapshot.umd.js   (UMD: <script> sets window.HtmlSnapshot,
#                                 also CommonJS / AMD compatible)
# → dist/html-snapshot.esm.js   (ESM: import { takeSnapshot } from "...")
# → dist/example.html           (drop-in demo that snapshots itself)
```

Both bundles expose the same three functions; both operate on the current
document and require no other globals:

```html
<script src="html-snapshot.umd.js"></script>
<script>
  (async () => {
    await HtmlSnapshot.inlineExternalImages();           // optional
    await HtmlSnapshot.inlineCanvases();                 // optional
    const { html, width, height } = HtmlSnapshot.takeSnapshot();
    // `html` is the same flat, subset-compliant string the CLI writes out.
    // Feed it to `pagx import --format html` (server-side) to convert to PAGX.
  })();
</script>
```

```js
import { takeSnapshot, inlineExternalImages, inlineCanvases } from './html-snapshot.esm.js';
```

Use cases:

- bookmarklet / DevTools snippet to capture any page on demand,
- Chrome extension content script (the bundle has no `eval` and no remote
  fetches beyond the optional image-inlining pass, which obeys CORS),
- in-app "export to PAGX" button that POSTs the snapshot to a backend
  running the `pagx` CLI.

The snapshot output is byte-identical to what `node snapshot.js` produces for
the same page — both code paths share `lib/browser-snapshot.js`.

The browser bundle covers step 1 of the HTML → PAGX pipeline (snapshot).
Step 2 (`pagx import --format html`) is currently a C++ binary; running it in
the browser would require a WebAssembly build of libpag's HTML importer.

### Manual pipeline (for debugging individual steps)

```bash
node tools/html-snapshot/snapshot.js xiaohongshu_react.html
pagx import --format html --input xiaohongshu_react.subset.html --output xiaohongshu_react.pagx
pagx resolve xiaohongshu_react.pagx
pagx render xiaohongshu_react.pagx -o xiaohongshu_react.png --scale 2
```

## Limitations

- Animations, hover states, and other dynamic effects are captured in whatever
  state the page is in when the snapshot is taken. Use `--wait-ms` or
  `--selector` to land on the desired frame.
- Elements with `display: none`, `visibility: hidden`, or `opacity: 0` are
  dropped, which is intentional: PAGX cannot represent hidden DOM nodes.
- `<video>`, `<audio>`, `<iframe>`, `<dialog>`, `<details>`,
  `<template>`, `<slot>`, `<map>`/`<area>`, `<source>`/`<track>`, etc. are
  dropped — they have no static visual representation.
- `<canvas>` is captured via `toDataURL`, which fails silently in two
  cases: tainted 2D canvases (cross-origin `drawImage` source without CORS),
  and WebGL canvases created without `preserveDrawingBuffer: true` (the
  back buffer may be empty by the time we read it). Such canvases are
  dropped and render as empty boxes.
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
