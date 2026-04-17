# CLI Reference

The `pagx` command-line tool provides utilities for working with PAGX files. All commands
operate on local `.pagx` files. Ensure `pagx` is installed before first use (see the
setup script in `SKILL.md`).

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
- No output when there are no problems (when both `--skip-render` and `--skip-layout` are set).

Example:

```
input.pagx:8: unknown attribute "borderRadius" on Rectangle, use "roundness" instead
input.pagx:18,22: overlapping siblings (20,10,200,40) and (180,10,40,40) in container layout. Fix: check parent layout direction, gap, padding, and children sizes
input.pagx:60,75: duplicate PathData, identical to line 60. Fix: extract to <Resources> and reference via @id
Wrote input.png (786x852 @2x)
Wrote input.layout.xml
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

Font operations with two subcommands: `info` (query metrics) and `embed` (embed into PAGX).

### pagx font info

Query font identity and metrics from a font file or system font.

```bash
pagx font info --file ./CustomFont.ttf
pagx font info --file ./CustomFont.ttf --size 24
pagx font info --name "PingFang SC,Bold"
pagx font info --name "Arial" --size 24 --json
```

| Option | Description |
|--------|-------------|
| `--file <path>` | Font file path |
| `--name <family[,style]>` | System font by name (e.g., `"Arial"` or `"Arial,Bold"`) |
| `--size <pt>` | Font size in points (default: 12, the PAGX spec default) |
| `--json` | JSON output |

Either `--file` or `--name` is required (mutually exclusive).

Returns typeface info (fontFamily, fontStyle, glyphsCount, unitsPerEm, hasColor, hasOutlines)
and all FontMetrics fields at the specified size (top, ascent, descent, bottom, leading, xMin,
xMax, xHeight, capHeight, underlineThickness, underlinePosition).

### pagx font embed

Embed fonts into a PAGX file by performing text layout and glyph extraction.

```bash
pagx font embed input.pagx
pagx font embed -o out.pagx input.pagx
pagx font embed --file a.ttf --file b.ttf input.pagx
pagx font embed --file a.ttf --fallback "PingFang SC" --fallback b.otf input.pagx
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--file <path>` | Register a font file (can be specified multiple times) |
| `--fallback <path\|name>` | Fallback font file or system font name (can be specified multiple times) |

`--file` and `--fallback` work the same as in `pagx render`.

---

## pagx import

Convert a file from another format (e.g. SVG) to a standalone PAGX file.

```bash
pagx import --input icon.svg                     # SVG to icon.pagx
pagx import --input icon.svg --output out.pagx   # SVG to out.pagx
```

| Option | Description |
|--------|-------------|
| `--input <file>` | Input file to import (required) |
| `--output <file>` | Output PAGX file (default: `<input>.pagx`) |
| `--format <format>` | Force input format (`svg`; default: inferred from extension) |
| `--svg-no-expand-use` | Do not expand `<use>` references |
| `--svg-flatten-transforms` | Flatten nested transforms into single matrices |
| `--svg-preserve-unknown` | Preserve unsupported SVG elements as Unknown nodes |

---

## pagx resolve

Resolve all inline `<svg>` elements and `import` attributes in a PAGX file into native PAGX
nodes. When a Layer has explicit `width`/`height`, content is uniformly scaled to fit
(centered, aspect ratio preserved). Otherwise, the Layer's size is set from the source
dimensions. Adds a comment indicating the source (e.g., `<!-- Resolved from: inline svg -->`
or `<!-- Resolved from: assets/logo.svg -->`).

```bash
pagx resolve design.pagx                          # resolve in place
pagx resolve design.pagx -o out.pagx              # resolve to new file
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |

Format-specific options (e.g. `--svg-*`) are shared with `pagx import`; see above for details.

---

## pagx export

Export a PAGX file to another format (e.g. SVG). The output format is inferred from the
output file extension. If neither `--format` nor a recognizable output extension is provided,
the command reports an error.

```bash
pagx export --input icon.pagx                    # PAGX to icon.svg
pagx export --input icon.pagx --output out.svg   # PAGX to out.svg
pagx export --format svg --input icon.pagx       # force SVG output format
pagx export --input icon.pagx --svg-indent 4     # 4-space indent
```

| Option | Description |
|--------|-------------|
| `--input <file>` | Input PAGX file (required) |
| `--output <file>` | Output file (default: `<input>.<format>`) |
| `--format <format>` | Output format (`svg`; inferred from output extension). Required if output has no extension |
| `--svg-indent <n>` | Indentation spaces (default: 2, valid range: 0–16) |
| `--svg-no-xml-declaration` | Omit the `<?xml ...?>` declaration |
| `--svg-no-convert-text-to-path` | Keep text as `<text>` elements instead of `<path>` |


