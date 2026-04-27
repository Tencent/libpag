# pagx

Command-line tool for working with [PAGX](https://pag.io/pagx/latest/) files — an XML-based vector
animation format that is human-readable, diff-friendly, and designed for AI-assisted generation.

Use this tool to validate PAGX files against the specification, render them to images, optimize
file structure and size, format for consistent style, query layer bounds, and manage font
embedding.

## Install

```bash
npm install -g @libpag/pagx
```

## Commands

| Command | Description |
|---------|-------------|
| `pagx validate` | Validate a PAGX file against the specification schema |
| `pagx render` | Render a PAGX file to an image (PNG, WebP, or JPEG) |
| `pagx optimize` | Validate, optimize, and format in one step |
| `pagx format` | Format a PAGX file with consistent indentation and attribute ordering |
| `pagx bounds` | Query the precise rendered bounds of layers |
| `pagx font` | Query a font file, a system font by name, or list system font families |
| `pagx embed` | Embed font glyphs and images into a PAGX file for self-contained output |

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

# Render only a specific layer by id
pagx render --id badge -o badge.png input.pagx

# Render only a specific layer by XPath
pagx render --xpath "//Layer[@id='card']" -o card.png input.pagx

# Optimize (validate + structural optimization + format)
pagx optimize input.pagx

# Preview optimizations without writing
pagx optimize --dry-run input.pagx

# Format with 4-space indentation
pagx format --indent 4 input.pagx

# Query bounds of a specific layer by id
pagx bounds --id btn input.pagx

# Query bounds of a specific layer by XPath
pagx bounds --xpath "//Layer[@id='btn']" input.pagx

# Query bounds relative to another layer, output as JSON
pagx bounds --xpath "//Layer[@id='icon']" --relative "//Layer[@id='card']" --json input.pagx

# Query system font metrics at 24pt
pagx font --name "Arial" --size 24

# Query font metrics from a file
pagx font --file CustomFont.ttf --json

# List all installed system font families
pagx font --list

# Embed fonts and images into a PAGX file
pagx embed input.pagx

# Embed with a custom font file and fallback
pagx embed --file BrandFont.ttf --fallback "Arial" input.pagx

# Embed images only (skip font embedding)
pagx embed --skip-fonts input.pagx
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
| `--id <id>` | Render only the Layer with the specified id |
| `--xpath <expr>` | Render only the Layer matched by XPath expression |
| `--crop <x,y,w,h>` | Crop region in document coordinates (relative to target Layer bounds when combined with `--id` or `--xpath`) |
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

Query the precise rendered bounds of Layer nodes. Bounds are reported in global (canvas)
coordinates by default.

| Option | Description |
|--------|-------------|
| `--id <id>` | Select a Layer by its id attribute |
| `--xpath <expr>` | Select nodes by XPath expression |
| `--relative <xpath>` | Output bounds relative to another Layer |
| `--json` | Output in JSON format |

`--id` and `--xpath` are mutually exclusive. Without either, outputs bounds for all layers.

### `pagx font [options]`

Query a font file, a system font by name, or list system font families. Exactly one of `--file`,
`--name`, or `--list` must be specified.

| Option | Description |
|--------|-------------|
| `--file <path>` | Query a font file |
| `--name <family,style>` | Query a system font (e.g., `"Arial"` or `"Arial,Bold"`) |
| `--size <pt>` | Font size in points (default: `12`) |
| `--json` | Output in JSON format |
| `--list` | List every installed system font family |

### `pagx embed [options] <file.pagx>`

Embed font glyphs and images into a PAGX file for self-contained output.

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--file <path>` | Register a font file (repeatable) |
| `--fallback <path\|name>` | Add a fallback font file or system font name (repeatable) |
| `--skip-fonts` | Skip font embedding |
| `--skip-images` | Skip image embedding |

## Supported Platforms

- macOS (Apple Silicon and Intel)
- Linux (x64)
- Windows (x64)

## License

Apache-2.0
