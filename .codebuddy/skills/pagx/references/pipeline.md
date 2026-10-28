# Conversion pipeline reference

Use this reference for HTML conversion setup, output sets, resource handling and failures.
[CLI](cli.md) owns the command option tables; [Interaction capture](interaction.md) owns the
scenario format. Paths in repository examples are relative to the libpag root.

## Choose the output

`pagx import` is the public entry for SVG, HTML files/URLs, interaction scenarios and HTML batches.
HTML normally passes through a browser snapshot, then native import, resolution and optimization.
Ordinary HTML capture is static unless `--html-capture-animations` is supplied. For stateful UI,
use [Interaction capture](interaction.md), which exports static checkpoints and cannot be combined
with animation capture.

- `-o` / `--output` names one PAGX file. A local single-result import defaults beside its input.
- `--output-dir` selects an artifact directory and is required for multiple results. It is
  mutually exclusive with `--output`; optional `--output-name` sets the base name in that directory.
- `--render` adds PNGs, `--html-keep-subset` retains intermediate HTML, and `--preview` adds PNGs
  plus static contact sheets. These artifacts are opt-in. Resource options work with either
  file or directory output.

The native CLI invokes the HTML runtime and uses that same native executable for conversion and
rendering. Select another build by invoking its path; `import` has no `--pagx-bin` option.
The former `capture` and `html2pagx` entry points are removed. The repository's `import.js` is an
internal request handler, not an alternative public CLI. `snapshot.js` remains a separate
single-subset tool for [manual debugging](#manual-subset-import).

## Setup

Select the executable before checking the needed commands with `--help`. Missing command support
requires a suitable build/package, not guessed flags. See [Environment selection](../SKILL.md#environment-and-command-selection).

**Complete npm installation.** The package bundles the HTML import runtime. Its launcher
prepares the browser driver and browser on first use in a per-user cache. Override that cache
with `PAGX_HTML_SNAPSHOT_CACHE`; `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1` disables automatic installation
and makes missing dependencies an actionable error. Help, invalid import arguments and batch
`--dry-run` do not start a browser or initialize that cache. A standalone native binary still
needs Node and a matching HTML runtime; it is not a substitute for the complete package.

**Repository build.** Build the native `pagx` from the configured CMake directory. Then run:

```bash
node .codebuddy/skills/pagx/scripts/setup.js --pagx-bin ./cmake-build-debug/pagx
# Read-only health check; reports missing/stale build outputs instead of rebuilding:
node .codebuddy/skills/pagx/scripts/setup.js --pagx-bin ./cmake-build-debug/pagx --check
```

The script uses the local native executable (or `--pagx-bin` / `PAGX_BIN`), never installs a global
CLI as a substitute for a missing repository build. It reports missing snapshot dependencies with
the appropriate `npm install` command, rebuilds TypeScript unless `--check` was requested, and
checks the actual executable selected by the browser driver. A batch dry-run checks that the
native bridge reaches its configured Node runtime without generating outputs. This does not test
browser launch or visual fidelity. Run setup again after source/dependency changes or setup
failures, rather than before every conversion.

The default browser driver is Puppeteer. Select Playwright with `--html-browser-engine playwright`
on `import`, or `HTML_SNAPSHOT_BROWSER=playwright` in the environment. It must have its own driver
and browser installed. `PUPPETEER_EXECUTABLE_PATH` / `PLAYWRIGHT_EXECUTABLE_PATH` override the
corresponding executable; a populated cache directory alone does not establish that it is usable.

**Runtime lookup.** `PAGX_NODE_BIN` selects Node. `PAGX_IMPORT_BIN` overrides the import runtime
launcher. `PAGX_HTML_SNAPSHOT_BIN` selects the separate single-document snapshot launcher used
when resolving HTML import directives. The native bridge checks an explicit override first,
then the bundled runtime near the executable,
then the repository runtime found from the working directory or executable location. An invalid
explicit override is an error, not a request to search for a different runtime.

## Ordinary files and URLs

```bash
# One file and a separate render.
pagx import --input page.html --output page.pagx
pagx render page.pagx -o page.png

# Optional artifacts with a viewport matching the design.
pagx import page.html --output-dir out --render --html-keep-subset \
  --html-viewport-width 1080 --html-viewport-height 1440

# URL input needs an exact output file, or a directory and base name.
pagx import https://example.com/promo --output-dir out --output-name promo

# Capture playable motion, including resource embedding when required.
pagx import motion.html -o motion.pagx --html-capture-animations --html-embed-fonts --images embed
```

Ordinary import writes a PAGX, with `<base>.png` and `<base>.subset.html` only when requested.
It does not create a scenario manifest. An exact output file supplies the stem for its sidecars;
directory output uses the input stem or `--output-name`. URL input requires an explicit file or
`--output-dir` with `--output-name`. `--no-resolve` keeps import directives and cannot be combined
with rendering or preview. Use a dedicated output directory when previous results need to be
preserved; these commands can replace existing targets.

The default viewport is 1400×900. Use `--html-viewport-width` / `--html-viewport-height` when the
design needs a different size. For delayed content, use `--html-selector` and/or `--html-wait-ms`;
for explicit interaction and business readiness use a scenario. `--html-scroll-reveal` scrolls
ordinary pages to trigger reveal/lazy loading; it is not an interaction recorder. Header/cookie
options apply only to URL inputs.

## Resources

A render that looks correct on this machine does not prove the PAGX is portable. Inspect the
resulting resource references and test the intended delivery form.

### Fonts

- Browser-loaded fonts influence the snapshot; downloading and embedding are opt-in. Without
  those options, PAGX text still needs the correct font on its render host.
- `import --html-download-fonts` saves the used web fonts and supplies that run's font list as native
  render fallbacks. It does **not** embed them. Checkpoints and batch inputs each use their own
  face list, even when they share a download cache.
- `import --html-embed-fonts` implies downloading and then embeds font glyph data using `pagx embed`.
  This can make the captured text portable, but unreadable fonts, family aliases, subsets and
  fallback selection can still affect typography. Check text and warnings before delivery.
- For an existing PAGX, register correct document font references or supply the needed fallback
  files when embedding, for example:

  ```bash
  pagx embed page.pagx --skip-images --fallback /absolute/path/Font.ttf -o portable.pagx
  ```

  A fallback supplies missing glyphs; it does not rename a CSS family or guarantee the intended
  font was selected. Embedding the wrong substitute preserves the wrong result. `pagx font`
  queries fonts; the retired `pagx font info` / `pagx font embed` commands are not valid entry points.

### Images

`--html-download-images` changes the snapshot stage: save fetched image bytes into
`--html-image-dir` (default `<base>.images`) instead of keeping those images inline.
`--images external|embed` controls storage in the resulting PAGX during native conversion.
Use `--image-base-dir` when relative image references require an explicit source directory.

These native storage modes operate on relative local image references; existing data URIs,
embedded bytes, absolute paths and remote URLs are not all rewritten into a portable bundle.
Use `--images embed` when a single file is desired, then inspect remaining references.
With external resources, deliver the required files with their relative layout.
Locally rasterized effects and downloaded images can also create required sidecar files.

Local HTML is navigated as a `file:` URL, so relative resources resolve against that file. Browser
file-origin/CORS rules and missing files can still prevent loading. A self-contained page is useful
for new designs; existing sites that depend on an HTTP origin should be served through a local URL.
Do not replace working relative assets unnecessarily.

## Batch conversion and static previews

```bash
pagx import --html-batch pages --output-dir out --dry-run
pagx import --html-batch pages --output-dir out --preview
pagx import page.html --output-dir out --preview --preview-columns 3
```

Batch mode recursively selects `.html`/`.htm`, excludes prepared subset files and its nested output
root, and rejects colliding output names before browser setup. It uses separate page contexts while
reusing a browser process. Required `--output-dir` mirrors the input tree. Batch mode cannot be
combined with a positional/`--input` source, `--output-name`, `-o`/`--output`, or `--html-scenario`.
`--skip-existing` reuses a complete artifact set recorded in `<base>.import.json` only when its
source, native binary, options and recorded artifact stamps still match; an existing PAGX alone
is insufficient. `--dry-run` lists planned work without starting the browser or creating outputs.
Both options are batch-only.

`--preview` assembles the current run's successfully rendered PNGs into contact sheets; it does
not replay the source. Ordinary and scenario sheets use `<base>.preview/sheet-NNN.png`; batches
use `<output-dir>/preview/`. Skipped/failed batch inputs are excluded; a fully skipped batch
creates no preview. `--preview-columns` requires
`--preview` and accepts integers 1–32. Preview implies rendering and requires resolution.

Sheets are static overviews, distinct from [live browser preview](../SKILL.md#live-browser-preview).
Scenario output includes a manifest listing the current sheets; see [Interaction capture](interaction.md#export-and-review).
Ordinary/batch import does not provide that scenario manifest: use the current invocation's
reported outputs, not a directory scan that could include old files.

## Manual subset import

For debugging, keep the browser snapshot separate from native conversion:

```bash
node tools/html-snapshot/snapshot.js page.html \
  --viewport-width 1080 --viewport-height 1440 -o page.subset.html
pagx import --format html --html-no-snapshot \
  --input page.subset.html --output page.pagx
pagx render page.pagx -o page.png
```

`--html-no-snapshot` is appropriate only for already prepared local subset HTML. It rejects URLs
and other `--html-*` options, including scenarios, and does not execute scripts. Existing subset
`@keyframes` can still become PAGX animation. Normal `import` resolves inline SVG/import directives
unless `--no-resolve` is specified. Ordinary snapshots may recover flex layout from measured boxes;
current-state snapshots preserve those positions without flex inference.

The standalone `snapshot.js` keeps its own unprefixed options, such as `--viewport-width` and
`--capture-animations`. Use `--html-*` on `pagx import`; do not apply its option names to the
snapshot tool or pass the old snapshot-disable environment switch to the unified import CLI.

## Warm server for repository integrations

For repeated ordinary captures, `node tools/html-snapshot/server.js` exposes a warm snapshot
service (default `http://127.0.0.1:8787`). Its `/snapshot?format=pagx` output still needs
`pagx resolve` before rendering. Consult `tools/html-snapshot/README.md` for HTTP parameters and
scenario support. Use this when an integration benefits from it, rather than as a prerequisite
for command-line conversion.

## Failures and review

Check process status before claiming success. A failed conversion may leave intermediate or older
files. Scenario jobs additionally record `running`, `complete` or `failed` in `<base>.captures.json`;
only `complete` establishes a complete output set. Later interaction failures preserve earlier
captured checkpoints where conversion succeeds. Contact-sheet failure preserves those checkpoint
files but makes the job fail. Inspect the failed step before rerunning source actions.

Capture/import warnings describe omitted or approximated content. Native `import -v` exposes its
normally suppressed conversion warnings. Review their visual consequences; harmless normalization
is different from a missing element or substituted font. `pagx verify` failures are separate and
must be resolved or explicitly reported under the [delivery rules](../SKILL.md#validation-and-delivery).

| Symptom | Next check |
|---|---|
| Runtime or browser missing | Selected executable, Node/runtime overrides and the exact dependency/browser repair command |
| Missing/stale `dist` module | Rebuild the snapshot tool; an existing directory may contain outputs from older source |
| Blank or clipped output | Source visibility, viewport, load readiness and retained subset HTML |
| Wrong font | Captured font face list, native font availability, warnings and actual embedded glyphs |
| Missing interaction state | Scenario locators and explicit readiness; ordinary capture does not perform those actions |
| Animation is static | Explicit `--html-capture-animations`, captured animation attachment and supported channels |
| Layout stayed absolute | Overlap/spacing may prevent flex recovery; current-state capture deliberately disables it |
| Native `pagx preview` is unknown | Use the npm or prepared repository preview launcher, not the standalone native binary |
