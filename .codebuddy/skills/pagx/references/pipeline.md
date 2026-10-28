# Conversion pipeline reference

Use this reference for HTML conversion setup, output sets, resource handling and failures.
[CLI](cli.md) owns the command option tables; [Interaction capture](interaction.md) owns the
scenario format. Paths in repository examples are relative to the libpag root.

## Choose an entry point

`pagx import --input page.html --output page.pagx` snapshots an HTML file or URL, imports the
subset and resolves inline SVG/import directives by default. It writes one PAGX, without the
capture pipeline's PNG or web-font download/embedding stage.

`pagx capture page.html -o out` runs snapshot, import, resolve and render, keeping the subset for
debugging. It supports resource options, custom viewports, batches and scenarios. Its `-o` means
an output **directory**. Both entry points capture a static frame unless `--capture-animations`
is supplied. For stateful UI use [Interaction capture](interaction.md), which produces static
checkpoints and cannot be combined with animation capture.

The repository's `node tools/html-snapshot/html2pagx ...` is another entry to the same capture
pipeline. Use it when explicitly requested or when that repository tool is the available entry;
pass `--pagx-bin /absolute/path/to/pagx` to select the native build. `pagx capture` defaults its
internal import/resolve/render commands to the invoking native executable, ignoring an unrelated
`PAGX_BIN` or `pagx` on PATH; only an explicit `--pagx-bin` overrides it.

## Setup

Select the executable before checking the needed commands with `--help`. Missing command support
requires a suitable build/package, not guessed flags. See [Environment selection](../SKILL.md#environment-and-command-selection).

**Complete npm installation.** The package bundles the snapshot/capture runtime. Its launcher
prepares the browser driver and browser on first use in a per-user cache. Override that cache
with `PAGX_HTML_SNAPSHOT_CACHE`; `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1` disables automatic installation
and makes missing dependencies an actionable error. Help, invalid capture arguments and batch
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
checks the actual executable selected by the browser driver. Run it again after source/dependency
changes or setup failures, rather than before every conversion.

The default browser driver is Puppeteer. Select Playwright with `--browser-engine playwright` on
`capture`, or `HTML_SNAPSHOT_BROWSER=playwright` in the environment. It must have its own driver
and browser installed. `PUPPETEER_EXECUTABLE_PATH` / `PLAYWRIGHT_EXECUTABLE_PATH` override the
corresponding executable; a populated cache directory alone does not establish that it is usable.

**Runtime lookup.** `PAGX_NODE_BIN` selects Node. `PAGX_HTML_SNAPSHOT_BIN` and
`PAGX_HTML_CAPTURE_BIN` override the single-snapshot and full-capture launchers respectively. The
native bridge checks an explicit override first, then the bundled runtime near the executable,
then the repository runtime found from the working directory or executable location. An invalid
explicit override is an error, not a request to search for a different runtime.

## Ordinary files and URLs

```bash
# One file and a separate render.
pagx import --input page.html --output page.pagx
pagx render page.pagx -o page.png

# Output set with a viewport matching the design.
pagx capture page.html -o out --viewport-width 1080 --viewport-height 1440

# URL input needs a stable output name for capture.
pagx capture https://example.com/promo --output-name promo -o out

# Capture playable motion, including resource embedding when required.
pagx capture motion.html -o out --capture-animations --embed-fonts --pagx-images embed
```

Ordinary capture produces `<base>.subset.html`, `<base>.pagx` and `<base>.png`. It does not create
a scenario manifest. The default output directory is the input file's directory (the current
directory for a URL); `--output-name` overrides the input stem and is required for URL capture.
`--no-render` skips the PNG, `--no-resolve` also disables rendering, and `--no-subset-html` removes
the retained subset. Use a dedicated output directory when previous results need to be preserved;
these commands can replace existing targets.

The default viewport is 1400×900. Use capture's viewport options when the design needs a different
size. For delayed content, use `--selector` and/or `--wait-ms`; for explicit interaction and business
readiness use a scenario. `--scroll-reveal` scrolls ordinary pages to trigger reveal/lazy loading;
it is not an interaction recorder. Header/cookie options apply only to URL inputs.

## Resources

A render that looks correct on this machine does not prove the PAGX is portable. Inspect the
resulting resource references and test the intended delivery form.

### Fonts

- Browser-loaded fonts influence the snapshot, but ordinary `import` does not download/embed them
  for native rendering. PAGX text still needs the correct font on its render host.
- `capture --download-fonts` saves the used web fonts and supplies that run's font list as native
  render fallbacks. It does **not** embed them. Checkpoints and batch inputs each use their own
  face list, even when they share a download cache.
- `capture --embed-fonts` implies downloading and then embeds font glyph data using `pagx embed`.
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

`--download-images` changes the snapshot stage: save fetched image bytes into `--image-dir`
(default `<base>.images`) instead of keeping those images inline. `--pagx-images external|embed`
controls image storage during capture's native resolve stage. For `import`/`resolve`, the option
is named `--images`, not `--pagx-images`.

These native storage modes operate on relative local image references; existing data URIs,
embedded bytes, absolute paths and remote URLs are not all rewritten into a portable bundle.
Use `--pagx-images embed` for capture when a single file is desired, then inspect remaining
references. With external resources, deliver the required files with their relative layout.
Locally rasterized effects and downloaded images can also create required sidecar files.

Local HTML is navigated as a `file:` URL, so relative resources resolve against that file. Browser
file-origin/CORS rules and missing files can still prevent loading. A self-contained page is useful
for new designs; existing sites that depend on an HTTP origin should be served through a local URL.
Do not replace working relative assets unnecessarily.

## Batch conversion and static previews

```bash
pagx capture --batch pages --output-root out --dry-run
pagx capture --batch pages --output-root out --preview
pagx capture page.html -o out --preview --preview-columns 3
```

Batch mode recursively selects `.html`/`.htm`, excludes prepared subset files and its nested output
root, and rejects colliding output names before browser setup. It uses separate page contexts while
reusing a browser process. `--output-root` mirrors the input tree; without it, outputs are written
beside each input. Batch mode cannot be combined with a positional input, `--output-name`,
`-o`/`--output-dir`, or `--scenario`. `--skip-existing` skips existing PAGX targets; `--dry-run`
lists the planned inputs without starting the browser or creating output directories. Both are
batch-only options.

`--preview` assembles the current run's successfully rendered PNGs into contact sheets; it does
not replay the source. Ordinary and scenario sheets use `<base>.preview/sheet-NNN.png`; batches
use `<output-root>/preview/`, or `<input-dir>/preview/` without an output root. Skipped/failed batch
inputs are excluded; a fully skipped batch creates no preview. `--preview-columns` requires
`--preview` and accepts integers 1–32. Preview requires both resolve and render.

Sheets are static overviews, distinct from [live browser preview](../SKILL.md#live-browser-preview).
Scenario output includes a manifest listing the current sheets; see [Interaction capture](interaction.md#export-and-review).
Ordinary/batch capture does not provide that scenario manifest: use the current invocation's
reported outputs, not a directory scan that could include old files.

## Manual subset import

For debugging, keep the browser snapshot separate from native conversion:

```bash
node tools/html-snapshot/snapshot.js page.html \
  --viewport-width 1080 --viewport-height 1440 -o page.subset.html
PAGX_HTML_SNAPSHOT=0 pagx import --format html \
  --input page.subset.html --output page.pagx
pagx render page.pagx -o page.png
```

The environment assignment above uses a POSIX shell. Set `PAGX_HTML_SNAPSHOT` in the invoking
process's environment on other platforms, and restore it afterward. Disabling the browser pass
is appropriate only for already prepared subset HTML; it rejects URL/scenario input and does not
execute scripts. Normal `import` resolves inline SVG/import directives unless `--no-resolve` is
specified. Ordinary snapshots may recover flex layout from measured boxes; current-state snapshots
preserve those positions without flex inference.

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
| Animation is static | Explicit `--capture-animations`, captured animation attachment and supported channels |
| Layout stayed absolute | Overlap/spacing may prevent flex recovery; current-state capture deliberately disables it |
| Native `pagx preview` is unknown | Use the npm or prepared repository preview launcher, not the standalone native binary |
