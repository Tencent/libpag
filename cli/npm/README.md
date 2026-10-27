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
| `pagx import` | Import SVG/HTML files, web pages, interaction states or recursive batches |
| `pagx validate` | Validate a PAGX file against the specification schema |
| `pagx render` | Render a PAGX file to an image (PNG, WebP, or JPEG) |
| `pagx optimize` | Validate, optimize, and format in one step |
| `pagx format` | Format a PAGX file with consistent indentation and attribute ordering |
| `pagx bounds` | Query the precise rendered bounds of layers |
| `pagx font` | Query a font file, a system font by name, or list system font families |
| `pagx embed` | Embed font glyphs and images into a PAGX file for self-contained output |

## Usage Examples

### Import files, web pages and interaction states

```bash
pagx import icon.svg -o icon.pagx
pagx import page.html -o page.pagx --render
pagx import page.html --html-scenario states.json --output-dir out --preview
pagx import page.html --html-scenario states.json --html-checkpoint advanced -o advanced.pagx
pagx import https://example.com --output-dir out --output-name demo
pagx import --batch pages --output-dir out --render
```

`-o` / `--output` always names one PAGX main file. `--output-dir` names a result set
and is mutually exclusive with `--output`. Local single inputs default to
`<input-stem>.pagx`. URLs require an explicit file output, or a directory plus
`--output-name`. Unselected scenarios require directory output even if the scenario
has just one checkpoint. Batch imports require directory output, preserve relative
paths, and infer HTML/SVG formats per file. Name collisions are rejected before capture.

Import produces PAGX by default. `--render` adds PNGs; `--preview` implies rendering
and adds ordered contact sheets, including for ordinary files and batches.
`--html-keep-snapshot` preserves intermediate HTML. `--images external` (default) writes
image bytes under `<output-stem>.assets/`; `--images embed` keeps them in the PAGX.
Web fonts are acquired for rendering or `--embed-fonts`; `--html-download-fonts` also
preserves them under `<output-stem>.fonts/`. `--no-resolve` keeps import directives
and conflicts with rendering and font embedding. Missing resources or failed
reference expansion are errors, not silently dropped content.

Existing targets require `--overwrite`. This replaces only planned files; it never
clears the output directory. `--dry-run` validates and prints a JSON plan without
capturing, installing a browser, or creating output directories. `--help` needs no
Node/browser runtime. Prepared local HTML can be imported without a browser using
`--html-source snapshot`; the default `browser` mode captures the document first.

Scenario capture exports independent static states in one source-page session.
`--html-checkpoint` executes the necessary preceding actions and stops at the selected
capture. Scenario files own viewport, waits and selection: `--batch`,
`--html-capture-animations`, `--html-scroll-reveal`, `--html-viewport-*`, `--html-wait-ms` and `--html-selector`
cannot be combined with a scenario. `--html-browser-engine playwright` enables role
locators; Puppeteer is the default. Batch HTML inputs have isolated browser contexts.

Directory results include `manifest.json`, with version 1, task `status`
(`running`, `complete`, `failed`, `cancelled`), per-output status, paths, dimensions,
warnings and errors. Paths to artifacts are relative to the manifest. Only
`status: "complete"` indicates that every requested artifact succeeded. Failed
jobs preserve completed outputs and return nonzero. POSIX SIGINT/SIGTERM close
browser children and return 130/143; abrupt termination may leave `running`.

Preview pages are `preview/sheet-NNN.png` in directory mode, or
`<output-stem>.preview/sheet-NNN.png` with a single file output. They use only this
run's PAGX-rendered PNGs, in output order, never old directory contents or another
visit to the source page. `--preview-columns` accepts 1–32 and requires `--preview`.
`pagx preview` remains the separate interactive viewer.

HTML capture requires Node.js 18 or newer. The package bundles its adapter and
installs the selected driver/browser into a user cache on first capture. Native SVG
import and PNG rendering do not need Node or Chromium. Contact sheets use the bundled
Node helper without initializing a browser. Conversion runs in the invoking native
binary; an unrelated `PAGX_BIN` or `pagx` on PATH cannot select another version.

| Environment variable | Purpose |
|---|---|
| `PAGX_HTML_SNAPSHOT_CACHE` | Browser cache; defaults to `~/.cache/libpag-pagx/html-snapshot`, or `%LOCALAPPDATA%/libpag-pagx/html-snapshot` on Windows |
| `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1` | Report a missing browser with installation instructions |
| `PUPPETEER_CACHE_DIR` / `PLAYWRIGHT_BROWSERS_PATH` | Override browser download cache |
| `PUPPETEER_EXECUTABLE_PATH` / `PLAYWRIGHT_EXECUTABLE_PATH` | Use an installed browser executable |
| `PAGX_NODE_BIN` | Override Node; the npm launcher defaults to its own Node |
| `PAGX_HTML_IMPORT_BIN` | Override the private structured import adapter launcher |
| `PAGX_HTML_SNAPSHOT_BIN` | Override the single-document library snapshot helper |

`capture`, `html2pagx` and their old flags are removed without compatibility aliases.
Use `pagx import --help` for supported options.

### HTML import option migration

**Breaking CLI change:** HTML-specific `pagx import` options now use the `--html-` prefix.
Update scripts using the previous names; old names fail with a migration hint and exit code 2.
Defaults and option behavior are unchanged. `--html-source` already uses this naming scheme.

| Previous name | Current name |
|---|---|
| `--scenario` | `--html-scenario` |
| `--checkpoint` | `--html-checkpoint` |
| `--browser-engine` | `--html-browser-engine` |
| `--viewport-width` | `--html-viewport-width` |
| `--viewport-height` | `--html-viewport-height` |
| `--wait-ms` | `--html-wait-ms` |
| `--selector` | `--html-selector` |
| `--cookie` | `--html-cookie` |
| `--header` | `--html-header` |
| `--capture-animations` | `--html-capture-animations` |
| `--scroll-reveal` | `--html-scroll-reveal` |
| `--no-inline-icon-fonts` | `--html-no-inline-icon-fonts` |
| `--download-fonts` | `--html-download-fonts` |
| `--keep-snapshot` | `--html-keep-snapshot` |

Shared options such as `--render`, `--preview`, `--images` and `--embed-fonts` retain their names,
as do SVG options (`--svg-*`). The standalone `snapshot.js` tool retains its own unprefixed
options; the HTML adapter and HTTP JSON option names are unchanged.

### Package validation

For a release, stage the native binary (`./pack.sh --build`) and run
`npm run check:import` on each supported OS/architecture. `prepack` rebuilds the
runtime and checks host capabilities and package/native version agreement.
Test the actual tarball after installation outside the repository:

```bash
PAGX_PACKAGE_TEST_DIR=/absolute/path/to/node_modules/@libpag/pagx \
  npm test --prefix ../../tools/html-snapshot -- --runInBand test/import-package.integration.test.js
```

This test copies browser drivers to an external cache and exercises both Puppeteer
and Playwright. Both browser executables must be provisioned on the test host.

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
