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

Validate a PAGX file against the specification schema.

```bash
pagx validate input.pagx
pagx validate --format json input.pagx
```

Text output: `filename:line: error message`. JSON output includes `file`, `valid`, and `errors`
array with `line` and `message` fields.

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

## pagx bounds

Query precise rendered bounds of the document or specific nodes. Without `--id`, outputs
bounds for the entire document and all layers.

```bash
pagx bounds input.pagx                   # all layers
pagx bounds --id myLayer input.pagx      # specific node
pagx bounds --format json input.pagx
```

| Option | Description |
|--------|-------------|
| `--id <nodeId>` | Query a specific node by id |
| `--format json` | JSON output |

---

## pagx measure

Measure font metrics and text dimensions.

```bash
pagx measure --font "Arial" --size 24 --text "Hello World"
pagx measure --font "PingFang SC" --size 16 --style Bold --text "测试" --format json
```

| Option | Description |
|--------|-------------|
| `--font <family>` | Font family name (required) |
| `--size <pt>` | Font size in points (required) |
| `--text <string>` | Text to measure (required) |
| `--style <style>` | Font style (e.g., Bold, Italic) |
| `--letter-spacing <float>` | Extra spacing between characters |
| `--format json` | JSON output |

Returns: fontFamily, fontSize, ascent, descent, leading, capHeight, xHeight, width, charCount.

---

## pagx font

Embed fonts into a PAGX file by performing text layout and glyph extraction.

```bash
pagx font -o output.pagx input.pagx
pagx font --font extra.ttf -o output.pagx input.pagx   # with additional font file
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--font <path>` | Additional font file (can be specified multiple times) |
