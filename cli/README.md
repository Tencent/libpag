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

Format and optimize a PAGX file. By default applies structural optimizations in addition
to pretty-printing.

```
pagx format [options] <file.pagx>
```

**Options:**

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (default: overwrite input) |
| `--indent <n>` | Indentation spaces (default: 2) |
| `--no-optimize` | Only format, skip optimizations |

**Automatic optimizations** (unless `--no-optimize` is specified):

- Remove empty elements (empty layers, zero-width strokes, empty groups)
- Remove unused resources (resources with no references)
- Deduplicate PathData (identical paths share a single resource)
- Deduplicate color sources (identical gradients share a single resource)
- Omit default attribute values (handled by the exporter)
- Normalize numeric values (handled by the exporter)
- Simplify transforms (handled by the exporter)
- Standardize attribute ordering (handled by the exporter)

**Example:**

```bash
# Format and optimize in-place
pagx format design.pagx

# Format only (no optimization), output to new file
pagx format --no-optimize -o formatted.pagx design.pagx
```

## Output Formats

Commands that support `--format json` output structured JSON suitable for programmatic
consumption. Without this flag, output is human-readable text on stdout.

Exit codes:
- `0` — Success
- `1` — Error (invalid arguments, file not found, parse failure, etc.)
