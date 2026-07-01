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
| `--download-fonts` | _disabled_ | Save the page's web fonts to disk as TTF/OTF (see [Download web fonts](#download-web-fonts)) |
| `--font-dir <dir>` | `<output>.fonts/` | Destination for `--download-fonts` (content-addressed; safe to share across runs) |
| `--font-manifest <file>` | _none_ | Write this page's font files (one path per line) for callers sharing a `--font-dir` |
| `--download-images` | _disabled_ | Save external images to disk instead of inlining them as base64 data URIs |
| `--image-dir <dir>` | `<output>.images/` | Destination for `--download-images` |
| `--browser-engine <name>` | `puppeteer` (or `$HTML_SNAPSHOT_BROWSER`) | Headless browser driver: `puppeteer` or `playwright` |

`snapshot.js` also supports `-o -` to write the snapshot HTML to stdout (used as the
`pagx import` bridge). `--embed-fonts` is a `html2pagx`-only flag; `snapshot.js` exposes
only `--download-fonts` (embedding happens in the `pagx font embed` step downstream).

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

### Download web fonts

The snapshot bakes each text node's computed `font-family` **name** into its
inline style, but it cannot embed the font itself: the HTML subset disallows
`@font-face`, so the importer drops it. At render time PAGX resolves a family
by name via `tgfx::Typeface::MakeFromName`, which only finds fonts **installed
on the rendering host**. A page styled with a Google-Fonts / self-hosted web
font that isn't installed locally therefore renders with a wrong fallback face
(e.g. the `font-[900] italic` `Noto Sans SC` heading falls back to a default
sans-serif).

Pass `--download-fonts` to capture the actual font files. Rather than
re-deriving the `@font-face` set from CSS, the pass saves the font files
**Chromium already fetched** while rendering — which for CJK families means
only the unicode-range subset files the page's text actually falls into, not
the hundreds of subsets a `Noto Sans SC` stylesheet declares. Each WOFF2/WOFF
payload is decompressed to a plain SFNT (TTF/OTF) — the only container
`tgfx::Typeface::MakeFromPath` reads — and written to `--font-dir` (default
`<output>.fonts/`) under a content-addressed name
(`<Family>-<Style>-<hash>.<ext>`). The hash suffix de-duplicates by content:
an identical face — even one captured by an earlier run sharing the same
`--font-dir` — is stored once rather than re-saved, while distinct
unicode-range subsets (different bytes) keep their own files so glyph coverage
is preserved:

```bash
node snapshot.js page.html --download-fonts
# → page.subset.html
# → page.fonts/Noto-Sans-SC-Black-1a2b3c4d5e6f7081.ttf, …
```

When a single `--font-dir` is shared across many pages, pass `--font-manifest
<file>` so the snapshot also records just *this* page's font files (one
absolute path per line); feed that list — not the whole shared directory — to
`pagx`.

Hand the files to `pagx` so the document renders (or embeds) with the correct
typeface — `--fallback` both registers the face (matching the document's
`font-family`) and adds it to the per-glyph fallback chain, so multiple CJK
subset files jointly cover the text:

```bash
pagx font embed page.pagx --fallback page.fonts/*.ttf   # self-contained .pagx
pagx render page.pagx -o page.png --fallback page.fonts/*.ttf
```

`html2pagx --download-fonts` wires all of this up automatically (see below).

**Caveats:**

- The pass saves what the browser actually fetched. For **Latin / single-file
  web fonts** (Inter, Roboto, …) each weight is a distinct, correctly-named
  file, so weights round-trip faithfully.
- For **CJK families served as unicode-range subsets** (Google's Noto Sans
  SC/TC/JP, …) the per-subset files carry unreliable, templated `name`-table
  metadata — they frequently all report the same family/weight (e.g.
  `Noto Sans SC Thin`, `usWeightClass: 100`) regardless of the weight the page
  requested, and Chromium may fetch only a subset of the declared weights and
  synthesize the rest. The captured glyphs render correctly (no tofu / system
  fallback) but the *weight* may not match the design. When weight fidelity
  matters for CJK, install the real font on the render host instead.

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
| `--download-fonts` | Download the page's web fonts and register them as render fallbacks (`pagx render --fallback`) **without** embedding them into the `.pagx` |
| `--embed-fonts` | On top of `--download-fonts`, embed the downloaded faces into the `.pagx` (`pagx font embed`) so the document is self-contained and its glyph metrics match the snapshot on any host. Implies `--download-fonts` |
| `--font-dir <dir>` | Where downloaded fonts are written (default `<output>.fonts/`) |
| `--download-images` | Save external images to disk and reference them by path instead of inlining base64 |
| `--image-dir <dir>` | Where downloaded images are written (default `<output>.images/`) |
| `--batch <dir>` | Convert every `.html` under a directory with one shared browser. Incompatible with a positional input, `--output-name`, and `-o`; use `--output-root` for the output location |
| `--output-root <dir>` | Mirror the batch's outputs under this directory (batch only) |
| `--skip-existing` | Skip an input whose target `.pagx` already exists (batch only) |
| `--dry-run` | List the files that would be converted without running the browser or `pagx` (batch only) |
| `--cookie <name=value>` / `--header <Key: Value>` | Forwarded to `snapshot.js` (URL inputs only; repeatable) |
| `--browser-engine <name>` | Headless driver to use (`puppeteer` or `playwright`); forwarded to `snapshot.js`. Honours `$HTML_SNAPSHOT_BROWSER`. |
| `--viewport-width / --viewport-height / --wait-ms / --selector` | Forwarded to `snapshot.js` (defaults 1400 / 900 / 800) |

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
  - `{ "html": "<!doctype …>", "format": "pagx", "options": {…} }` — snapshot
    the given HTML string and (optionally) convert to PAGX.
  - `{ "url": "https://example.com/…", "format": "html", "options": {…} }` —
    fetch the URL in the headless browser and snapshot the rendered page. URL
    inputs also accept `cookies` / `headers` inside `options` (see below).
  Response shape depends on `format`:
  - `format: "html"` (default) → `text/html` with the processed HTML.
  - `format: "pagx"` → `application/xml` with the PAGX document (the same
    XML that `pagx import --format html` writes).
  - `format: "both"` → JSON only (`{ html, pagx, width, height }`); requires
    `Accept: application/json` (otherwise the server returns `406`).
  All single-format requests also accept `Accept: application/json` to get
  `{ html, width, height }` or `{ pagx, width, height }` back. Every
  response carries `X-Snapshot-Width` / `X-Snapshot-Height` /
  `X-Snapshot-Format` headers.
- `GET /snapshot?url=<http(s)-url>&format=html|pagx|both&...options` —
  convenience route for "fetch this URL and snapshot it". No body required.
  Same response semantics as `POST /snapshot`. Supported query params:
  `url` (required), `format`, `viewportWidth`, `viewportHeight`, `waitMs`,
  `selector`, `inlineIconFonts` (booleans accept any of
  `true`/`false`/`1`/`0`).
- `GET /health` — `200 { status, engine, pagxBin, pagxBinExists, activeRequests }`.

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
| `--pagx-bin <path>` | `$PAGX_BIN` or `<repo-root>/cmake-build-debug/pagx` | `pagx` binary used for `format=pagx`/`format=both` requests. Boot succeeds even if the binary is missing — only PAGX requests fail until it is built. |
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

# Save the subset, then import it (PAGX_HTML_SNAPSHOT=0 skips re-snapshotting it):
curl -s --data-binary @page.html http://127.0.0.1:8787/snapshot > page.subset.html
PAGX_HTML_SNAPSHOT=0 pagx import --format html --input page.subset.html --output page.pagx

# …or skip the local pagx step and ask the server to do it for you:
curl -s --data-binary @page.html \
     -H 'Content-Type: text/html' \
     'http://127.0.0.1:8787/snapshot?format=pagx' > page.pagx

# Get HTML and PAGX in one round-trip (JSON envelope):
curl -s -H 'Content-Type: application/json' \
     -H 'Accept: application/json' \
     --data '{
       "html": "<!doctype html><html><body><h1>hi</h1></body></html>",
       "format": "both"
     }' \
     http://127.0.0.1:8787/snapshot
# → {"width":1400,"height":900,"html":"<!DOCTYPE html>…","pagx":"<?xml …"}
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

## Testing

Unit tests live in `test/` and run with [Jest](https://jestjs.io/). They cover
the Node-side modules — CLI parsing, the HTTP query/header helpers, the
browser-engine adapter, font download/SFNT conversion, the `pagx import`
runner, icon-font parsing, and the bench/eval report + image-diff utilities —
without launching a real headless browser (the engine, page, and `pagx` binary
are stubbed).

```bash
cd tools/html-snapshot
npm install          # installs Jest + dev deps (one-time)

npm test             # run the suite
npm run test:coverage # run with a coverage report (written to coverage/)
```

The pure browser-side code (`lib/browser-snapshot.js`) and the modules that
only orchestrate a live browser (`lib/page-loader.js`, `lib/snapshot-runner.js`)
are excluded from the coverage target — they are exercised end-to-end by the
`eval/` and `bench/` harnesses against a headless Chromium instead. A few
ESM-only dependencies (e.g. `pixelmatch`) are transformed for Jest via
`babel.config.js`; the source tree itself stays plain CommonJS and is never
Babel-compiled at runtime.

## Limitations

- Animations are normalised into the canonical `@keyframes` + `animation`
  subset (`spec/html_subset.md` §13) so the importer can replay them. The
  capture pass (`lib/animation-capture.ts`) reads running animations from CSS
  `@keyframes`, the Web Animations API, GSAP, and anime.js, and rewrites them
  as `@keyframes pagxAnim<N>` rules plus an inline `animation` shorthand. Only
  runtime-playable channels survive: `opacity` (→ `alpha`),
  `transform: translate[X|Y]` (→ `x`/`y`), and `color` / `background-color`
  (→ a fill's `SolidColor.color`). Other animated properties (`rotate`,
  `scale`, `width`/`height`, `filter`, `box-shadow`, …) are dropped, and the
  element holds whatever value it had when the snapshot was taken — use
  `--wait-ms` or `--selector` to land on the desired frame for those. Disable
  the pass with `captureAnimations: false` if you want a purely static frame.
- Hover states and other dynamic effects that are not expressed as animations
  are captured in whatever state the page is in when the snapshot is taken.
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
