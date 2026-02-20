# pagx CLI

Command-line tool for working with PAGX files. Provides validation, rendering, querying,
font operations, and formatting capabilities.

## Build

```bash
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target pagx-cli
```

The binary is output as `pagx` in the build directory.

## Usage

```
pagx <command> [options] <file>
```

### Global Options

| Option | Description |
|--------|-------------|
| `--help, -h` | Show help |
| `--version, -v` | Show version |

---

### validate

Validate a PAGX file against the specification. Note: `pagx optimize` already includes
validation as its first step — use this command only when you need a standalone check without
running optimizations.

```
pagx validate [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `--format json` | Output validation results as JSON |

**Example:**

```bash
pagx validate design.pagx
pagx validate --format json design.pagx
```

---

### render

Render a PAGX file to an image. Supports crop and scale options.

```
pagx render [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output image path (PNG or WebP, default: `output.png`) |
| `--scale <factor>` | Scale factor for rendering (default: 1) |
| `--crop <x,y,w,h>` | Crop region in canvas coordinates |

**Example:**

```bash
pagx render -o preview.png design.pagx
pagx render --scale 2 -o retina.png design.pagx
pagx render --crop 0,0,400,300 -o cropped.png design.pagx
```

---

### bounds

Query precise rendered bounds of Layer nodes. Supports XPath expressions for node selection
and optional relative coordinate output.

```
pagx bounds [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `--xpath <expr>` | Select nodes by XPath expression |
| `--relative <xpath>` | Output bounds relative to another Layer |
| `--json` | Output in JSON format |
| `--format json` | Same as `--json` |

Without `--xpath`, outputs bounds for all layers.

**XPath examples:**

```bash
pagx bounds --xpath "//Layer[@id='btn']" design.pagx         # Layer with id 'btn'
pagx bounds --xpath "/pagx/Layer[1]" design.pagx              # First top-level Layer
pagx bounds --xpath "/pagx/Layer[2]/Layer" design.pagx        # Child Layers of second Layer
pagx bounds --xpath "//Layer[@id='icon']" --relative "//Layer[@id='card']" design.pagx  # relative
pagx bounds --json design.pagx                                # all layers as JSON
```

---

### font

Font operations: querying font metrics and embedding fonts into PAGX files.

```
pagx font <subcommand> [options]
```

#### font info

Query font identity and metrics from a font file or system font.

```
pagx font info [options]
```

**Options:**

| Option | Description |
|--------|-------------|
| `--file <path>` | Font file path |
| `--name <family[,style]>` | System font name (e.g., `"Arial"` or `"Arial,Bold"`) |
| `--size <pt>` | Font size in points (default: 12) |
| `--json` | Output in JSON format |

Either `--file` or `--name` is required (mutually exclusive).

**Example:**

```bash
pagx font info --file ./fonts/CustomFont.ttf
pagx font info --name "PingFang SC,Bold" --size 24
pagx font info --name "Arial" --json
```

#### font embed

Embed fonts into a PAGX file by performing text layout and glyph extraction.

```
pagx font embed [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--file <path>` | Add a font file (can be specified multiple times) |
| `--fallback <family[,style]>` | Add a fallback font (can be specified multiple times) |

Added font files are matched by fontFamily/fontStyle against PAGX text references. Fallback
fonts are tried in order when a character is not found in the primary font — they can
reference loaded `--file` fonts or system fonts by name.

**Example:**

```bash
# Embed fonts using system fonts only
pagx font embed design.pagx

# Embed with custom fonts and fallback chain
pagx font embed --file ./fonts/Brand.ttf --fallback "PingFang SC" --fallback "Noto Emoji" -o output.pagx design.pagx
```

---

### format

Format a PAGX file with consistent indentation and standardized attribute ordering.
This is a pure text-level formatting operation — it does not modify values, remove elements,
or apply any structural optimizations. Note: `pagx optimize` already formats its output —
use this command only when you want to reformat without applying optimizations.

```
pagx format [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--indent <n>` | Indentation spaces (default: 2) |

**Example:**

```bash
# Format in-place with default 2-space indentation
pagx format design.pagx

# Format with 4-space indentation, output to new file
pagx format --indent 4 -o formatted.pagx design.pagx
```

---

### optimize

Validate, optimize, and format a PAGX file in one step. Validates input against the XSD
schema first — aborts with errors if invalid. Then applies deterministic structural
optimizations (all equivalent transforms that preserve the original rendering) and exports
with consistent formatting. There is no need to run `pagx validate` or `pagx format`
separately after optimize.

```
pagx optimize [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--dry-run` | Print optimization report without writing the file |

**Optimizations applied:**

| # | Optimization | Description |
|---|-------------|-------------|
| 1 | Remove empty elements | Empty Layer/Group, zero-width Stroke, empty Resources |
| 2 | Deduplicate PathData | Identical path data strings share a single PathData resource |
| 3 | Deduplicate gradients | Identical gradient definitions share a single resource |
| 4 | Remove unreferenced resources | Resources with no `@id` references are removed |
| 5 | Replace Path with primitive | Path describing a Rectangle or Ellipse (with rounding detection) is replaced with the primitive element |
| 6 | Remove full-canvas clips | Clip mask covering the entire canvas is removed along with its reference |
| 7 | Remove off-canvas layers | Layers whose bounds do not intersect the canvas are removed |

The exporter also normalizes the output: omits default attribute values, normalizes numbers,
simplifies transforms, standardizes attribute ordering, and moves Resources to the end.

**Example:**

```bash
# Optimize in-place
pagx optimize design.pagx

# Optimize to a new file
pagx optimize -o optimized.pagx design.pagx

# Preview optimizations without writing
pagx optimize --dry-run design.pagx
```

## Output Formats

Commands that support `--json` or `--format json` output structured JSON suitable for
programmatic consumption. Without this flag, output is human-readable text on stdout.

Exit codes:
- `0` — Success
- `1` — Error (invalid arguments, file not found, parse failure, etc.)
