# CLI Reference

The `pagx` command-line tool provides utilities for working with PAGX files. All commands
operate on local `.pagx` files.

## Setup

The `pagx` binary is provided by the `pagx` npm package. Before running any command below,
ensure it is installed and meets the minimum version:

```bash
PAGX_MIN="0.1.0"
if ! command -v pagx &>/dev/null; then
  npm install -g @libpag/pagx
elif [ "$(printf '%s\n' "$PAGX_MIN" "$(pagx -v | awk '{print $2}')" | sort -V | head -1)" != "$PAGX_MIN" ]; then
  npm update -g @libpag/pagx
fi
```

Run this check before the first `pagx` invocation in each session.

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

Render a PAGX file to an image. By default, output to the same directory as the input file
with the same base name (e.g., `foo.pagx` → `foo.png`).

```bash
pagx render input.pagx                    # outputs input.png
pagx render --format webp --scale 2 input.pagx   # outputs input.webp
pagx render -o output.png input.pagx
pagx render -o cropped.png --crop 50,50,200,200 input.pagx
pagx render -o output.jpg --format jpg --quality 90 --background "#FFFFFF" input.pagx
pagx render --font CustomFont.ttf input.pagx
pagx render --font a.ttf --fallback "PingFang SC" input.pagx
pagx render --font a.ttf --fallback b.otf --fallback "Noto Emoji" input.pagx
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: input path with format extension) |
| `--format png\|webp\|jpg` | Output format (default: png) |
| `--scale <float>` | Scale factor (default: 1.0) |
| `--crop <x,y,w,h>` | Crop region in document coordinates |
| `--quality <0-100>` | Encoding quality (default: 100) |
| `--background <color>` | Background color (#RRGGBB or #RRGGBBAA) |
| `--font <path>` | Register a font file (can be specified multiple times) |
| `--fallback <path\|name>` | Fallback font file or system font name (can be specified multiple times) |

`--font` registers a font file matched by fontFamily/fontStyle against PAGX text references.
`--fallback` accepts either a file path (e.g., `b.otf`) or a system font name (e.g.,
`"PingFang SC"` or `"Arial,Bold"`). All fonts added via `--fallback` are also registered
automatically. Fallback fonts are tried in order when a character is not found in the
primary font. System fallback fonts are always appended after user-specified fallbacks.

**Output convention**: Always output to the same directory as the input `.pagx` file — either omit
`-o` (default behavior) or specify `-o` with a path in the same directory (e.g., for `--crop`
variants). Rendered images are verification artifacts — do not delete them, but if the project
context includes auto-commit rules, do not include these image files (`.png`, `.webp`, `.jpg`)
in the commit.

---

## pagx validate

Validate a PAGX file against the specification schema. Note: `pagx optimize` already includes
validation as its first step — use this command only when you need a standalone check without
running optimizations.

```bash
pagx validate input.pagx
pagx validate --json input.pagx
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
Without `--xpath`, outputs bounds for the entire document and all layers.

```bash
pagx bounds input.pagx                                          # all layers
pagx bounds --xpath "//Layer[@id='btn']" input.pagx              # by id
pagx bounds --xpath "/pagx/Layer[2]/Layer[1]" input.pagx         # by position
pagx bounds --xpath "//Layer[@id='icon']" --relative "//Layer[@id='card']" input.pagx
pagx bounds --json input.pagx
```

| Option | Description |
|--------|-------------|
| `--xpath <expr>` | Select Layer nodes by XPath expression |
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
pagx font embed --file a.ttf --fallback "PingFang SC" --fallback b.otf input.pagx
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--file <path>` | Register a font file (can be specified multiple times) |
| `--fallback <path\|name>` | Fallback font file or system font name (can be specified multiple times) |

`--file` registers a font file matched by fontFamily/fontStyle against PAGX text references.
`--fallback` accepts either a file path (e.g., `b.otf`) or a system font name (e.g.,
`"PingFang SC"` or `"Arial,Bold"`). All fonts added via `--fallback` are also registered
automatically. Fallback fonts are tried in order when a character is not found in the
primary font. System fallback fonts are always appended after user-specified fallbacks.
