# CLI Reference

Back to main: [SKILL.md](../SKILL.md)

The `pagx` command-line tool provides utilities for working with PAGX files. All commands
operate on local `.pagx` files.

---

## pagx optimize

Validates, optimizes, and formats a PAGX file in one step. First validates the input against
the specification schema — aborts with errors if invalid. Then applies structural optimizations:
empty element removal, PathData/gradient dedup, unreferenced resource removal,
Path→Rectangle/Ellipse replacement, full-canvas clip mask removal, off-canvas layer removal,
coordinate localization, and Composition extraction. The exporter also handles default value
omission, number normalization, transform simplification, Resources ordering, and consistent
formatting.

```bash
pagx optimize -o output.pagx input.pagx
pagx optimize --dry-run input.pagx       # preview only
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--dry-run` | Print report without writing output |

---

## pagx render

Render a PAGX file to an image.

```bash
pagx render -o output.png input.pagx
pagx render -o output.webp --format webp --scale 2 input.pagx
pagx render -o cropped.png --crop 50,50,200,200 input.pagx
pagx render -o output.jpg --format jpg --quality 90 --background "#FFFFFF" input.pagx
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (required) |
| `--format png\|webp\|jpg` | Output format (default: png) |
| `--scale <float>` | Scale factor (default: 1.0) |
| `--crop <x,y,w,h>` | Crop region in document coordinates |
| `--quality <0-100>` | Encoding quality (default: 100) |
| `--background <color>` | Background color (#RRGGBB or #RRGGBBAA) |

---

## pagx validate

Validate a PAGX file against the specification schema. Note: `pagx optimize` already includes
validation as its first step — use this command only when you need a standalone check without
running optimizations.

```bash
pagx validate input.pagx
pagx validate --format json input.pagx
```

Text output: `filename:line: error message`. JSON output includes `file`, `valid`, and `errors`
array with `line` and `message` fields.

---

## pagx format

Pretty-print a PAGX file with consistent indentation and attribute ordering. Does not modify
values or structure. Note: `pagx optimize` already formats its output — use this command only
when you want to reformat without applying optimizations.

```bash
pagx format -o output.pagx input.pagx
pagx format --indent 4 input.pagx       # 4-space indent (default: 2)
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--indent <n>` | Indentation spaces (default: 2) |

---

## pagx bounds

Query precise rendered bounds of Layer nodes. Supports XPath expressions for node selection.
Without `--path`, outputs bounds for the entire document and all layers.

```bash
pagx bounds input.pagx                                          # all layers
pagx bounds --path "//Layer[@id='btn']" input.pagx              # by id
pagx bounds --path "/pagx/Layer[2]/Layer[1]" input.pagx         # by position
pagx bounds --path "//Layer[@id='icon']" --relative "//Layer[@id='card']" input.pagx
pagx bounds --json input.pagx
```

| Option | Description |
|--------|-------------|
| `--path <xpath>` | Select Layer nodes by XPath expression |
| `--relative <xpath>` | Output bounds relative to another Layer |
| `--json` | JSON output |

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
pagx font embed --file a.ttf --fallback "PingFang SC" --fallback "Noto Emoji" input.pagx
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--file <path>` | Add a font file (can be specified multiple times) |
| `--fallback <family[,style]>` | Fallback font in order (can be specified multiple times) |

Added `--file` fonts are matched by fontFamily/fontStyle against PAGX text references.
`--fallback` fonts are tried in order when a character is not found in the primary font —
they can reference loaded `--file` fonts or system fonts by name.
