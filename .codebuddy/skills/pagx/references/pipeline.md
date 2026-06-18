# Conversion pipeline reference

Full details for converting HTML to PAGX. The one-shot tool is
`tools/html-snapshot/html2pagx`; everything below explains its options, setup, fonts/images, URL
inputs, a warm server for fast iteration, a manual debugging path, and troubleshooting. Paths are
relative to the repository root.

## Table of contents

- [What the converter does](#what-the-converter-does)
- [Setup details](#setup-details)
- [html2pagx options](#html2pagx-options)
- [Fonts](#fonts)
- [Images](#images)
- [URL inputs](#url-inputs)
- [Fast iteration with the warm server](#fast-iteration-with-the-warm-server)
- [Manual step-by-step pipeline](#manual-step-by-step-pipeline)
- [Direct subset import without a browser](#direct-subset-import-without-a-browser)
- [Conversion warnings](#conversion-warnings)
- [Troubleshooting](#troubleshooting)

---

## What the converter does

`html2pagx` wraps four steps into one command:

1. **snapshot** — render `<input>.html` in headless Chromium and emit `<input>.subset.html`, a
   flat, absolute-positioned page that conforms to `spec/html_subset.md` (computed styles baked in,
   Tailwind/utility classes dropped, icon fonts and `<canvas>` inlined).
2. **import** — `pagx import --format html` turns the subset HTML into `<input>.pagx`
   (with `--html-infer-flex` to recover flex layout from the absolute boxes).
3. **resolve** — expand inline `<svg>`/import directives into native PAGX nodes.
4. **render** — write `<input>.png`, a preview at `--scale` (default 1).

The intermediate `<input>.subset.html` is kept by default — it is useful for debugging what the
browser actually captured.

## Setup details

**Installed via npm (no repo).** `npm install -g @libpag/pagx` ships the snapshot tool bundled
inside the package, so `tools/html-snapshot/` is not needed. The wrapper points the native binary's
`--html-snapshot` bridge (`PAGX_HTML_SNAPSHOT_BIN`) at the bundled launcher automatically. Puppeteer
and its Chromium build are **not** installed by `npm install`; the launcher installs them on the
first snapshot run into a per-user cache (`~/.cache/libpag-pagx/html-snapshot`, or `%LOCALAPPDATA%`
on Windows; override with `PAGX_HTML_SNAPSHOT_CACHE`). Set `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL=1` to
disable the auto-install and have the launcher print the manual command instead. The one-shot
`html2pagx` wrapper and the warm HTTP server below require the repo (Case B).

**Inside the repo.** `scripts/setup.sh` automates this; run it once. Manually, the requirements are:

- **node** on `PATH`.
- **pagx** CLI: `npm install -g @libpag/pagx` (or build the repo's `cmake-build-debug/pagx` and
  pass `--pagx-bin`). Minimum version that supports HTML import is recent; `pagx -v` to check.
- **Snapshot tool deps + build**:
  ```bash
  cd tools/html-snapshot
  npm install          # pulls puppeteer + a Chromium build (~150 MB first time)
  npm run build        # compiles TypeScript to dist/ (html2pagx requires dist/)
  ```
- If Chromium is missing from the cache (e.g. npm ran in a sandbox), install it explicitly:
  ```bash
  PUPPETEER_CACHE_DIR="$HOME/.cache/puppeteer" \
    npx --prefix tools/html-snapshot puppeteer browsers install chrome
  ```

`html2pagx` runs its own preflight checks and prints the exact remediation command when a
dependency is missing.

## html2pagx options

```bash
tools/html-snapshot/html2pagx <input.html | http(s)://url> [options]
```

| Flag | Use |
|------|-----|
| `-o, --output-dir <dir>` | Write outputs to a different directory |
| `--output-name <name>` | Base name for outputs (**required** for URL inputs) |
| `--scale <N>` | Preview PNG scale (default 1; use 2 for a crisp review image) |
| `--viewport-width <px>` / `--viewport-height <px>` | Headless viewport; **match the `body` size** to avoid clipping (default 1400×900) |
| `--wait-ms <ms>` | Extra settle delay after load (default 800); raise for late-rendering charts/animations |
| `--selector <css>` | Wait for this selector before snapshotting |
| `--embed-fonts` | Download the page's web fonts and embed them into the `.pagx` (self-contained, correct text). Implies `--download-fonts` |
| `--download-fonts` | Register downloaded fonts as render fallbacks without embedding them |
| `--download-images` | Save external images to disk and reference them by path instead of inlining base64 (keeps the `.pagx` small) |
| `--no-render` | Stop after resolve (no PNG) |
| `--no-resolve` | Stop after import (no resolve/render) |
| `--no-infer-flex` | Disable flex inference (keep pure absolute layout) |
| `--no-subset-html` | Do not keep `<input>.subset.html` |
| `--pagx-bin <path>` | Path to the `pagx` binary (default `$PAGX_BIN` or `cmake-build-debug/pagx`) |
| `--browser-engine <name>` | `puppeteer` (default) or `playwright` |
| `--cookie <name=value>` / `--header <Key: Value>` | URL inputs only; repeatable |
| `--batch <dir>` | Convert every `.html` under a directory with one shared browser |

## Fonts

The snapshot bakes each text node's font-family *name* but cannot embed the font itself, and PAGX
resolves fonts by name against fonts installed on the rendering host. A page styled with an
uninstalled web font therefore renders with a wrong fallback.

- **`--embed-fonts`** (recommended for AI-generated designs): captures the exact font files Chromium
  fetched and embeds them into the `.pagx`, so glyphs and metrics match the design anywhere.
- **`--download-fonts`**: registers the fonts as render fallbacks but does not embed them.
- **Caveat (CJK)**: Google's Noto Sans SC/TC/JP are served as per-subset files whose weight metadata
  is often unreliable; glyphs render correctly but the *weight* may not match. For weight-critical
  CJK, install the real font on the render host instead.

## Images

By default external images are inlined as base64 data URIs, making the `.pagx` self-contained but
large for image-heavy pages. Use `--download-images` to write them to a sidecar directory and
reference them by path; the images are read from disk at render time. Use `--image-dir <dir>` to
choose the location.

## URL inputs

A public URL can be converted directly — the browser navigates to it and resolves its own
resources:

```bash
tools/html-snapshot/html2pagx https://example.com/promo \
  --output-name promo --output-dir /tmp/out --embed-fonts
# → /tmp/out/promo.subset.html / promo.pagx / promo.png
```

`--output-name` is required for URLs (there is no filename to derive). Pass `--cookie` / `--header`
for pages behind auth or feature flags.

## Fast iteration with the warm server

When iterating on a design (edit HTML, re-convert, repeat), the per-run Chromium startup dominates.
Run the snapshot pipeline as an HTTP server that keeps one browser warm:

```bash
cd tools/html-snapshot
node server.js            # http://127.0.0.1:8787
```

Then convert via HTTP (the server can also run `pagx import` itself):

```bash
# HTML in, PAGX out:
curl -s --data-binary @page.html -H 'Content-Type: text/html' \
  'http://127.0.0.1:8787/snapshot?format=pagx' > page.pagx

# Fetch a live URL and get PAGX back:
curl -sG --data-urlencode 'url=https://example.com/' \
  --data-urlencode 'format=pagx' \
  'http://127.0.0.1:8787/snapshot' > page.pagx
```

The server needs the `pagx` binary for `format=pagx`/`both`; point it with `--pagx-bin` if it is not
at the default location. After getting the `.pagx`, run `pagx resolve` + `pagx render` (or
`pagx verify`) to preview.

## Manual step-by-step pipeline

For debugging a single step, run them individually:

```bash
node tools/html-snapshot/snapshot.js page.html            # → page.subset.html
pagx import --format html --html-infer-flex \
  --input page.subset.html --output page.pagx             # → page.pagx
pagx resolve page.pagx                                     # expand svg/import
pagx render page.pagx -o page.png --scale 2                # preview
```

Inspecting `page.subset.html` shows exactly what the browser captured — the fastest way to see why
an element is missing or mispositioned. `pagx verify page.pagx` runs diagnostics + layout + render
in one call.

## Direct subset import without a browser

If the HTML is *already* simple and subset-compliant (static markup, inline styles, no JS, only the
properties in `spec/html_subset.md`), skip the browser entirely:

```bash
pagx import --format html --input page.html --output page.pagx
pagx verify page.pagx
```

This is lighter (no Chromium) but unforgiving — anything outside the subset is dropped or warned.
For AI-generated designs that use modern CSS, the browser path (`html2pagx`) is more reliable.
`pagx import --input page.html --html-snapshot --html-infer-flex` is a middle ground: `pagx` invokes
`snapshot.js` itself, but still needs the snapshot tool installed and produces no preview PNG.

## Conversion warnings

`pagx import` prints `subset:<category>` warnings for anything it skipped or downgraded. Common,
usually-harmless ones:

- `subset:unit-coerced` — `em`/`rem`/`pt`/`vw`/`vh` converted to pixels. Fine.
- `subset:flex-shorthand-collapsed` — `flex: 1 1 0` reduced to `flex: 1`. Fine.
- `subset:unsupported-property` — a CSS property has no PAGX equivalent and was dropped (e.g.
  `min-width`, `aspect-ratio`, `background-size`, `text-overflow`). If the visual is unaffected in
  the preview, ignore; otherwise express the intent differently (fixed sizes, etc.).
- `subset:unsupported-tag` — an element was removed (interactive widget, `<iframe>`, `<video>`,
  etc.). Replace it with a styled `<div>` or a poster `<img>` that looks the same.
- `subset:flex-inference-skipped` — a container's children overlapped or had inconsistent spacing,
  so flex was not inferred (kept absolute). Usually fine; tidy the source layout if alignment looks
  off.

`--html-strict` turns warnings into hard errors — useful for CI, not for normal use.

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| First `pagx import --html-snapshot` is slow / downloads ~150 MB | Expected one-time install of the bundled headless browser into the per-user cache. Subsequent runs are fast. To pre-install or relocate it, see §Setup details (`PAGX_HTML_SNAPSHOT_CACHE`, `PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL`). |
| `pagx html-snapshot: error: failed to install puppeteer` (npm path) | The launcher could not auto-install the browser (no network / no `npm` on PATH). Run the exact command it printed, then retry; or use a machine with the repo (Case B). |
| `pagx binary not found or not executable` | Install `pagx` (`npm install -g @libpag/pagx`) or pass `--pagx-bin <path>`. |
| `'puppeteer' is not installed` / `bundled Chrome is missing` | Run `npm install` in `tools/html-snapshot`, then the `puppeteer browsers install chrome` command the tool prints. |
| `cannot find module './dist/...'` | The tool was not built. Run `npm run build` in `tools/html-snapshot`. |
| Blank / mostly-empty PNG | Content was hidden, off-canvas, or rendered after the snapshot. Match `--viewport-width/height` to the `body` size and raise `--wait-ms`; check `<input>.subset.html` for what was captured. |
| Text in the wrong font | The web font was not embedded. Keep the Google Fonts `<link>` and convert with `--embed-fonts`. |
| An element is missing | It was hidden (`display:none`/`opacity:0`), an unsupported/interactive widget, or a tainted/WebGL `<canvas>`. Include only visible content; use a styled `<div>` for controls and same-origin/CORS image sources. |
| Chart is missing or empty | Canvas captured before it drew (raise `--wait-ms`/`--selector`) or it is tainted/WebGL (use same-origin/CORS sources). |
| Relative `<img>`/CSS not loading (local file) | Local relative resources do not resolve. Inline as data URIs or use absolute URLs; or convert from a URL instead. |
| Layout looks absolute, not flex | Add `--html-infer-flex` (on by default in `html2pagx`); some layouts are intentionally skipped — see `subset:flex-inference-skipped`. |
