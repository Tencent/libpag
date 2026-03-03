# pagx

Command-line tool for working with [PAGX](https://pag.io/pagx/latest/) files — an XML-based vector
animation format that is human-readable, diff-friendly, and designed for AI-assisted generation.

Use this tool to validate PAGX files against the specification, render them to images, optimize
file structure and size, format for consistent style, query layer bounds, and manage font
embedding.

## Install

```bash
npm install -g pagx
```

## Commands

| Command | Description |
|---------|-------------|
| `pagx validate` | Validate a PAGX file against the specification schema |
| `pagx render` | Render a PAGX file to an image (PNG, WebP, or JPEG) |
| `pagx optimize` | Validate, optimize, and format in one step |
| `pagx format` | Format a PAGX file with consistent indentation and attribute ordering |
| `pagx bounds` | Query the precise rendered bounds of layers |
| `pagx font info` | Query font identity and metrics from a file or system font |
| `pagx font embed` | Embed fonts into a PAGX file with glyph extraction |

## Usage Examples

```bash
# Validate against the specification
pagx validate input.pagx

# Render to PNG (default)
pagx render input.pagx

# Render to WebP at 2x scale with a white background
pagx render --format webp --scale 2 --background "#FFFFFF" input.pagx

# Render a cropped region
pagx render --crop 0,0,200,200 -o cropped.png input.pagx

# Optimize (validate + structural optimization + format)
pagx optimize input.pagx

# Preview optimizations without writing
pagx optimize --dry-run input.pagx

# Format with 4-space indentation
pagx format --indent 4 input.pagx

# Query bounds of a specific layer
pagx bounds --xpath "//Layer[@id='btn']" input.pagx

# Query bounds relative to another layer, output as JSON
pagx bounds --xpath "//Layer[@id='icon']" --relative "//Layer[@id='card']" --json input.pagx

# Query system font metrics at 24pt
pagx font info --name "Arial" --size 24

# Query font metrics from a file
pagx font info --file CustomFont.ttf --json

# Embed fonts with a custom fallback
pagx font embed --file BrandFont.ttf --fallback "Arial" input.pagx
```

## Command Reference

### `pagx validate [options] <file.pagx>`

Validate a PAGX file against the specification schema.

| Option | Description |
|--------|-------------|
| `--json` | Output validation results in JSON format |

### `pagx render [options] <file.pagx>`

Render a PAGX file to an image.

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: input path with format extension) |
| `--format png\|webp\|jpg` | Output format (default: `png`) |
| `--scale <float>` | Scale factor (default: `1.0`) |
| `--crop <x,y,w,h>` | Crop region in document coordinates |
| `--quality <0-100>` | Encoding quality (default: `100`) |
| `--background <color>` | Background color (`#RRGGBB` or `#RRGGBBAA`) |
| `--font <path>` | Register a font file (repeatable) |
| `--fallback <path\|name>` | Add a fallback font file or system font name (repeatable) |

### `pagx optimize [options] <file.pagx>`

Validate, optimize, and format a PAGX file in one step. Aborts if the input fails validation.

Optimizations applied:

1. Remove empty elements (empty Layer/Group, zero-width Stroke)
2. Deduplicate PathData resources
3. Deduplicate gradient resources
4. Merge adjacent Groups with identical painters
5. Replace Path with Rectangle/Ellipse where possible
6. Remove full-canvas clip masks
7. Remove off-canvas invisible layers
8. Localize layer coordinates
9. Extract duplicate layers to compositions
10. Remove unreferenced resources

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--dry-run` | Print optimization report without writing output |

### `pagx format [options] <file.pagx>`

Format a PAGX file with consistent indentation and attribute ordering. Does not modify values or
structure.

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--indent <n>` | Indentation spaces (default: `2`) |

### `pagx bounds [options] <file.pagx>`

Query the precise rendered bounds of Layer nodes.

| Option | Description |
|--------|-------------|
| `--xpath <expr>` | Select nodes by XPath expression |
| `--relative <xpath>` | Output bounds relative to another Layer |
| `--json` | Output in JSON format |

Without `--xpath`, outputs bounds for all layers.

### `pagx font info [options]`

Query font identity and metrics. Requires either `--file` or `--name` (mutually exclusive).

| Option | Description |
|--------|-------------|
| `--file <path>` | Font file path |
| `--name <family,style>` | System font name (e.g., `"Arial"` or `"Arial,Bold"`) |
| `--size <pt>` | Font size in points (default: `12`) |
| `--json` | Output in JSON format |

### `pagx font embed [options] <file.pagx>`

Embed fonts into a PAGX file by performing text layout and glyph extraction.

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--file <path>` | Register a font file (repeatable) |
| `--fallback <path\|name>` | Add a fallback font file or system font name (repeatable) |

## Supported Platforms

- macOS (Apple Silicon and Intel)
- Linux (x64)
- Windows (x64)

## License

Apache-2.0
