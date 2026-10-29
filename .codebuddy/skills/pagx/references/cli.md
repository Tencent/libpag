# CLI Reference

Use the selected CLI build consistently for conversion and validation. Check its `--version`,
`--help` and the needed subcommand's `--help`; version alone does not establish feature support.
Most commands operate on PAGX files; `import` accepts SVG/HTML sources, URLs and HTML batches.
See [Environment selection](../SKILL.md#environment-and-command-selection) for repository versus
installed usage. `preview` is provided by the npm launcher, not the standalone native binary.

## Table of contents

- [pagx verify](#pagx-verify) — diagnostics + layout + render in one call
- [pagx render](#pagx-render) — render a PAGX file to an image
- [pagx format](#pagx-format) — pretty-print a PAGX file
- [pagx layout](#pagx-layout) — layout-engine bounds as XML
- [pagx bounds](#pagx-bounds) — rendered pixel bounds of Layers
- [pagx font](#pagx-font) — query font files, families and metrics
- [pagx embed](#pagx-embed) — embed font glyphs and images
- [pagx import](#pagx-import) — SVG/HTML conversion, scenarios, batches and optional artifacts
- [pagx resolve](#pagx-resolve) — expand inline `<svg>` and `import` attributes
- [pagx export](#pagx-export) — export PAGX to SVG/HTML/PPTX
- [pagx preview](#pagx-preview) — live browser preview with hot reload

---

## pagx verify

All-in-one verification command. Resolves inline `<svg>` and `import` attributes (via
`pagx resolve`), runs all checks, and optionally outputs a layout file (via `pagx layout`)
and rendered screenshot (via `pagx render`).

```bash
pagx verify input.pagx                                # full file — outputs .png + .layout.xml
pagx verify --id "header" input.pagx                   # scoped to Layer id
pagx verify --skip-render input.pagx                   # skip screenshot, output .layout.xml only
pagx verify --skip-render --skip-layout input.pagx     # checks only, no file output
```

| Option | Description |
|--------|-------------|
| `--id <id>` | Scope checks and render to the Layer with the specified `id` |
| `--scale <float>` | Render scale factor (default: 1.0) |
| `--skip-render` | Skip screenshot generation |
| `--skip-layout` | Skip layout XML generation |
| `--json` | Output diagnostics in JSON format |

### Steps

1. **Resolve** — expands all inline `<svg>` and `import` attributes in the file
   (full file, idempotent). See `pagx resolve`.
2. **Parse + layout** — loads the document and computes layout bounds.
3. **Check** — runs all detection rules (schema, semantic, structural, spatial, performance).
4. **Diagnostics** — writes problems to stderr, sorted by line number.
5. **Files** (unless skipped) — writes layout XML (see `pagx layout` §Output format)
   and renders screenshot (see `pagx render`). Use `--skip-render` / `--skip-layout` to
   skip individual outputs.

### Diagnostics format

All reported problems must be fixed.

```
file:line: description. Fix: suggested check direction
```

- Single-node problems: single line number.
- Two-node problems (e.g., overlap): two line numbers comma-separated.
- Spatial problems include bounds in the description.
- Text diagnostics: nothing is printed to stderr when there are no problems.
- `--json`: diagnostics are always written to **stdout** as JSON (e.g.
  `{"file": "input.pagx", "ok": true, "diagnostics": []}` when clean), and the text diagnostics above
  are suppressed.
- The layout XML is written silently; only the screenshot prints a `Wrote …` confirmation line.

Example:

```
input.pagx:8: unknown attribute "borderRadius" on Rectangle, use "roundness" instead
input.pagx:18,22: overlapping siblings (20,10,200,40) and (180,10,40,40) in container layout. Fix: check parent layout direction, gap, padding, and children sizes
input.pagx:60,75: duplicate PathData, identical to line 60. Fix: extract to <Resources> and reference via @id
Wrote input.png (786x852 @2x)
```

### File output

Output files are written to the **current working directory** (not the input file's directory).

| Scenario | Screenshot | Layout file |
|----------|-----------|-------------|
| `pagx verify input.pagx` | `input.png` | `input.layout.xml` |
| `pagx verify --id "header" input.pagx` | `input.header.png` | `input.header.layout.xml` |
| `pagx verify --skip-render ...` | not generated | `input.layout.xml` |
| `pagx verify --skip-render --skip-layout ...` | not generated | not generated |

### Exit code

| Code | Meaning |
|------|---------|
| 0 | No problems |
| 1 | Problems found |

---

## pagx render

Render a PAGX file to an image. By default, output to the same directory as the input file
with the same base name (e.g., `foo.pagx` → `foo.png`). `pagx verify` outputs to the
current working directory; `pagx render` outputs next to the input file.

```bash
pagx render input.pagx                    # outputs input.png
pagx render --format webp --scale 2 input.pagx   # outputs input.webp
pagx render -o output.png input.pagx
pagx render -o cropped.png --crop 50,50,200,200 input.pagx
pagx render -o output.jpg --format jpg --quality 90 --background "#FFFFFF" input.pagx
pagx render --font CustomFont.ttf input.pagx
pagx render --font a.ttf --fallback "PingFang SC" input.pagx
pagx render --font a.ttf --fallback b.otf --fallback "Noto Emoji" input.pagx
pagx render --time 0.5 -o frame.png input.pagx   # evaluate animation at 0.5 seconds
pagx render --id "btnLayer" input.pagx            # render only the Layer with id="btnLayer"
pagx render --xpath "/pagx/Layer[2]" input.pagx   # render only the matched Layer
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: input path with format extension) |
| `--format png\|webp\|jpg` | Output format (default: png) |
| `--scale <float>` | Scale factor (default: 1.0) |
| `--time <seconds>` | Evaluate animation at a nonnegative finite playback time; incompatible with `--crop`, `--id` and `--xpath` |
| `--crop <x,y,w,h>` | Crop region in document coordinates (relative to target Layer when `--id`/`--xpath` is set) |
| `--id <id>` | Render only the Layer with the specified `id` attribute |
| `--xpath <expr>` | Render only the Layer matched by XPath expression (must match exactly one) |
| `--quality <0-100>` | Encoding quality (default: 100) |
| `--background <color>` | Background color (#RRGGBB or #RRGGBBAA) |
| `--font <path>` | Register a font file (can be specified multiple times) |
| `--fallback <path\|name>` | Fallback font file or system font name (can be specified multiple times) |

`--id` and `--xpath` are mutually exclusive. When either is specified, only the target Layer
is rendered, cropped to that Layer's bounds.

`--font` registers a font file matched by fontFamily/fontStyle against PAGX text references.
`--fallback` accepts either a file path (e.g., `b.otf`) or a system font name (e.g.,
`"PingFang SC"` or `"Arial,Bold"`). Fallback fonts are tried in order when a character is
not found in the primary font.

Without `--time`, rendering uses the static document state. With it, top-level animations
and timelines mounted on Composition instances are evaluated; mounted timelines honor
`evaluationOffset` (in frames) and `playing="false"`. A paused timeline is evaluated at its
initial playback position, including its offset. See [Animation](animation.md#previewing-the-motion).

Choose output paths appropriate to the task. Keep temporary review images out of commits unless
the user or repository workflow calls for them.

---

## pagx format

Pretty-print a PAGX file with consistent indentation and attribute ordering. Does not modify
values or structure.

```bash
pagx format -o output.pagx input.pagx
pagx format --indent 4 input.pagx       # 4-space indent (default: 2)
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--indent <n>` | Indentation spaces (default: 2) |

---

## pagx layout

Display the layout structure of a PAGX file in XML format. Outputs the full layout tree with
resolved bounds and layout attributes for all Layers and their internal elements (Rectangle,
Ellipse, Path, Polystar, Text, TextPath, Group, TextBox). Pure data output — no problem
detection (use `pagx verify` for checks).

Each node includes a `line` attribute mapping back to the source file line number.

Output is wrapped in a `<layout>` root element. Without `--id`/`--xpath`, a `<pagx>` element
with document dimensions contains the layer tree. With `--id`/`--xpath`, target Layers appear
directly inside `<layout>` with bounds relative to their own origin (starting at 0,0).

```bash
pagx layout input.pagx                                      # full layout tree
pagx layout --id "card" input.pagx                           # scope to a Layer by id
pagx layout --xpath "//Layer[@id='header']" input.pagx       # scope to Layers by XPath
pagx layout --depth 1 input.pagx                             # only one level of child Layers
```

| Option | Description |
|--------|-------------|
| `--id <id>` | Limit scope to the Layer with the specified `id` attribute |
| `--xpath <expr>` | Limit scope to Layers matched by XPath expression |
| `--depth <n>` | Limit Layer nesting depth (0 or negative = unlimited, default: unlimited) |

`--id` and `--xpath` are mutually exclusive.

```xml
<layout>
  <pagx width="400" height="300">
    <Layer line="3" id="root" bounds="0,0,400,300" layout="horizontal" gap="20" padding="20">
      <Rectangle line="5" bounds="0,0,400,300"/>
      <Layer line="7" id="sidebar" bounds="20,20,120,260">
        <Rectangle line="9" bounds="20,20,120,260"/>
      </Layer>
      <Layer line="12" id="content" bounds="160,20,220,260" flex="1">
        <Rectangle line="14" bounds="160,20,220,260"/>
        <TextBox line="16" bounds="176,30,188,20">
          <Text line="17" bounds="176,30,188,20"/>
        </TextBox>
      </Layer>
    </Layer>
  </pagx>
</layout>
```

**Node attributes:**

- `line` — source file line number.
- `bounds` — resolved position and size as `x,y,width,height` in canvas coordinates.

Layout attributes output when non-default: `layout`, `gap`, `flex`, `padding`, `alignment`,
`arrangement`, `includeInLayout="false"`, `clipToBounds="true"`.

`pagx verify` outputs the same format to `input.layout.xml` (or `input.{id}.layout.xml`
when scoped with `--id`), with bounds in canvas coordinates matching `pagx layout` format.

---

## pagx bounds

Query **rendered pixel bounds** of Layer nodes — the actual bounding box of rendered content
including stroke width, shadows, and blur effects. This differs from `pagx layout` bounds,
which show layout-engine-resolved positions and sizes (based on `layout`/`flex`/`gap`/`padding`/
constraint attributes, without rendering effects).

Use `pagx bounds` for determining crop regions (`pagx render --crop`) and measuring rendered
output size. Use `pagx layout` for debugging layout structure and alignment.

Supports `--id` for quick lookup by id attribute and `--xpath` for flexible node selection.
Without either, outputs bounds for the entire document and all layers.

```bash
pagx bounds input.pagx                                          # all layers
pagx bounds --id "btn" input.pagx                                # by id
pagx bounds --xpath "/pagx/Layer[2]/Layer[1]" input.pagx         # by position
pagx bounds --xpath "//Layer[@id='icon']" --relative "//Layer[@id='card']" input.pagx
pagx bounds --json input.pagx
```

| Option | Description |
|--------|-------------|
| `--id <id>` | Select a Layer by its `id` attribute |
| `--xpath <expr>` | Select Layer nodes by XPath expression |
| `--relative <xpath>` | Output bounds relative to another Layer |
| `--json` | JSON output |

`--id` and `--xpath` are mutually exclusive.

XPath quick reference for PAGX:
- `//Layer[@id='x']` — Layer with `id="x"` anywhere in the document
- `/pagx/Layer[1]` — first top-level Layer (1-indexed)
- `/pagx/Layer[2]/Layer` — all child Layers of the second top-level Layer
- `//Layer[@name='Title']` — Layer with `name="Title"`

Only `<Layer>` nodes are supported for bounds queries. If the XPath matches a non-Layer
element, an error is reported.

---

## pagx font

Query font identity and metrics from a font file or system font, or enumerate installed
system font families.

```bash
pagx font --file ./CustomFont.ttf
pagx font --file ./CustomFont.ttf --size 24
pagx font --name "PingFang SC,Bold"
pagx font --name "Arial" --size 24 --json
pagx font --list
pagx font --list --json
```

| Option | Description |
|--------|-------------|
| `--file <path>` | Query a font file |
| `--name <family[,style]>` | Query a system font (e.g., `"Arial"` or `"Arial,Bold"`) |
| `--size <pt>` | Font size in points (default: 12, the PAGX spec default) |
| `--json` | Output in JSON format |
| `--list` | List every installed system font family |

Exactly one of `--file`, `--name`, or `--list` is required. `--list` cannot be combined
with `--file` or `--name`; `--file` and `--name` are mutually exclusive.

Returns typeface info (fontFamily, fontStyle, glyphsCount, unitsPerEm, hasColor, hasOutlines)
and all FontMetrics fields at the specified size (top, ascent, descent, bottom, leading, xMin,
xMax, xHeight, capHeight, underlineThickness, underlinePosition).

The retired `pagx font info` and `pagx font embed` subcommands now error out with a redirect
message; use `pagx font ...` for font queries and `pagx embed` for font embedding.

---

## pagx embed

Embed font glyphs and images into a PAGX file for self-contained output. Font embedding extracts glyph data from laid-out text; image embedding inlines external image files as base64. Font nodes with a `file` attribute are automatically discovered and registered for text shaping — no `--font-file` flag needed.

```bash
pagx embed input.pagx                                    # embed fonts + images (overwrite)
pagx embed -o out.pagx input.pagx                        # embed fonts + images to new file
pagx embed --skip-fonts input.pagx                       # embed images only
pagx embed --skip-images input.pagx                      # embed fonts only
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--fallback <path\|name>` | Fallback font file or system font name (can be specified multiple times) |
| `--skip-fonts` | Skip font embedding |
| `--skip-images` | Skip image embedding |
| `-h, --help` | Show this help message |

Fonts are resolved in the following order:
1. Font nodes with `file` attribute are loaded and registered by their internal family name
2. `--fallback` fonts are tried when a character is not found in the primary font

Image embedding inlines external file references (Image nodes with `filePath`) as base64 data. See the Font `file` attribute in `attributes.md` for details on external font references.

---

## pagx import

Import SVG or HTML into PAGX. HTML files and URLs normally run through the browser snapshot
runtime before native conversion. Prepared subset HTML can bypass the browser explicitly.
Source format is inferred from the extension (URLs and batches use HTML), or set with `--format`.
The native stage resolves import directives by default, optimizes the document and stores images.
Use `-v` / `--verbose` to expose normally suppressed conversion warnings; errors are always reported.

```bash
pagx import icon.svg -o icon.pagx
pagx import page.html --output-dir out --render --html-keep-subset
pagx import https://example.com/demo --output-dir out --output-name demo
pagx import motion.html -o motion.pagx --html-capture-animations
pagx import app.html --html-scenario states.json --output-dir out --preview
pagx import app.html --html-scenario states.json --html-checkpoint filled -o filled.pagx
pagx import --html-batch pages --output-dir out --dry-run
```

### Input, output and artifacts

| Option | Description |
|---|---|
| `<file-or-url>` or `--input <file-or-url>` | One SVG/HTML source; choose one form, and omit it in batch mode |
| `-o, --output <file>` | Exact PAGX filename for one result; never an output directory |
| `--output-dir <dir>` | Artifact directory; required for multiple results and batches; mutually exclusive with `--output` |
| `--output-name <name>` | Base name with `--output-dir`; required for URL directory output; rejected in batch mode |
| `--format svg\|html` | Force the source format |
| `--images external\|embed` | Native image storage, default external; see below |
| `--image-base-dir <dir>` | Source directory for relative image references; normally the local input's directory |
| `--no-resolve` | Keep inline SVG/import directives; incompatible with `--render` and `--preview` |
| `--render` | Add a PNG for each imported PAGX; off by default |
| `--render-scale <factor>` | Positive, finite PNG scale, default 1; requires `--render` or `--preview` |
| `--preview` | Enable PNG rendering and assemble static contact sheets |
| `--preview-columns <n>` | With preview, 1–32 columns; default automatic |
| `--verbose, -v` | Print conversion warnings |

Local single-result imports default beside the input. Ordinary directory output uses
`<base>.pagx`; scenario directory output uses `<base>.<checkpoint>.pagx`.
An exact output filename supplies the stem for its sidecars. URLs require `--output`, or
`--output-dir` plus `--output-name`. Multiple selected checkpoints require `--output-dir`;
select one with `--html-checkpoint` when an exact file is needed.

PNG rendering and subset retention are opt-in. Scenarios also write `<base>.captures.json`,
including when one checkpoint is selected. Require exit code 0 and manifest `status: "complete"`
before reporting a successful scenario. Failure may leave partial files; see
[Interaction capture](interaction.md#export-and-review). Preview is a static overview, distinct
from the live `pagx preview` command.

### HTML options

HTML-specific options use the `--html-` prefix on `pagx import`. The batch control flags
`--skip-existing` and `--dry-run` are also listed here.

| Option | Description |
|---|---|
| `--html-no-snapshot` | Import prepared local subset HTML without a browser; rejects other `--html-*` options and URL input |
| `--html-keep-subset` | Retain `<stem>.subset.html` for debugging; off by default |
| `--html-capture-animations` | Capture supported page motion; ordinary capture is static by default |
| `--html-scenario <file.json>` | Replay an interaction scenario and export all named static checkpoints |
| `--html-checkpoint <name>` | With a scenario, run through and export only this checkpoint |
| `--html-browser-engine puppeteer\|playwright` | Overrides `HTML_SNAPSHOT_BROWSER`; default Puppeteer; role locators need Playwright |
| `--html-viewport-width <px>`, `--html-viewport-height <px>` | Positive integer viewport dimensions; default 1400×900 |
| `--html-wait-ms <ms>`, `--html-selector <css>` | Extra ordinary-page settle delay (default 800 ms) and selector wait |
| `--html-scroll-reveal` | Scroll ordinary pages to trigger reveal/lazy content before capture |
| `--html-download-fonts` | Save used web fonts and register this capture's faces as native render fallbacks |
| `--html-embed-fonts` | Embed captured font glyphs; implies downloading |
| `--html-font-dir <dir>` | Download directory; requires font download/embedding; defaults to `<base>.fonts` |
| `--html-download-images` | Save captured image bytes instead of keeping them inline in the subset |
| `--html-image-dir <dir>` | Download directory; requires `--html-download-images`; defaults to `<base>.images` |
| `--html-no-inline-icon-fonts` | Disable icon-webfont to SVG conversion |
| `--html-cookie <name=value>`, `--html-header <Key: Value>` | Repeatable, URL inputs only |
| `--html-batch <dir>` | Recursively import `.html`/`.htm`; requires `--output-dir` |
| `--skip-existing` | Batch-only: reuse complete matching artifact sets, not merely existing PAGX files |
| `--dry-run` | Batch-only: list planned work without starting/installing a browser or creating outputs |

Scenario mode owns viewport and readiness. It rejects the ordinary viewport, wait, selector,
scroll-reveal, animation-capture and batch options; use scenario fields and steps instead.
Batch mode rejects a single input, `--output` and `--output-name`. `--dry-run` is not a scenario
validation mode. Unknown options are errors; legacy unprefixed HTML flags are not aliases.

Ordinary snapshots may recover flex layout from measured boxes; interaction-state snapshots
preserve measured positions without flex inference. HTML browser capabilities and the native
subset contract differ: see [HTML subset](../spec/html_subset.md) and [Animation](animation.md).
For resources, setup and runtime lookup (`PAGX_IMPORT_BIN`, `PAGX_NODE_BIN`), see
[Pipeline](pipeline.md). `snapshot.js` remains a separate single-subset tool with its own options.

### Image storage

`--images` controls native storage of relative local image references. Existing embedded bytes,
`data:` URIs, absolute paths and remote URLs are left as they are; neither mode guarantees that
all resources in an arbitrary input become portable.

- **`external` (default)** — copy relative local images beside the output while preserving their
  relative paths, such as `assets/logo.png`. Deliver the PAGX and its required sidecars together.
- **`embed`** — inline relative local image files as base64 data URIs.

`--image-base-dir` changes where those relative files are read from. HTML image downloading is
separate from native storage; use `--html-download-images` only when downloaded files are wanted.
For portable captured text, use `--html-embed-fonts` and inspect typography and warnings.

```bash
pagx import page.html -o card.pagx --images embed --html-embed-fonts
pagx import page.subset.html -o page.pagx --html-no-snapshot
```

### SVG options and resolution

| Option | Description |
|---|---|
| `--svg-no-expand-use` | Do not expand SVG `<use>` references |
| `--svg-flatten-transforms` | Flatten nested transforms into single matrices |
| `--svg-preserve-unknown` | Preserve unsupported SVG elements as Unknown nodes |

These options also apply to nested SVG imports. Inline SVG and external import directives are
resolved by default. A failed resolution prevents that result's PAGX from being written;
`--no-resolve` retains directives for later resolution. A larger scenario/batch may still retain
other results or intermediate files. Plain SVG conversion and prepared-subset conversion run
natively; requesting `--render` or `--preview` uses the Node artifact runtime too.

---

## pagx resolve

Resolve all inline `<svg>` elements and `import` attributes in a PAGX file into native PAGX
nodes. When a Layer has explicit `width`/`height`, content is uniformly scaled to fit
(centered, aspect ratio preserved). Otherwise, the Layer's size is set from the source
dimensions. Adds a comment indicating the source (e.g., `<!-- Resolved from: inline svg -->`
or `<!-- Resolved from: assets/logo.svg -->`).

`pagx import` already runs this pass by default, so this command is mainly for **hand-authored**
PAGX files that reference external SVG or embed inline `<svg>`, or for PAGX produced with
`pagx import --no-resolve`. It is idempotent — running it on an already-resolved file is a no-op.

Most other commands require a fully-resolved file: `render`, `layout`, `bounds`, `export`, and
`embed` error out on any remaining `import` directive or inline `<svg>`. Run `pagx resolve`
(or `pagx import` without `--no-resolve`, or `pagx verify`, which resolves first) before them.

```bash
pagx resolve design.pagx                          # resolve in place
pagx resolve design.pagx -o out.pagx              # resolve to new file
pagx resolve design.pagx --images embed           # also inline local images as base64
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--images <mode>` | How image resources are stored: `external` (default) or `embed`. Same behavior as [`pagx import`](#image-storage) — only local file references are affected; data URIs are left untouched |
| `--image-base-dir <dir>` | Directory to resolve relative image paths against (default: the input file's directory) |

Image resources created while resolving inline `<svg>` (e.g. `<image href="…">`) are normalized to
the requested storage mode in the same pass. Passing `--images`/`--image-base-dir` also forces the
image pass (and file writes) even when there are no import directives to resolve.

Format-specific options (e.g. `--svg-*`) are shared with `pagx import`; see above for details.

---

## pagx export

Export a PAGX file to another format (SVG, PPTX, or HTML). The output format is inferred from the
output file extension. If neither `--format` nor an output file with a recognizable extension is
provided, the command reports an error (there is no implicit default format — `--input icon.pagx`
alone fails). Once the format is known, `--output` defaults to `<first input>.<format>`.

For **PPTX only**, `--input` may be repeated to build a multi-slide deck — each `--input` becomes one
slide, in the order given, and the deck adopts the first document's canvas size (PPTX stores a single
slide size for the whole presentation). SVG and HTML take a single document, so passing more than one
`--input` for those formats is rejected. If any input fails to load, the whole export aborts.

```bash
pagx export --format svg --input icon.pagx       # PAGX to icon.svg
pagx export --input icon.pagx --output out.svg   # PAGX to out.svg
pagx export --input icon.pagx --output out.pptx  # PAGX to out.pptx
pagx export --format svg --input icon.pagx       # force SVG output format
pagx export --format pptx --input icon.pagx      # force PPTX output format
pagx export --input a.pagx --input b.pagx --output deck.pptx  # multi-slide deck (one slide per input)
pagx export --input icon.pagx --svg-indent 4     # 4-space indent
pagx export --input icon.pagx --text-to-path     # convert text to paths
pagx export --input icon.pagx --output out.pptx --ppt-no-bake-unsupported  # keep unsupported features editable
pagx export --input icon.pagx --output out.pptx --ppt-ignore-glyphruns  # editable text instead of glyph paths
pagx export --input icon.pagx --output out.html  # PAGX to HTML
```

| Option | Description |
|--------|-------------|
| `--input <file>` | Input PAGX file (required). Repeat to add more slides — **pptx only**; each `--input` becomes one slide in deck order |
| `--output <file>` | Output file (default: `<first input>.<format>`) |
| `--format <format>` | Output format (`svg`, `pptx`, or `html`; inferred from output extension). Required if output has no extension |
| `--text-to-path` | Convert text to path geometry using pre-shaped glyph outlines (default: native text rendering) |
| `--ppt-ignore-glyphruns` | Ignore the GlyphRun geometry carried by Text nodes and emit native, editable PowerPoint text derived from the `text` attribute instead. By default a Text that carries GlyphRun data is written as custom glyph paths, because `a:r` runs cannot express arbitrary glyph IDs, per-glyph offsets, anchors, or rotations; this flag trades that glyph-level fidelity for text the reader can still edit. Text nodes without GlyphRun data are unaffected. Opposite of `--text-to-path` — if both are passed, `--text-to-path` wins |
| `--svg-indent <n>` | Indentation spaces (default: 2, valid range: 0–16) |
| `--svg-no-xml-declaration` | Omit the `<?xml ...?>` declaration |
| `--ppt-no-bake-unsupported` | Disable the default baking of layers that use features OOXML cannot represent natively — masks, scrollRect clipping, blend modes outside of `Normal`/`Multiply`/`Screen`/`Darken`/`Lighten`, wide-gamut color, and `BackgroundBlurStyle`. By default the exporter bakes these layers into PNG patches so the slide matches the tgfx renderer (for unsupported blend modes and `BackgroundBlurStyle` the backdrop beneath the layer is baked into the PNG too, so the blend/frosted-glass composites against the real scene, at the cost of turning native content under the patch into pixels). Pass this flag to silently drop those features and emit the layer as editable shapes instead (mask ignored, scrollRect dropped, blend falls back to `Normal`, wide-gamut clamped to sRGB). Tiled image patterns are always baked regardless of this flag, and features with no vector fallback (TextPath, ColorMatrix, conic/diamond gradient, shear transform) always bake regardless of this flag |

---

## pagx preview

The complete npm package exposes a live browser viewer with hot reload. A standalone native
`pagx` has no `preview` command. A prepared repository preview can instead be launched with
`node /absolute/path/libpag/playground/pagx-preview/src/cli.js <file>`; follow that directory's
README for dependencies and viewer artifacts. Use the same entry point with `stop` for cleanup.

The viewer watches the PAGX file. Saving source HTML alone does not update it; reconvert the HTML
first. Static PNGs/contact sheets are separate review artifacts, and viewing a captured checkpoint
does not restore the original webpage's event handlers. See [Preview delivery](../SKILL.md#live-browser-preview)
for when to launch and what to give the user.

```bash
pagx preview input.pagx                # background daemon, prints URL, returns prompt
pagx preview --json input.pagx         # single-line JSON {url,pid,logFile,reused} on stdout
pagx preview --foreground input.pagx   # run in foreground (Ctrl+C to stop)
pagx preview --port 8080 input.pagx    # pick a specific port (default: random free port)
pagx preview stop                      # stop the background daemon
pagx preview --log                     # print the daemon log file
```

| Option / Subcommand | Description |
|---------------------|-------------|
| `<file>` | PAGX file to preview (required unless using `--mcp`, `stop`, or `--log`) |
| `--port <n>` | Port to bind (default: 0 = auto-pick a free port) |
| `--host <addr>` | Bind address (default: `127.0.0.1`) |
| `--fonts <dir>` | Extra font directory for the preview renderer |
| `--json` | Emit a single-line JSON record on stdout (`{url,pid,logFile,reused}`); implies `--no-open` |
| `--no-open` | Do not auto-open a system browser (default in non-TTY / agent contexts) |
| `--foreground` | Run the server in the foreground instead of a detached daemon |
| `--mcp` | Run as a stdio MCP server for AI assistants (not for interactive preview) |
| `stop` | Stop the running background daemon |
| `--log` | Print the daemon log file path and contents |

### Behavior

- **Default = background daemon.** `pagx preview input.pagx` spawns a detached server process,
  prints `pagx preview: http://127.0.0.1:<port>/...` to stdout, and **returns the shell prompt**
  (it does not block). The server keeps running in the background so repeated invocations reuse
  it instead of starting a new one.
- **Browser auto-open.** In a real TTY the default opens the system browser; in non-TTY contexts
  (piped stdout, IDE agents, CI) it does not — the URL is printed only. Pass `--no-open` to force
  it off, or open the printed URL manually / via the IDE's browser preview tool.
- **Hot reload.** The server watches the `.pagx` file; any save triggers a `reload` SSE event
  that refreshes every open tab. Editing the source HTML in the HTML→PAGX workflow and re-running
  `pagx import` will also reload the preview automatically.
- **Session reuse.** A second `pagx preview another.pagx` reuses the already-running daemon,
  opens a new session for the new file, and prints its URL. Each file gets its own session and
  is cleaned up independently when its tab disconnects.
- **Lifecycle.** Reuse the server while iterating. Stop it through the chosen launcher when the
  requested preview work is finished, unless the user wants it left running.


### HTML font matching

`pagx import` supports `--font-replacement <families> <weight> <style> <target-family>
<target-face>` for exact face selection before layout. Bind a leading-dash family stack with
`--font-replacement=<families>`.
