# Conversion pipeline reference

Use `pagx import` for HTML/SVG files, URLs, interaction states and batches. The same command
works with a repository build or the npm package. The old `html2pagx` entrypoint and its flags
have been removed without compatibility aliases.

Read [CLI options and output rules](cli.md#pagx-import) for the command contract,
[Animation](animation.md) for motion, and [Interaction capture](interaction.md) for scenarios.
Paths below are relative to the repository root where repository tools are used.

## Table of contents

- [Setup details](#setup-details)
- [Ordinary conversion](#ordinary-conversion)
- [Fonts and images](#fonts-and-images)
- [Result sets and batches](#result-sets-and-batches)
- [Manual step-by-step pipeline](#manual-step-by-step-pipeline)
- [Warm HTTP service](#warm-http-service)
- [Conversion warnings](#conversion-warnings)
- [Troubleshooting](#troubleshooting)

## Setup details

**Outside the repository:** install `@libpag/pagx` if needed, then check `pagx -v` and
`pagx import --help` for the options required by the task. The package bundles the import
adapter; its launcher installs the selected browser driver/browser into a user cache on first
capture. HTML capture requires Node.js 18 or newer. Native SVG import and PNG rendering do not
require a browser. Contact sheets use Node but do not launch a browser.

**Inside the repository:** build and use the local CLI, then prepare the snapshot tool:

```bash
cmake --build cmake-build-debug --target pagx
export PATH="$(git rev-parse --show-toplevel)/cmake-build-debug:$PATH"
node .codebuddy/skills/pagx/scripts/setup.js
pagx import --help
```

`cmake-build-debug` is the usual build directory; use the project's configured build directory
if different. In shells without `export`, invoke the binary by its path or set PATH using that
shell's syntax. Rebuild the native CLI after C++ changes and the snapshot tool after TypeScript
changes. The setup script installs dependencies when `node_modules` is absent and rebuilds the
snapshot tool. Run `npm install` in `tools/html-snapshot` after dependency changes. For manual
setup, run `npm install` followed by `npm run build` there.

If Chromium is missing, follow the install command printed by setup. For repository Playwright
use, provision its browser as well (`npx playwright install chromium` from `tools/html-snapshot`).

| Environment variable | Purpose |
|---|---|
| `PAGX_NODE_BIN` | Node executable override; the npm launcher defaults to its own Node |
| `PAGX_HTML_IMPORT_BIN` | Override the structured import adapter launcher |
| `PAGX_HTML_SNAPSHOT_CACHE` | npm runtime cache; defaults to `~/.cache/libpag-pagx/html-snapshot`, or `%LOCALAPPDATA%/libpag-pagx/html-snapshot` on Windows |
| `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1` | Report missing browser dependencies with installation instructions instead of installing them |
| `PUPPETEER_CACHE_DIR` / `PLAYWRIGHT_BROWSERS_PATH` | Driver-specific browser cache location |
| `PUPPETEER_EXECUTABLE_PATH` / `PLAYWRIGHT_EXECUTABLE_PATH` | Use an installed browser executable |

The native import workflow locates `import-adapter.js` in the repository or its packaged
launcher. `PAGX_HTML_SNAPSHOT_BIN` configures the separate single-document library helper;
it is not the public import adapter override. Select prepared HTML with `--html-source snapshot`.

## Ordinary conversion

```bash
pagx import page.html -o page.pagx --render
```

The browser captures the page's supported layout, paint, text and resources into a subset.
The native importer converts it, resolves SVG/import directives and optimizes the document.
PAGX is the default output; `--render` additionally writes `page.png`.

- Set `--viewport-width` / `--viewport-height` to the intended CSS viewport (default 1400×900).
  Use `--scale` with rendering to change PNG resolution without changing the layout.
- `--selector` waits for an element; `--wait-ms` adds a settle delay (default 800 ms).
  For a specific application state, use a scenario with explicit readiness conditions.
- Animation capture is off by default. Add `--capture-animations` for a playable timeline.
- `--keep-snapshot` retains `page.subset.html` for inspection; the default does not retain it.
- `--no-resolve` keeps import directives. It conflicts with `--render`, `--preview` and
  `--embed-fonts`; resolve the document before subsequent rendering.

Examples below are independent. When intentionally regenerating an existing output, add
`--overwrite`; it replaces planned artifacts without clearing the output directory.

```bash
# A design with a fixed target viewport and portable resources.
pagx import poster.html -o poster.pagx --viewport-width 1080 --viewport-height 1440 \
  --embed-fonts --images embed --render --scale 2

# A web page, with an explicit output filename.
pagx import https://example.com/promo -o promo.pagx --render

# A supported animation timeline.
pagx import motion.html -o motion.pagx --capture-animations --render
```

URL inputs accept repeatable `--cookie 'name=value'` and `--header 'Key: Value'` options.
Local files are opened as `file:` URLs, so ordinary relative assets resolve against the HTML
location. Apps needing HTTP-origin features should be served locally and imported by URL.

## Fonts and images

Browser font loading and portable PAGX font storage are separate steps:

- `--render` (also implied by `--preview`) acquires web fonts and uses them for that import/render.
  It does not embed them by default.
- `--embed-fonts` acquires web fonts and embeds shaped glyphs into the PAGX for use on other hosts.
  This is useful when the deliverable must reproduce the captured typography independently of
  installed fonts. It does not preserve downloaded font files unless requested separately.
- `--download-fonts` also preserves acquired files under `<output-stem>.fonts/`. Use those files
  as `--fallback` inputs for later rendering or embedding. Check the actual family, weight and
  glyph coverage when a face differs from the browser result.

For an existing PAGX and a local font file:

```bash
pagx render page.pagx --fallback ./CustomFont.ttf -o page.png
pagx embed page.pagx --fallback ./CustomFont.ttf
```

Final image storage is controlled by `--images`:

- `external` (default) writes loaded image bytes under `<output-stem>.assets/` and references them
  relatively. Deliver this directory with the PAGX. This also applies to captured inline images.
- `embed` keeps image bytes inside the PAGX. Combine with `--embed-fonts` for portable resources
  in a single document.

`--image-base-dir` overrides relative image lookup for local inputs. Missing resources fail the
import. `--no-resolve` does not silently disable image processing. Standalone snapshot options
such as `--download-images` are not flags of the public `pagx import` command.

## Result sets and batches

`-o` / `--output` names one PAGX file. `--output-dir` names a result set and is mutually exclusive
with `-o`. URL directory output additionally needs `--output-name`.

```bash
pagx import https://example.com/promo --output-dir out --output-name promo --preview
pagx import --batch pages --output-dir batch-out --render
pagx import app.html --scenario states.json --output-dir states-out --preview
```

Batch mode recursively imports HTML/SVG inputs, preserves relative directories and isolates
HTML browser contexts. It cannot be combined with a positional input, `--format`, `--output-name`
or `--scenario`. See [Interaction capture](interaction.md) for scenario output selection.

Directory mode writes `manifest.json` with artifact paths relative to the manifest. Require both
a successful exit code and top-level `status: "complete"`. Failed/cancelled jobs may retain completed
outputs; do not treat partial files or an old preview as a completed job. Name collisions are
rejected before capture. `--dry-run` prints a validated JSON plan without capturing, installing
a browser or creating output directories.

`--preview` adds rendered PNGs and ordered contact sheets from this run's outputs. Sheets live
in `preview/sheet-NNN.png` for directory output, or `<output-stem>.preview/sheet-NNN.png` for file
output. `--preview-columns` accepts 1–32. These are static overview images; `pagx preview` is the
separate live viewer. A sheet does not sample animation playback times.

## Manual step-by-step pipeline

Use this repository debugging path when you need to inspect or edit the prepared subset:

```bash
node tools/html-snapshot/snapshot.js page.html \
  --viewport-width 1080 --viewport-height 1440 -o page.subset.html
pagx import page.subset.html --html-source snapshot -o page.pagx --render
```

`--html-source snapshot` consumes a local, subset-compliant HTML file without a browser.
Use it for generated snapshots or deliberately authored subset markup. It cannot evaluate JS or
general browser CSS, fetch a URL, or accept browser/scenario options. If the snapshot references
resources elsewhere, preserve their relative paths or supply `--image-base-dir`.

For manual animated capture, pass `--capture-animations` to `snapshot.js`; the prepared subset
already carries the animation data, so do not pass that browser flag to the subsequent import.
Web font files acquired separately must also be available for later shaping/embedding.

## Warm HTTP service

For repository integrations that need a persistent snapshot service:

```bash
node tools/html-snapshot/server.js
```

The default endpoint is `http://127.0.0.1:8787/snapshot`. It accepts HTML or URL input and can
return subset HTML or PAGX. Use the native import workflow above for full artifact sets and
portable resources. The service's PAGX output can retain import directives; resolve them before
rendering. Consult `tools/html-snapshot/README.md` in the checkout for its HTTP-specific options.

## Conversion warnings

Use `-v` to print warnings; directory manifests also record warnings per output. Judge them
against the requested result rather than assuming every downgrade is harmless.

- `subset:unit-coerced` / `subset:flex-shorthand-collapsed`: normalization; check geometry.
- `subset:unsupported-property` / `subset:unsupported-tag`: something was skipped or downgraded.
  Browser source HTML and direct subset HTML have different boundaries; consult
  [Authoring HTML](authoring-html.md) and [the subset specification](../spec/html_subset.md).
- `subset:flex-inference-skipped`: absolute layout was retained. Paint-planned containers must
  preserve their emitted order; do not force flex inference just to remove this warning.
- `paint:preserve-3d`, `paint:top-layer`, `paint:animated-stacking`: the supported paint model cannot
  fully reproduce the source's 3D, top-layer or changing stacking behavior. Inspect those regions.
- Scenario raster fallback warnings mean a whole viewport became an image; disclose the loss of
  editability when delivering that result.

## Troubleshooting

| Symptom | Check / action |
|---|---|
| Unknown option or missing command | Check the selected binary with `pagx -v` and command help; update the package or rebuild the checkout as appropriate. |
| First HTML capture downloads a browser | Expected for the npm runtime; follow its printed instructions if automatic installation fails. |
| Missing adapter, dependency or `dist` module in a checkout | Run setup, or install dependencies and rebuild `tools/html-snapshot`; check `PAGX_HTML_IMPORT_BIN` if overridden. |
| Output already exists | Use a new destination, or `--overwrite` for an intentional regeneration. |
| Blank or clipped PNG | Check viewport, readiness and warnings; use `--keep-snapshot` to inspect the captured content. |
| Wrong font / weight | Check that the intended faces loaded, use `--render` for captured-font rendering and `--embed-fonts` for portable output. |
| Missing local asset | Check the HTML-relative path and browser access; use an HTTP URL when the application requires an HTTP origin. |
| Missing chart | Wait for it to draw; canvas pixel capture also requires readable pixels. Use an SVG or image alternative when needed. |
| Missing motion | Add `--capture-animations`, then inspect attachment/timing and supported channels in [Animation](animation.md). |
| Missing state or unsupported scenario content | Check explicit `waitFor` conditions and [capture boundaries](interaction.md#capture-boundaries); do not silently substitute another state. |
