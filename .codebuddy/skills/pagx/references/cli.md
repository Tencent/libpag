# CLI Reference

The `pagx` command-line tool provides utilities for working with PAGX files. All commands
operate on local `.pagx` files. Ensure `pagx` is installed before first use (see the
setup script in `SKILL.md`). Run `pagx --version` (or `pagx -v`) to check the installed version,
and `pagx <command> --help` for per-command usage.

## Table of contents

- [pagx verify](#pagx-verify) — diagnostics + layout + render in one call
- [pagx render](#pagx-render) — render a PAGX file to an image
- [pagx format](#pagx-format) — pretty-print a PAGX file
- [pagx layout](#pagx-layout) — layout-engine bounds as XML
- [pagx bounds](#pagx-bounds) — rendered pixel bounds of Layers
- [pagx font](#pagx-font) — `info` (query metrics) and `embed` (embed into PAGX)
- [pagx import](#pagx-import) — convert SVG/HTML to PAGX
- [pagx resolve](#pagx-resolve) — expand inline `<svg>` and `import` attributes
- [pagx export](#pagx-export) — export PAGX to SVG/HTML/PPTX

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
pagx render --id "btnLayer" input.pagx            # render only the Layer with id="btnLayer"
pagx render --xpath "/pagx/Layer[2]" input.pagx   # render only the matched Layer
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: input path with format extension) |
| `--format png\|webp\|jpg` | Output format (default: png) |
| `--scale <float>` | Scale factor (default: 1.0) |
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

Always output to the same directory as the input `.pagx` file. Do not commit rendered
image files.

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

Convert a file from another format to a standalone PAGX file. Two input formats are
supported: **SVG** and **HTML** (a restricted HTML/CSS subset — see `spec/html_subset.md`).
The format is inferred from the input file extension and can be forced with `--format`.
The output is optimized via `PAGXOptimizer` (PathData is inlined rather than shared) so the
result is close to hand-authored PAGX and can be polished further with the other CLI tools.

Unsupported constructs are handled on a best-effort basis: offending elements/properties are
skipped or downgraded and recorded as warnings. Conversion warnings are **suppressed by default**
(they are noisy and non-fatal); pass `--verbose` / `-v` to print them to stderr. Errors are always
printed regardless of `--verbose`.

**Import directives are resolved by default.** The HTML importer represents external SVG
`<img src="….svg">` and inline `<svg>…</svg>` as unresolved `import` directives on a Layer.
`import` expands those into native PAGX nodes in the same pass, so the output is fully flattened
and a separate `pagx resolve` is normally unnecessary. Pass `--no-resolve` to keep the directives
in place (e.g. to preserve external references, or to resolve later with different options).
If any directive fails to resolve, `pagx import` reports the error and exits non-zero **without
writing the output file** — fix the source (or pass `--no-resolve` to import the directives
unexpanded and resolve them later) and retry.

```bash
pagx import --input icon.svg                      # SVG to icon.pagx
pagx import --input icon.svg --output out.pagx    # SVG to out.pagx
pagx import --input layout.html                   # HTML to layout.pagx (import directives resolved)
pagx import --input page.html --output card.pagx  # HTML to card.pagx
pagx import --input page.html --no-resolve        # keep inline <svg>/import directives unexpanded
pagx import --input page.html -v                  # print conversion warnings
```

| Option | Description |
|--------|-------------|
| `--input <file\|url>` | Input file or URL to import (required) |
| `-o, --output <file>` | Output PAGX file (default: `<input>.pagx`; required when `--input` is a URL) |
| `--format <format>` | Force input format (`svg` or `html`; default: inferred from the input file extension) |
| `--no-resolve` | Keep `import` directives (external `<svg>` images, inline `<svg>`) instead of expanding them into native nodes (default: resolve) |
| `--images <mode>` | How image resources are stored: `external` (default) or `embed`. See [Image storage](#image-storage) |
| `--image-base-dir <dir>` | Directory to resolve relative image paths against (default: the input file's directory) |
| `--verbose, -v` | Print conversion warnings (suppressed by default) |

### Image storage

Controls how `<Image>` resources are stored in the output PAGX (`--images`, default `external`).
The two modes are symmetric and **only ever touch local file references** — an image already
carried as embedded bytes or a `data:` URI is left exactly as it is in either mode (inline content
is never extracted to a file, and existing data URIs are never rewritten). Absolute paths and remote
`http(s)://` URLs are also left untouched.

- **`external` (default)** — images that reference a local file are copied next to the output
  `.pagx` at their original relative path, and `source` keeps that relative name unchanged, so the
  `.pagx` plus its sidecar image files form a portable folder. The authored relative path is
  preserved even when writing to a different output directory (e.g. `assets/logo.png` stays
  `assets/logo.png`).
- **`embed`** — images that reference a local file are read and inlined as base64 `data:` URIs,
  producing a single self-contained `.pagx`.

`--image-base-dir` sets the directory that relative image paths are resolved against when locating
the files on disk; it defaults to the input file's directory.

```bash
pagx import --input page.html --output card.pagx                 # external (default): sidecar images
pagx import --input page.html --output card.pagx --images embed  # self-contained: base64 images
```

### SVG options

| Option | Description |
|--------|-------------|
| `--svg-no-expand-use` | Do not expand `<use>` references |
| `--svg-flatten-transforms` | Flatten nested transforms into single matrices |
| `--svg-preserve-unknown` | Preserve unsupported SVG elements as Unknown nodes |

### HTML import behavior

HTML import behavior is fixed in code (there are no `--html-*` flags). Every HTML input is:

- **rendered through `tools/html-snapshot/snapshot.js`** (a headless browser) first, so
  JS/React/Tailwind-driven pages are flattened into a static, absolute-positioned HTML subset
  the importer understands;
- **normalized** by the subset normalizer, which resolves the `<style>` cascade into inline
  styles, drops disallowed properties, and prunes unsupported elements;
- **flex-recovered** via flex inference (on by default), which recovers `display:flex` semantics
  from the absolute-positioned snapshot output;
- sized from the `<body>` intrinsic/declared size (preferred over any caller-provided target);
- imported non-strictly — unsupported constructs become warnings, not hard errors.

The snapshot step requires `node` on `PATH` and a `snapshot.js` install. The script is located
in this order:

1. relative `tools/html-snapshot/snapshot.js` from the current directory
2. `PAGX_HTML_SNAPSHOT_BIN` environment variable
3. Upward search from the current directory for `tools/html-snapshot/snapshot.js` (up to 8
   parent levels)

URL inputs (`http://` / `https://`) are imported the same way (the snapshot renderer fetches
the page) and require an explicit `--output`.

**Disabling the snapshot pre-pass.** Set `PAGX_HTML_SNAPSHOT=0` (also accepts
`false`/`no`/`off`) to skip the browser step and import the HTML file directly — useful when the
input is *already* a flat, subset-compliant snapshot (this is what the `html2pagx` tooling sets,
since it renders the snapshot itself). With the pre-pass disabled, URL inputs are rejected (the
importer cannot fetch them).

```bash
pagx import --input app.html                                        # React/Tailwind page
pagx import --input https://example.com/demo --output demo.pagx     # URL input
```

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
`font embed` error out on any remaining `import` directive or inline `<svg>`. Run `pagx resolve`
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
alone fails). Once the format is known, `--output` defaults to `<input>.<format>`.

```bash
pagx export --format svg --input icon.pagx       # PAGX to icon.svg
pagx export --input icon.pagx --output out.svg   # PAGX to out.svg
pagx export --input icon.pagx --output out.pptx  # PAGX to out.pptx
pagx export --format svg --input icon.pagx       # force SVG output format
pagx export --format pptx --input icon.pagx      # force PPTX output format
pagx export --input icon.pagx --svg-indent 4     # 4-space indent
pagx export --input icon.pagx --text-to-path     # convert text to paths
pagx export --input icon.pagx --output out.pptx --ppt-no-bake-unsupported  # keep unsupported features editable
pagx export --input icon.pagx --output out.html  # PAGX to HTML
```

| Option | Description |
|--------|-------------|
| `--input <file>` | Input PAGX file (required) |
| `--output <file>` | Output file (default: `<input>.<format>`) |
| `--format <format>` | Output format (`svg`, `pptx`, or `html`; inferred from output extension). Required if output has no extension |
| `--text-to-path` | Convert text to path geometry using pre-shaped glyph outlines (default: native text rendering) |
| `--svg-indent <n>` | Indentation spaces (default: 2, valid range: 0–16) |
| `--svg-no-xml-declaration` | Omit the `<?xml ...?>` declaration |
| `--ppt-no-bake-unsupported` | Disable the default baking of layers that use features OOXML cannot represent natively — masks, scrollRect clipping, blend modes outside of `Normal`/`Multiply`/`Screen`/`Darken`/`Lighten`, wide-gamut color, and `BackgroundBlurStyle`. By default the exporter bakes these layers into PNG patches so the slide matches the tgfx renderer (for unsupported blend modes and `BackgroundBlurStyle` the backdrop beneath the layer is baked into the PNG too, so the blend/frosted-glass composites against the real scene, at the cost of turning native content under the patch into pixels). Pass this flag to silently drop those features and emit the layer as editable shapes instead (mask ignored, scrollRect dropped, blend falls back to `Normal`, wide-gamut clamped to sRGB). Tiled image patterns are always baked regardless of this flag, and features with no vector fallback (TextPath, ColorMatrix, conic/diamond gradient, shear transform) always bake regardless of this flag |

