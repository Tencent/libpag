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
- webfont icon glyphs (Phosphor, Material Icons, Font Awesome, Lucide,
  Remix, Tabler, …) injected via `::before { content: "\eXXX"; font-family:
  "IconFont" }` are converted to inline `<svg><path/></svg>` so the
  resulting PAGX is self-contained — no longer depends on the icon font
  being installed on the rendering host (see `--no-inline-icon-fonts`
  below to opt out);
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

### Optional: Playwright

The snapshot pipeline can drive Chromium through [Playwright](https://playwright.dev/)
as an alternative to Puppeteer. Playwright is declared as an
`optionalDependency`, so a plain `npm install` will fetch it on supported
platforms; if you skipped it (or if `npm install --omit=optional` was used),
install it explicitly:

```bash
cd tools/html-snapshot
npm install playwright
npx playwright install chromium
```

Select Playwright either per invocation or via an env var:

```bash
node snapshot.js page.html -o page.subset.html --browser-engine playwright
# or:
HTML_SNAPSHOT_BROWSER=playwright node snapshot.js page.html -o page.subset.html
```

The flag wins over the env var; the env var wins over the `puppeteer` default.
Both engines run the same `lib/browser-snapshot.js` payload, so the resulting
HTML is structurally identical; sub-pixel coordinates may differ by a fraction
because the two engines bundle slightly different Chromium builds (Puppeteer's
`chrome-for-testing` vs. Playwright's `chromium-headless-shell`) with their
own font metrics.

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
| `--no-inline-icon-fonts` | _enabled_ | Disable webfont-glyph → inline SVG conversion (see below) |
| `--browser-engine <name>` | `puppeteer` (or `$HTML_SNAPSHOT_BROWSER`) | Headless browser driver: `puppeteer` or `playwright` |

### Inline icon fonts

Icon webfonts (Phosphor, Material Icons, Font Awesome, Lucide, Remix,
Tabler, …) deliver their glyphs through `@font-face` plus
`::before { content: "\eXXX" }` pseudo-elements. By default, the snapshot
walks every PUA pseudo whose `font-family` resolves to a registered
`@font-face` rule, downloads the font file (WOFF2 → `wawoff2` → TTF →
`opentype.js`), extracts the glyph as a vector path, and emits it as an
inline `<svg><path fill="currentColor"/></svg>`. The resulting PAGX:

- no longer depends on the icon font being installed on the rendering
  machine (the legacy path emits `<Text fontFamily="Phosphor"/>` which
  silently falls back to a system font when "Phosphor" isn't found —
  typically a row of `□` glyphs);
- ships only the glyphs the page actually uses (one `<Path>` per icon, no
  font tables or kerning data);
- inherits color through `currentColor`, so `color: red` on an ancestor
  paints the icon red exactly like the original CSS-driven icon font.

CDN-hosted CSS (e.g. `unpkg.com`, `cdnjs.com`) typically blocks
`document.styleSheets[i].cssRules` introspection because the response
wasn't loaded with `crossorigin="anonymous"`. The pre-pass falls back to
fetching the stylesheet via `window.fetch` and parsing `@font-face` blocks
out of the raw CSS text — this works for every CDN that sends
`Access-Control-Allow-Origin: *` (which is all the major ones).

Pass `--no-inline-icon-fonts` to skip the pass entirely. The snapshot
then emits a font-named `<span>` for each pseudo glyph, matching the
pre-flag behaviour. Use this when:

- the source page does not use webfont icons (the pass is a no-op then,
  but disabling it skips a small `document.fonts` walk and a network
  round-trip per unique font URL);
- the rendering host is guaranteed to have the icon font installed and
  you'd rather keep the icon as text (preserves selectability,
  per-glyph color animation in PAGX TextBoxes, etc.);
- network egress is undesirable (the pass fetches each unique font URL
  once via Node's `fetch`).

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
| `--no-inline-icon-fonts` | Forwarded to `snapshot.js`: disable webfont-glyph → inline SVG conversion |
| `--cookie <name=value>` / `--header <Key: Value>` | Forwarded to `snapshot.js` (URL inputs only; repeatable) |
| `--browser-engine <name>` | Headless driver to use (`puppeteer` or `playwright`); forwarded to `snapshot.js`. Honours `$HTML_SNAPSHOT_BROWSER`. |
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

Both bundles expose the same set of functions; all operate on the current
document and require no other globals:

```html
<script src="html-snapshot.umd.js"></script>
<script>
  (async () => {
    await HtmlSnapshot.inlineExternalImages();           // optional
    await HtmlSnapshot.inlineCanvases();                 // optional
    // Optional: inline icon-font glyphs as <svg>. The browser bundle only
    // exposes the collect/apply hooks (font parsing requires opentype.js
    // + a WOFF2 decoder, which we don't bundle). A typical setup posts
    // the targets to a backend running `lib/icon-font.js`'s
    // `resolveIconFontSvgs(targets)` and applies the result here:
    const targets = await HtmlSnapshot.collectIconFontTargets();
    if (targets.length) {
      const results = await fetch('/api/inline-icon-fonts', {
        method: 'POST',
        body: JSON.stringify(targets),
        headers: { 'Content-Type': 'application/json' },
      }).then((r) => r.json());
      HtmlSnapshot.applyIconFontSvgs(results);
    }
    const { html, width, height } = HtmlSnapshot.takeSnapshot();
    // `html` is the same flat, subset-compliant string the CLI writes out.
    // Feed it to `pagx import --format html` (server-side) to convert to PAGX.
  })();
</script>
```

```js
import {
  takeSnapshot,
  inlineExternalImages,
  inlineCanvases,
  collectIconFontTargets,
  applyIconFontSvgs,
} from './html-snapshot.esm.js';
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

### HTTP service

For workflows that want to keep one warm browser around and POST HTML to it
on demand (e.g. an LLM agent that iterates on a page and re-renders after
each edit), launch the snapshot pipeline as an HTTP server:

```bash
cd tools/html-snapshot
npm install                # one-time
node server.js             # listens on http://127.0.0.1:8787
# or via npm:
npm run server
```

The server launches one headless Chromium at startup and keeps it alive for
the lifetime of the process. Each request opens a fresh page, runs the
snapshot pipeline, and closes the page; concurrent requests are served in
parallel against the same browser.

Endpoints:

- `POST /snapshot` — body is either raw HTML (any `Content-Type` other than
  `application/json`) or a JSON envelope. The JSON body must carry exactly one
  of `html` or `url`:
  - `{ "html": "<!doctype …>", "options": {…} }` — snapshot the given HTML
    string.
  - `{ "url": "https://example.com/…", "options": {…} }` — fetch the URL in
    the headless browser and snapshot the rendered page. URL inputs also
    accept `cookies` / `headers` inside `options` (see below).
  Response is the processed HTML (`text/html`) by default, plus
  `X-Snapshot-Width` / `X-Snapshot-Height` headers. Send
  `Accept: application/json` to get `{ html, width, height }` instead.
- `GET /snapshot?url=<http(s)-url>&...options` — convenience route for
  "fetch this URL and snapshot it". No body required. Same response
  semantics as `POST /snapshot`. Supported query params: `url` (required),
  `viewportWidth`, `viewportHeight`, `waitMs`, `selector`, `inlineIconFonts`
  (any of `true`/`false`/`1`/`0`).
- `GET /health` — `200 { status, engine, activeRequests }`.

`options` accepts the same knobs the CLI exposes (all optional):

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| `viewportWidth` | number | `1400` | Headless viewport width in px |
| `viewportHeight` | number | `900` | Headless viewport height in px |
| `waitMs` | number | `800` | Extra settle delay after networkidle |
| `selector` | string | _(auto)_ | Wait for this CSS selector before snapshotting |
| `inlineIconFonts` | boolean | `true` | Convert webfont icon glyphs to inline `<svg>` |
| `cookies` | `[{name, value}]` | — | _URL inputs only_; scoped to the target URL |
| `headers` | `[[key, value]]` or `{key: value}` | — | _URL inputs only_; extra request headers |

Server CLI flags:

| Flag | Default | Notes |
|------|---------|-------|
| `--port <n>` | `8787` | Listen port (env: `PORT`) |
| `--host <addr>` | `127.0.0.1` | Listen address (env: `HOST`) |
| `--browser-engine <name>` | `puppeteer` | `puppeteer` or `playwright` (env: `HTML_SNAPSHOT_BROWSER`) |
| `--max-body-mb <n>` | `32` | Maximum request body size in MB (env: `MAX_BODY_MB`) |

Examples:

```bash
# Raw HTML in, processed HTML out:
curl -s --data-binary @page.html \
     -H 'Content-Type: text/html' \
     http://127.0.0.1:8787/snapshot > page.subset.html

# JSON envelope with custom viewport:
curl -s -H 'Content-Type: application/json' \
     -H 'Accept: application/json' \
     --data '{
       "html": "<!doctype html><html><body><h1>hi</h1></body></html>",
       "options": { "viewportWidth": 800, "waitMs": 200 }
     }' \
     http://127.0.0.1:8787/snapshot
# → {"html":"<!DOCTYPE html>…","width":800,"height":900}

# Fetch a live page and snapshot it (GET, no body):
curl -sG --data-urlencode 'url=https://example.com/' \
     --data-urlencode 'viewportWidth=1280' \
     --data-urlencode 'waitMs=500' \
     http://127.0.0.1:8787/snapshot > example.subset.html

# Same thing via POST JSON (lets you also pass cookies / headers):
curl -s -H 'Content-Type: application/json' \
     --data '{
       "url": "https://example.com/dashboard",
       "options": {
         "cookies": [{ "name": "session", "value": "abc123" }],
         "headers": { "X-User": "alice" },
         "waitMs": 1500
       }
     }' \
     http://127.0.0.1:8787/snapshot > dashboard.subset.html

# Pipe straight into pagx import:
curl -s --data-binary @page.html http://127.0.0.1:8787/snapshot \
  | pagx import --format html --html-infer-flex --input - --output page.pagx
```

For HTML inputs, the pipeline runs the payload in a `file://` URL backed by
a per-request temp file under `$TMPDIR`, so relative external resources
(`<img src="…">`, `<link href="…">`, …) will not resolve. Inline everything
(data URIs, CSS `<style>` blocks) or use absolute URLs in the payload — the
snapshot pipeline itself will still inline images / canvases / icon fonts
before emitting the final HTML. URL inputs avoid this entirely: the
browser navigates to the live page and resolves resources against its
real origin.

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
