# html-snapshot

Render an arbitrary HTML page in headless Chromium and emit a flat,
absolute-positioned snapshot that conforms to the HTML subset documented in
[`spec/html_subset.md`](../../spec/html_subset.md). The output is intended to be
fed directly to `pagx import --format html`.

Text capture uses Chromium layout text fragments for source boundaries and effective styles
(including `::first-letter`), then measures the corresponding DOM Ranges in CSS pixels.
This preserves drop caps, columns, and overlapping line boxes without relying on global
Y ordering. The browser-only bundle accepts the same metadata through
`HtmlSnapshot.installTextLayout(data)` from
`lib/text-layout.ts`; without a provider, it uses DOM fragment measurements.
First letters originating in generated content/shadow trees without a DOM Range are
diagnosed. Length-changing text transforms use measured source fragments to preserve
the transformed text without duplicating or truncating it. Text is emitted per line
with natural glyph spacing; browser justification and extra word spacing are not retained.

Form text is captured from Chromium's UA layout: live values, placeholder styles,
password masks and textarea line breaks remain editable text. Password DOM values
are never copied into snapshot metadata. Native surfaces (date/select decorations,
checkboxes, radios, range/color, progress/meter, etc.) use isolated transparent PNGs;
ancestor backgrounds and effects are reapplied by the normal paint pipeline. Fieldset
captures only its decoration, retaining editable legend/content and the browser's
legend cutout. Radios, checkboxes and selects using `appearance:none` keep their
authored CSS boxes.
Generated `::before`/`::after` text joins the measured flow, including resolved
counters and `attr()` content when protocol capture is available.

The browser bundle accepts native surfaces through `installControlPaints(paints)`
(`ControlPaint[]` in `lib/control-capture.ts`) and control text through the optional
`controls` field of `installTextLayout(data)` (`TextLayoutData`). Without
these providers it retains control positions and emits diagnostics for missing
native paint/approximate text. Capture and install providers immediately before
`takeSnapshot()`, after viewport, fonts and live control state have settled.

Use `--embed-fonts` for portable output with the page's web fonts. Font capture includes
inline `data:` sources, resolves CSS family aliases to the saved font's internal family,
and copies WOFF2 decoder memory before another decode can reuse it.

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
  (`background-color`, layered `background-image` with per-layer size, position,
  repeat, origin and clip, `border`,
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

Painting is planned across DOM ancestors, using the original computed positioning and
stacking-context properties before transforms are temporarily removed for measurement.
Negative, in-flow, floating, inline, zero-level and positive-level paints follow CSS painting
order; `z-index: auto` remains distinct from an explicit `0`. Flex/grid items use
order-modified tree order. Real stacking contexts remain compositing groups, while
positioned descendants of non-context containers can participate in their enclosing context.
Overflow clips follow the source containing-block chain, including absolute descendants
that bypass an intermediate overflow container.

The emitted `data-pagx-paint-order="fixed"` attribute prevents the importer's flex inference
from rearranging planned fragments. Ordinary flex subtrees stay as flex where their children
can be emitted together. `isolation: isolate` preserves context boundaries for blending.

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

### Capturing an already-open page

The `lib/current-state.ts` API captures a static interaction state without navigating,
scrolling, changing focus, cancelling animations, or rewriting the source DOM:

```js
const { captureCurrentPage } = require('./dist/lib/current-state');
// page is an already-open Puppeteer or Playwright page, after the desired interaction.
const result = await captureCurrentPage(page, { extent: 'viewport' });
// result: { html, width, height, url, warnings }
```

The synchronous `collectCurrentState()` browser helper records computed styles, pseudo
elements, current form values, canvas pixels and scroll offsets into an inert copy. A
separate headless browser converts that copy using the existing HTML subset emitter;
its element rectangles are checked against the source before conversion. This isolates
the emitter's normalization from the live application. Callers capturing repeatedly can
pass a reusable `mirror` engine handle from a **separate** browser process.

Viewport capture preserves the current view. `extent: 'document'` captures the current
materialized document without scrolling to load more content (maximum height 16384px;
larger documents fail instead of truncating). Capture is best effort: runtime content that
cannot be represented, native control/top-layer appearance limitations, and static-copy
layout differences are reported in `warnings` while supported content is still exported.
Embedded documents/media become inert layout placeholders; custom elements retain their
light-DOM children. Neither extent uses a whole-page screenshot fallback. Ordinary HTML/CSS
retains the existing importer's rendering support and limitations. This API captures a static
state; the caller decides when the application is ready.

The browser bundle also exports `collectCurrentState()` so a user-operated page can send
its captured state to a Node host using `renderCapturedPage()`. The older `takeSnapshot()`
continues to use the original page-normalizing behavior.

The collector freezes readable image/canvas pixels and uses captured response bytes for
cross-origin images and CSS assets. Scenario execution attaches `StateResources` before
navigation, retaining assets loaded after interactions and replacing earlier responses
to the same URL. Missing image/CSS resource references are omitted with warnings, and the
isolated renderer blocks remaining external requests instead of refetching them with a
different login session. SDK callers can attach
`StateResources.onResponse` before navigation and pass `resources: await cache.snapshot(timeoutMs)`
to `captureCurrentPage`; the browser-only collector accepts `{urls, stylesheets}` maps
containing data URLs and captured CSS text. Unavailable font sources are removed while
available alternatives are retained. Runtime `FontFace` objects without a captured CSS
definition produce warnings and use font fallbacks. The version-1 browser payload retains
its `unsupported` diagnostic list; `renderCapturedPage()` merges it into result warnings.

Use `--download-fonts` for native render fallbacks, or `pagx import --html-embed-fonts` for portable
PAGX fonts; structured snapshots warn when web fonts require this handling. `--download-images`
writes captured image bytes to disk. These options also apply to scenario execution.
The source application remains running during collection: `stableMs` detects visual
settling, not a completed business transaction. Setting `stableMs: 0` allows a moving state
to be sampled but does not freeze it or guarantee repeatable animation frames.
The existing subset renderer's CSS/text/compositing limitations still apply; a structured
capture does not guarantee pixel identity for arbitrary CSS or WebGL surfaces.

`lib/interaction.ts` adds `runScenario(engineHandle, url, scenario, options)`. A version-1
scenario contains a `steps` array and optional `viewport`, `timeoutMs` (default 120000),
`stepTimeoutMs` (15000) and `stableMs` (150). Supported step types are `click`, `fill`,
`type`, `press`, `hover`, `focus`, `check`, `select`, `scroll`, `waitFor` and `capture`.
Targets specify exactly one of `css`, `testId`, or `role` (with optional exact `name`;
role locators require Playwright). An optional `frame` CSS selector scopes a target to
one iframe. Its embedded document is omitted during capture with a warning; targeting a
frame does not extend the subset emitter's rendering capabilities.

`waitFor` takes exactly one of `state` (`visible`, `hidden`, `attached`, `detached`),
`attribute: {name, value}`, exact `text`, or exact `url` (relative URLs are allowed).
`click` and `press` can also include an expected `url`. `fill` replaces a control's
value, while `type` sends individual keystrokes. `check` takes a boolean `value`;
`select` takes an option value. `scroll` takes `x`/`y` and an optional target container.
Every step can override `timeoutMs`. `capture` requires a unique filesystem-safe `name`
and accepts `extent` as above. Unknown fields are errors. The old capture `unsupported`
option has been removed; remove it from existing scenarios and SDK capture options.

All steps share one page in a fresh isolated context. Captures never reload that page.
The CLI and HTTP entry points dedicate a browser process to each active scenario so
concurrent sessions cannot change one another's window focus. SDK callers should also
use a dedicated source browser per concurrent scenario.
Passing `options.checkpoint` runs only through the selected capture; otherwise results
are returned in capture order. Business readiness belongs in explicit `waitFor` steps.
Before capture the runner waits for visible images, fonts and stable visible geometry.
Images that have already failed to load/decode produce warnings instead of aborting the
scenario. Pending-resource, stability and explicit business-wait deadlines remain errors;
set `stableMs: 0` when deliberately capturing continuous motion. Input actions are not
replayed after a failed readiness condition. Errors include the failed step number.
Warnings are retained per checkpoint in SDK/HTTP results and the export manifest, and
printed by the CLI. A `complete` manifest confirms execution and output generation;
consumers must inspect warnings when visual completeness matters.

Use the checked-in `test/fixtures/interaction.scenario.json` with native import:

```bash
# One exact file, selecting a state after replaying preceding actions.
cmake-build-debug/pagx import tools/html-snapshot/test/fixtures/interaction.html \
  --html-scenario tools/html-snapshot/test/fixtures/interaction.scenario.json \
  --html-checkpoint advanced -o /tmp/advanced.pagx

# All states, one source-page session, optional subsets and rendered preview.
cmake-build-debug/pagx import page.html --html-scenario states.json \
  --output-dir out --html-keep-subset --preview
```

`-o` / `--output` always names one file; multiple states require `--output-dir`.
Directory exports use `<base>.<checkpoint>.pagx`, optional `.png` and `.subset.html`,
and `<base>.captures.json`. An exact output file uses its own stem for sidecars.
`--preview` implies PNG rendering and writes `<base>.preview/sheet-NNN.png` contact
sheets. `--preview-columns` accepts 1–32. Preview never revisits the source page.

The manifest records `running`, `complete` or `failed`, completed outputs, and
preview status/pages. Earlier saved checkpoints still convert after a later
interaction failure; the run remains failed. Cancellation stops further work.
Only listed outputs belong to the current run; older files are not automatically
removed. These are independent static states, not an interactive PAGX state machine.

HTML CLI parameters use `--html-`; scenario mode owns viewport and readiness and
rejects `--html-viewport-*`, `--html-wait-ms`, `--html-selector`, `--html-scroll-reveal`,
`--html-capture-animations` and `--html-batch`. `snapshot.js` and the HTTP API retain
single-snapshot contracts and require a checkpoint when a scenario has multiple captures.

The native frontend parses the public CLI once and passes a normalized request to
`import.js`. Conversion uses the invoking binary with `--html-no-snapshot`, so there
is no recursive browser pass. The native importer resolves, optimizes and stores
images once per result. `PAGX_IMPORT_BIN` overrides the import launcher;
`PAGX_NODE_BIN` selects Node. Help, invalid arguments and batch dry-run do not start
or install a browser. The single-document `PAGX_HTML_SNAPSHOT_BIN` launcher remains
available to HTML import directives and the snapshot tool.


Whole images fit within 320×480 pixels without upscaling or cropping. The actual PNG
dimensions are authoritative, including when `--render-scale` changes the rendered size;
preview rendering always uses scale 1. Labels show sequence, checkpoint name (at most
two lines), and source pixel dimensions. Transparent pixels are composited over a
checkerboard. Text uses system font fallbacks, so label pixels can vary by platform.

Sheets paginate at 4096 pixels per edge, 16 million output pixels, or 32 million
source pixels per sheet. A single input exceeding the source budget is an error;
PNG files over 128 MiB, missing files, and corrupt PNGs are also rejected before
preview rendering. These are resource limits, not a guarantee of a particular peak
memory usage. The generic `lib/contact-sheet.ts` module accepts ordered `{name, png}`
items independently of scenarios, and uses `pngjs` for validation plus the existing
`pagx render` pipeline for composition. Python and additional browser processes are
not required.

Scenario manifests include an optional preview field; paths below are relative to
the manifest directory and use `/` separators:

```json
{
  "preview": {
    "status": "complete",
    "pages": [
      { "file": "interaction.preview/sheet-001.png", "width": 748, "height": 356, "items": ["initial", "advanced"] }
    ]
  }
}
```

The manifest records `pending` with an empty `pages` array during generation, then
`complete` only after all sheets are rendered and published. Preview failure exits
nonzero and records `failed`, an `error` message, and empty `pages`, while preserving
the per-checkpoint outputs. Files are staged and replaced by atomic per-file renames;
a set of multiple files is not an atomic transaction. After a shorter repeated run,
old extra sheets may remain on disk: consumers must read the manifest's `pages`,
not scan the directory. With preview disabled, no `preview` field is written.

The version-1 manifest also has a top-level `status`: `running`, `complete`, or
`failed`. A run replaces the old manifest before opening the source page, appends
only successfully converted checkpoints to `outputs`, and records `error` on failure.
Each checkpoint is saved as it is captured. Native conversion starts after the source
session closes, so conversion time does not delay later interaction steps. If a later
interaction fails, already saved checkpoints are still converted and listed, while
the job remains failed. Actions are never replayed to recover earlier checkpoints.
Preview failure preserves completed per-checkpoint outputs and exits nonzero.
Font fallbacks for a checkpoint or batch input come from that capture's face list;
ordinary CLI snapshots use a per-run font manifest. Shared font caches never add
unrelated faces to these conversions.
On POSIX, SIGINT/SIGTERM cancel the active CLI, close its browser sessions and native
children, stop further conversions, and return 130/143. Interrupted scenario runs record failure when cleanup
can finish; abrupt termination such as SIGKILL can leave `running` in the manifest.
Consumers must require `status: "complete"` for manifests carrying this field.

For HTTP, POST the usual `{html, format}` or `{url, format}` JSON with
`options: {scenario: {...}, checkpoint: "advanced"}`. JSON responses include
`checkpoint` and `warnings`; multiple captures require a checkpoint. Invalid scenario
syntax, conflicting options and unknown checkpoints return HTTP 400 before execution.
The result contains the supported editable static content and capture warnings, not
the application's event handlers, component state or a replayable interaction runtime.

### Snapshot CLI

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
| `--no-reduced-motion` | _reduced motion enabled_ | Request `prefers-reduced-motion: no-preference`; use when reduced-motion styles hide content |
| `--no-inline-icon-fonts` | _enabled_ | Disable webfont-glyph → inline SVG conversion (see below) |
| `--capture-animations` | _disabled_ | Capture the page's animations (CSS `@keyframes`, Web Animations, GSAP, anime.js) into the subset as `@keyframes` + `animation` so the importer can replay the motion; implies `--no-reduced-motion`; otherwise a single static frame is emitted |
| `--download-fonts` | _disabled_ | Save the page's web fonts to disk as TTF/OTF (see [Download web fonts](#download-web-fonts)) |
| `--font-dir <dir>` | `<output>.fonts/` | Destination for `--download-fonts` (content-addressed; safe to share across runs) |
| `--font-manifest <file>` | _none_ | Write this page's font files (one path per line) for callers sharing a `--font-dir` |
| `--download-images` | _disabled_ | Save external images to disk instead of inlining them as base64 data URIs |
| `--image-dir <dir>` | `<output>.images/` | Destination for `--download-images` |
| `--browser-engine <name>` | `puppeteer` (or `$HTML_SNAPSHOT_BROWSER`) | Headless browser driver: `puppeteer` or `playwright` |

`snapshot.js` also supports `-o -` to write the snapshot HTML to stdout (used as the
single-document HTML loader). `--html-embed-fonts` is a `pagx import` flag; `snapshot.js` exposes
only `--download-fonts` (embedding happens in the `pagx embed` step downstream).

### Inline icon fonts

Icon webfonts (Phosphor, Material Icons, Font Awesome, Lucide, Remix,
Tabler, …) deliver their glyphs through `@font-face` plus
`::before { content: "\eXXX" }` pseudo-elements. By default, the snapshot
walks every PUA pseudo whose `font-family` resolves to a registered
`@font-face` rule, downloads the font file (WOFF2 → `wawoff2` → TTF →
`opentype.js`), extracts the glyph as a vector path, and emits it as an
inline `<svg><path fill="currentColor"/></svg>`. Families with multiple
physical faces are matched by computed `font-weight` and `font-style` (for
example Font Awesome Free Regular/400 versus Solid/900). The resulting PAGX:

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

### Local web fonts

A page whose `@font-face` rules point at font files on **local disk** (a saved
page, or a design-tool HTML export with an `assets/fonts/` folder) needs no
extra flag: the snapshot carries every such rule over into its `<style>` block
as `@font-face{font-family:"…";src:url("<absolute path>");font-weight:…;font-style:…}`
— the same absolute path form local images take. Rules whose `src` is a `data:`
URI are carried over verbatim, so a page that inlines its fonts needs neither a
file on disk nor any flag. The importer registers each source under the **CSS
family the rule declares** — not the family stored inside the font file, which a
page routinely aliases (`font-family: MaShanZheng` for a file named
`Ma Shan Zheng`). These registrations are used during import and are not serialized as
font resources. The host rendering the exported PAGX must supply the selected fonts.
HTTP font declarations are also carried into the snapshot so catalog matching can read their
weight and style. This does not download font bytes: native import only loads local or inline
sources. Without a font catalog or captured font files, remote fonts still use host fallbacks;
use `--download-fonts` below when the actual font files are needed.
When a captured file has a different internal family name, the snapshot retains the CSS family
first and appends the internal name as a fallback, preserving the catalog identity.

The weight and style descriptors travel with the source because they decide
which cut of the family the page actually painted. A page may declare a single
`font-weight: 700` face for a family its CSS requests at 400 — the browser then
matches that face and renders bold outlines — while a variable face declared as
`font-weight: 400 900` answers the same 400 request at weight 400. Carrying only
the family name and source would make the subset claim a regular cut the page
never had, and the editor-catalog match built from it (below) would pick one.
Faces are de-duplicated by family, source **and** descriptors, so the several
cuts of one family survive as separate rules.

### Download web fonts

Local and inline `@font-face` sources are carried into the subset for import-time font
matching. Remote fonts still require capture of the font files used by the browser.

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
pagx embed page.pagx --fallback page.fonts/*.ttf   # self-contained .pagx
pagx render page.pagx -o page.png --fallback page.fonts/*.ttf
```

`pagx import --html-download-fonts` wires all of this up automatically (see below).

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

### Native import pipeline

```bash
# PAGX only by default. Rendering and keeping the subset are explicit.
cmake-build-debug/pagx import /path/to/page.html --output-dir /tmp/out --render --html-keep-subset

# URLs: use an explicit output file or directory + basename.
cmake-build-debug/pagx import https://example.com/demo \
  --output-name demo --output-dir /tmp/out --preview \
  --html-cookie session=abc123 --html-header 'X-User: alice'

# Mirror HTML batches under an output root; reuse a browser, isolate contexts.
cmake-build-debug/pagx import --html-batch /path/to/pages --output-dir /tmp/batch --preview
```

| Flag | Description |
|------|-------------|
| `-o, --output <file>` | One exact PAGX file |
| `--output-dir <dir>` | Artifact directory; required for multiple results |
| `--output-name <name>` | Directory output basename; required for URLs |
| `--render`, `--render-scale <N>` | PNG generation and positive render scale |
| `--preview`, `--preview-columns <1..32>` | Render and assemble contact sheets (SVG or HTML) |
| `--no-resolve` | Keep import directives; conflicts with rendering/preview |
| `--images external\|embed`, `--image-base-dir <dir>` | Native image storage and resource base |
| `--html-no-snapshot` | Import an existing local subset directly |
| `--html-keep-subset` | Retain intermediate subset HTML (default: temporary) |
| `--html-no-inline-icon-fonts` | Disable icon glyph to SVG conversion |
| `--html-capture-animations` | Capture page motion (default: static) |
| `--html-download-fonts` | Download web fonts for native fallback rendering |
| `--html-embed-fonts` | Embed fonts; implies download |
| `--html-font-dir <dir>` | Font download destination |
| `--html-download-images`, `--html-image-dir <dir>` | Download page images and choose their destination |
| `--html-batch <dir>` | Recursively scan HTML; requires `--output-dir` |
| `--skip-existing` | Batch: skip only complete matching artifact records |
| `--dry-run` | Batch: list work without creating outputs or starting a browser |
| `--html-cookie`, `--html-header` | Repeatable URL request credentials |
| `--html-browser-engine <name>` | Puppeteer or Playwright; overrides `HTML_SNAPSHOT_BROWSER` |
| `--html-viewport-width`, `--html-viewport-height`, `--html-wait-ms`, `--html-selector`, `--html-scroll-reveal` | Page viewport/readiness settings |

The runtime shares `lib/pipeline.ts` with the evaluators. Batch completion records
include source/native stamps, options and all required artifacts/resources. Missing
PNGs or changed settings invalidate `--skip-existing`. Batch previews include only
newly completed inputs and live in `<output-dir>/preview/`.

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

# Save the subset, then explicitly import it without another snapshot:
curl -s --data-binary @page.html http://127.0.0.1:8787/snapshot > page.subset.html
pagx import --html-no-snapshot --format html --input page.subset.html --output page.pagx

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
runner, icon-font parsing, and the bench/eval report + image-diff utilities.
DOM tests use jsdom with explicit layout/style fixtures to exercise paint order,
clipping, control isolation, text layout, and animation sampling. Browser protocol
responses and subprocess fixtures keep the unit suite independent of Chromium,
network access, system fonts, and a built `pagx` binary.

```bash
cd tools/html-snapshot
npm install          # installs Jest + dev deps (one-time)

npm test             # run the suite
npm run test:coverage # run with a coverage report (written to coverage/)
```

All three test commands compile the current TypeScript sources first. Coverage
inputs are derived from those sources in `jest.config.js`, so stale `dist/` files
from removed or renamed modules cannot affect the report. The coverage command
enforces minimums of 97% statements, 90% branches, 98% functions, and 98% lines.
Open `coverage/lcov-report/index.html` for the per-file report.

Browser regressions in `test/browser/` run separately with `npm run test:browser`.
They launch Puppeteer's Chromium to check CSS cascade,
pseudo-element paint order, layout, and animation behavior against the source page.
Set `PAGX_PAINT_TEST_BIN` to an absolute path to a built `pagx` binary to additionally
import and render each paint-order fixture and compare its solid interior pixels.

Jest runs at most two workers by default because browser-backed tests can start
both source and mirror browsers. This keeps parallel browser load from causing
timeouts. Override the limit with `npm test -- --maxWorkers=N` when needed.

The browser snapshot implementation (`lib/browser-snapshot.ts`) and the modules that
orchestrate a live browser (`lib/page-loader.ts`, `lib/snapshot-runner.ts`)
are excluded from the coverage target — they are exercised end-to-end by the
`eval/` and `bench/` harnesses against a headless Chromium instead. A few
ESM-only dependencies (e.g. `pixelmatch`) are transformed for Jest via
`babel.config.js`; the TypeScript source tree compiles to CommonJS in `dist/`
and is never Babel-compiled at runtime.

## Limitations

- CSS `preserve-3d` and top-layer elements need painting models beyond the supported 2D
  subset. The snapshot returns `paint:preserve-3d` / `paint:top-layer` warnings for these
  cases; the CLI logs them. Their existing supported representation is retained.
- Captured animations that change the stacking topology (`z-index`, `position`, `display`,
  `isolation`, `contain`) return `paint:animated-stacking`. The emitted paint order is static;
  captured opacity/transform channels retain their compositing groups.
- Animation capture is **off by default** — the snapshot emits a single static
  frame. Pass `--capture-animations` to `snapshot.js`, or `--html-capture-animations`
  to `pagx import`, set `captureAnimations: true` on the `runSnapshot` /
  `runHtmlToPagx` APIs, or send `captureAnimations: true` in the HTTP service's
  options to opt in. When enabled, animations are normalised into the canonical
  `@keyframes` + `animation` subset (`spec/html_subset.md` §13) so the importer
  can replay them. The capture pass (`lib/animation-capture.ts`) reads running
  animations from CSS `@keyframes`, the Web Animations API, GSAP, and anime.js,
  and rewrites them as `@keyframes pagxAnim<N>` rules plus an inline `animation`
  shorthand. Only runtime-playable channels survive: `opacity` (→ `alpha`),
  `transform: translate[X|Y]` (→ `x`/`y`), and `color` / `background-color`
  (→ a fill's `SolidColor.color`). Other animated properties (`rotate`,
  `scale`, `width`/`height`, `filter`, `box-shadow`, …) are dropped, and the
  element holds whatever value it had when the snapshot was taken — use
  `--wait-ms` or `--selector` to land on the desired frame for those.
- Hover states and other dynamic effects that are not expressed as animations
  are captured in whatever state the page is in when the snapshot is taken.
- The browser requests `prefers-reduced-motion: reduce` before navigation so
  pages can expose a complete static presentation instead of an entrance-
  animation start frame. Dynamic effects that ignore that preference are
  captured in whatever state they are in; use `--wait-ms` or `--selector` to
  land on the desired frame. Some pages instead hide or remove animated content
  in their reduced-motion styles; use `--no-reduced-motion` for those pages.
- `position: sticky` scrollytelling blocks — a pinned panel inside a tall
  scroll track whose step layers cross-fade as the page scrolls — are expanded
  before the snapshot: the panel is tiled once per step so the static output
  shows every step instead of the frozen top frame plus blank track. The
  detection is heuristic (a track at least twice the panel's height, stacked
  same-size absolute layers with mutually exclusive opacities) and the
  expansion is opt-out via `HTML_SNAPSHOT_NO_STICKY_EXPAND=1`.
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
- Images whose declared format is not PNG / JPEG / WebP / GIF — an AVIF or
  HEIC served by a CDN, for instance — are re-encoded to WebP in the browser,
  because PAGX consumers are only required to decode those four formats. An
  animated source outside that set keeps only its first frame, a re-encode
  failure keeps the original bytes and warns, and images saved by
  `--download-images` are written to disk untouched.
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

### Match available editor fonts

Provide the frontend's available font catalog in the same conversion request. The converter
reads original CSS family stacks, numeric weights and slants from the snapshot in memory,
selects a catalog face and passes its exact family/style to native import before layout.
No source-font JSON file or matching round trip is required.

The match starts from the cut the page's own `@font-face` rules produced, not from the raw
computed request: when the family at the **head** of the stack declares a face, that face's
weight and slant drive the catalog search (a family declared only at 700 therefore maps to
the catalog's bold face even though the text computes to 400). Requests already inside a
variable face's range are left as computed, and a request is only ever promoted to a heavier
declared cut, never lowered: a 700 heading served by a lone 400 face keeps its 700. A stack
whose head declares no face is left untouched — a font installed on the machine may have
answered it, so a later family's cut is never applied to it (in `Arial, Cinzel`, a declared
700 cut of `Cinzel` never makes the `Arial` request bold). The `--font-replacement`
arguments still carry the computed family, weight and style as the key — that is what the
subset's inline styles declare — with the resolved catalog face as the target.

```json
[
  { "fontName": { "family": "ZCOOL XiaoWei", "style": "Regular" } },
  { "fontName": { "family": "Noto Sans", "style": "Regular" } },
  { "fontName": { "family": "Noto Sans", "style": "Bold" } }
]
```

Module API: `runPagxImport({ pagxBin, html: snapshotHtml, availableFonts })` matches the
catalog against the snapshot in memory and forwards the result to native import as
`--font-replacement <families> <weight> <style> <target-family> <target-face>`; it returns
only PAGX text. Native conversions no longer generate `<output-name>.fonts.json`, and the
former `--font-map` / `fontMap` / `fontMetadataPath` contract has been removed.

Matching follows the ordered CSS family stack. Each family tries literal identity, unique
Unicode NFKC/case/whitespace normalization, then unique normalization of spaces, hyphens and
underscores. Fuzzy matching requires at least six normalized characters, at most 64, a score
of at least 0.85 and a margin of at least 0.08 over the runner-up. A unique vendor-prefix
suffix match (for example `XiaoWei` → `ZCOOL XiaoWei`, length ratio at least 0.5) scores 0.94;
otherwise at most two character edits are considered. Ambiguities stay unmatched. Generic
families such as `sans-serif` and `system-ui` are skipped because the catalog defines no
generic-to-family policy. Missing/empty catalogs preserve existing conversion behavior.

Within a family, prefer the requested slant (italic and oblique may approximate one another),
then a known nearest numeric weight. Common face labels and numeric face names are recognized;
unknown labels are not assumed to weigh 400. Equal distances prefer the lighter face at
weights up to 500, the heavier face above 500. Remaining ties use the original style string's
code-unit order, independent of catalog ordering. Arbitrary variable axes and glyph coverage
are not inferred from names. The original catalog names are copied verbatim to the native
replacement arguments; no family or face is synthesized. The converter still needs the
selected font file installed or captured from the page to lay out that target correctly.

Invalid catalog entries are rejected before browser work. At most 100,000 faces are accepted;
each family/style must be a nonempty string of at most 256 characters without control codes.
Requests are cached within the conversion. `fontMapping` and `[font-map]` logs count matched,
unmatched and request-less text runs; they do not publish a font inventory. Inline SVG text
retains authored markup and is outside computed HTML font matching. Existing icon-to-vector
handling is unchanged.

### Native font replacement

`pagx import` accepts `--font-replacement <families> <weight> <style> <target-family>
<target-face>` to select the exact face before layout. For a family stack beginning with `-`,
use `--font-replacement="-apple-system, sans-serif" 400 normal Target Regular`.
