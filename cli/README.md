# pagx CLI

Command-line tool for working with PAGX files. Provides validation, rendering, querying,
font embedding, and formatting capabilities.

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
| `--format json` | Output in JSON format (validate, bounds, measure) |
| `--help, -h` | Show help |
| `--version, -v` | Show version |

---

### validate

Validate a PAGX file against the specification.

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

Query the precise bounds of a node or layer.

```
pagx bounds [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `--id <node-id>` | Query bounds of a specific node by ID |
| `--layer <name>` | Query bounds of a layer by name |
| `--format json` | Output bounds as JSON |

**Example:**

```bash
pagx bounds --id myRect design.pagx
pagx bounds --layer "Background" --format json design.pagx
```

---

### measure

Measure visual properties of a PAGX node or layer.

```
pagx measure [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `--id <node-id>` | Measure a specific node by ID |
| `--layer <name>` | Measure a layer by name |
| `--format json` | Output measurements as JSON |

**Example:**

```bash
pagx measure --id textBlock design.pagx
pagx measure --format json design.pagx
```

---

### font

Embed fonts into a PAGX file by performing text layout and glyph extraction.

```
pagx font [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--font <path>` | Extra font file path (can be specified multiple times) |

The command loads the PAGX file, performs text shaping using system fonts and any
additionally specified fonts, embeds the glyph data into the document, and writes the
result back. This ensures cross-platform text rendering consistency.

**Example:**

```bash
# Embed fonts using system fonts only
pagx font design.pagx

# Embed fonts with additional custom fonts
pagx font --font ./fonts/CustomFont.ttf --font ./fonts/Noto.ttf -o output.pagx design.pagx
```

---

### format

Format a PAGX file with consistent indentation and standardized attribute ordering.
This is a pure text-level formatting operation — it does not modify values, remove elements,
or apply any structural optimizations.

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

Apply deterministic structural optimizations to a PAGX file. All optimizations are
equivalent transforms that preserve the original rendering.

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

Commands that support `--format json` output structured JSON suitable for programmatic
consumption. Without this flag, output is human-readable text on stdout.

Exit codes:
- `0` — Success
- `1` — Error (invalid arguments, file not found, parse failure, etc.)
