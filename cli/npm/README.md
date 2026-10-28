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
| `pagx import` | Import SVG/HTML, URLs, or interaction checkpoints into PAGX with optional PNGs and previews |
| `pagx validate` | Validate a PAGX file against the specification schema |
| `pagx render` | Render a PAGX file to an image (PNG, WebP, or JPEG) |
| `pagx optimize` | Validate, optimize, and format in one step |
| `pagx format` | Format a PAGX file with consistent indentation and attribute ordering |
| `pagx bounds` | Query the precise rendered bounds of layers |
| `pagx font` | Query a font file, a system font by name, or list system font families |
| `pagx embed` | Embed font glyphs and images into a PAGX file for self-contained output |

## Usage Examples

### Import documents and interaction states

HTML acquisition requires Node.js 18 or newer. The package includes the import runtime;
the selected browser driver and Chromium are installed into a user cache on first use.
Basic SVG and prepared-subset conversion need only the native binary. Rendering and
contact sheets support both SVG and HTML; SVG previews never initialize a browser.

```bash
# PAGX only; resolve and optimize are enabled by default
pagx import page.html -o page.pagx
pagx import icon.svg --output-dir out --preview

# Replay once and export all named states; retain subsets for inspection
pagx import page.html --html-scenario states.json --output-dir out --html-keep-subset --preview

# Replay preceding actions and export exactly one state to an explicit file
pagx import page.html --html-scenario states.json --html-checkpoint advanced -o advanced.pagx

# URL directory output requires a basename; an explicit -o demo.pagx also works
pagx import https://example.com --output-name demo --output-dir out --render

# Batch: one browser, independent contexts, mirrored output directories
pagx import --html-batch pages --output-dir out --preview
pagx import --html-batch pages --output-dir out --dry-run

# Playwright supports role locators in scenarios
pagx import page.html --html-scenario states.json --html-browser-engine playwright --output-dir out

# Convert an already prepared subset without another browser pass
pagx import page.subset.html --html-no-snapshot -o page.pagx
```

`-o` / `--output` always names one PAGX **file**. `--output-dir` names an artifact
**directory**; the two are mutually exclusive. Multiple checkpoints require
`--output-dir`; the runtime never silently selects the first state. An explicit
`--output-name` requires directory output. Local single results default beside the
input; URLs always require an explicit output. `--input` is an alternative to a
single positional input, not an additional input.

PAGX is the default output. `--render` adds PNG, `--render-scale` controls its scale,
and `--preview` implies rendering and produces `<base>.preview/sheet-NNN.png` contact
sheets. `--preview-columns` accepts 1–32. These options do not open a browser viewer;
`pagx preview` is a separate viewer command. `--no-resolve` conflicts with rendering
and preview. Subset HTML is retained only with `--html-keep-subset`.

Scenario directory outputs use `<base>.<checkpoint>.pagx`, optional `.png` and
`.subset.html`, plus `<base>.captures.json`. With an explicit output file, sidecars
use that file's stem. States are independent static documents. The version-1
manifest records `running`, `complete`, or `failed`, completed `outputs`, and optional
preview status/pages. Read the manifest instead of scanning the directory: older
unlisted files may remain after a shorter run. A later interaction or preview failure
returns nonzero, preserves completed exports, and records failure. Cancellation
stops further actions and conversions and closes the process/browser tree.

HTML-specific options use `--html-`: scenario/checkpoint, browser-engine,
capture-animations, viewport-width/height, wait-ms, selector, scroll-reveal,
no-inline-icon-fonts, download-fonts, embed-fonts, font-dir, download-images,
image-dir, cookie and header. Cookie/header are repeatable and require a URL.
`--html-embed-fonts` implies font download. Scenario mode owns viewport/readiness
and rejects animation capture, viewport/wait/selector/scroll flags and HTML batch.
`--html-no-snapshot` requires local HTML and excludes capture options.

Image storage uses the common `--images external|embed` and `--image-base-dir`.
SVG options also apply to SVG resources resolved inside HTML. The import runtime
always uses the invoking native binary; there is no binary override argument.
Help, invalid arguments and `--dry-run` do not install or start a browser.

Batch `--skip-existing` requires a complete `<base>.import.json` record matching the
source, native binary, options and artifact stamps, including referenced resources.
A lone PAGX or a missing PNG cannot cause a skip. Batch previews in
`<output-dir>/preview/` contain only newly completed inputs, in order; skipped and
failed inputs are excluded. An entirely skipped batch generates no preview.

Runtime configuration:

| Environment variable | Purpose |
|---|---|
| `PAGX_HTML_SNAPSHOT_CACHE` | Browser cache (default `~/.cache/libpag-pagx/html-snapshot`; Windows: `%LOCALAPPDATA%/libpag-pagx/html-snapshot`) |
| `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1` | Report a missing browser instead of installing it |
| `HTML_SNAPSHOT_BROWSER` | Default engine; `--html-browser-engine` takes precedence |
| `PUPPETEER_CACHE_DIR` / `PLAYWRIGHT_BROWSERS_PATH` | Browser download cache |
| `PUPPETEER_EXECUTABLE_PATH` / `PLAYWRIGHT_EXECUTABLE_PATH` | Preinstalled browser executable |
| `PAGX_NODE_BIN` | Node executable; npm defaults to its current Node |
| `PAGX_IMPORT_BIN` | Import runtime launcher override |
| `PAGX_HTML_SNAPSHOT_BIN` | Single-document snapshot launcher used when resolving HTML imports |

Run `npm run check:import` with the staged native binary on each supported
OS/architecture. `prepack` builds the runtime and checks host binary/version
agreement and import capabilities. Stage the native binary before packing
(`./pack.sh --build`). Test the installed tarball outside the repository:

```bash
PAGX_PACKAGE_TEST_DIR=/absolute/path/to/node_modules/@libpag/pagx \
  npm test --prefix ../../tools/html-snapshot -- --runInBand test/import-package.integration.test.js
```

This test copies drivers to a temporary external cache and checks both Puppeteer
and Playwright. Provision the HTML test dependencies and both browsers first.

### Other examples

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

# Embed with fallback fonts
pagx embed --fallback "Arial" input.pagx

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
| `--fallback <path\|name>` | Add a fallback font file or system font name (repeatable) |
| `--skip-fonts` | Skip font embedding |
| `--skip-images` | Skip image embedding |

## Supported Platforms

- macOS (Apple Silicon and Intel)
- Linux (x64)
- Windows (x64)

## License

Apache-2.0
