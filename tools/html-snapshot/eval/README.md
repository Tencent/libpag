# html-snapshot eval

Quantify the fidelity of the `snapshot.js → pagx import → pagx render`
pipeline against the source HTML corpus. Used to drive iteration on the
snapshot's layout-preserving rewrites.

The eval harness supports Puppeteer 23.0.0 or newer only. This matches the
tested minimum declared by `tools/html-snapshot/package.json`; ad-hoc installs
must stay at or above that floor because baseline capture passes the
`captureBeyondViewport` screenshot option.

## Layout

| File | Role |
|------|------|
| `baseline.js` | Render an original HTML in headless Chromium, save a PNG cropped to the body's measured rect — this is the ground truth. |

> Baseline and snapshot share `lib/page-loader.ts`, which walks the viewport
> top-to-bottom before capturing so lazy-loaded content (IntersectionObserver /
> scroll listeners / native `loading="lazy"`, and infinite-scroll feeds) is
> realised instead of captured as skeleton placeholders. The pass is bounded
> (pass count / height / time) so infinite feeds settle at a sane length. Set
> `HTML_SNAPSHOT_NO_AUTOSCROLL=1` to disable it.
| `compare.js`  | Pixel metrics (pixelmatch + mean RGB delta + luma SSIM), Chromium-based flex container counter (same method for live and subset), importer-warning parser. Importable as a module or runnable per-case. |
| `run.js`      | Per-case driver: baseline → snapshot → import → resolve → render → compare. Emits `report.csv`, `report.md`, and `index.html`. |
| `run.sh`      | Wrapper around `run.js`. |

## Usage

```bash
# Full run with the default corpus (~/Desktop/tmp_pagx) and label=current.
./run.sh

# Tag the run for diff-friendly comparison across iterations.
./run.sh --label baseline-v1

# Only iterate on a subset of cases (matches by filename substring).
./run.sh --only meitu

# Reuse already-generated PNGs (only re-runs metrics).
./run.sh --skip-existing --label current

# Download the page's web fonts, embed them into the .pagx and pass them to
# the renderer as fallbacks (mirrors pagx import; useful for CJK corpora).
./run.sh --download-fonts --label fonts-on

# Download the page's external images and reference them by file path instead
# of inlining base64 (mirrors pagx import; keeps subset HTML / .pagx small).
./run.sh --download-images --label images-on
```

With `--download-fonts`, `snapshot.js` saves each web font the page uses into a
single shared, content-addressed cache at `out/<label>/fonts/` — a face common
to the corpus is stored once instead of being re-downloaded per case — and each
case records the subset it uses in `out/<label>/<case>/fonts.txt`. `run.js` then
runs `pagx embed --fallback …` on the resolved document and passes the same
per-case files to `pagx render --fallback`, so text in an uninstalled web font
renders with the correct face instead of a host system fallback. Cases with no
web fonts are unaffected.

With `--download-images`, `snapshot.js` saves each external image the page uses
into a single shared, content-addressed cache at `out/<label>/images/` (an image
common to the corpus is stored once) and rewrites the subset HTML to reference
it by its absolute file path instead of inlining it as a base64 `data:` URI.
This keeps the subset HTML — and the `.pagx` produced from it — small for
image-heavy corpora. Unlike fonts, the path is baked into the `.pagx` by
`pagx import`, so `pagx render` reads the files directly with no per-case
manifest or `--fallback` wiring. Cases with no external images are unaffected.

Outputs land in `eval/out/<label>/`:

- `report.csv` — machine-readable per-case row.
- `report.md` — markdown table with corpus-level means at the top.
- `index.html` — side-by-side `baseline / subset / diff` viewer; open in a browser.
- `<case>/baseline.png`, `subset.png`, `diff.png`, `subset.html`, `subset.pagx`,
  `import.stderr.txt`, `snapshot.stderr.txt`, …
- `<case>/hierarchy.json` — Layer count/depth, Group-inclusive structure depth, masks,
  and depth histogram. CSV and Markdown reports include the three primary hierarchy metrics.

## Metrics

| Field | Meaning |
|-------|---------|
| `ssim` | Single-window luma SSIM. Higher is better. Robust to small offsets. |
| `pixelDiffRatio` | Fraction of differing pixels per `pixelmatch` (threshold 0.1, AA off). Lower is better. |
| `meanRgbDelta` | Mean per-channel absolute difference (0..255). Lower is better. |
| `flexLive` | Number of `display: flex \| inline-flex` elements in the live original DOM (Chromium-rendered). |
| `flexSubset` | Same count, but on the rendered subset DOM. Both counts come from `getComputedStyle` over `document.body.querySelectorAll('*')`, so they are directly comparable. |
| `flexDelta` | `flexSubset - flexLive`. Signed; positive means the subset emits more flex containers than the live DOM. |
| `flexRetention` | `min(flexSubset, flexLive) / flexLive` — capped retention. 1.0 means the subset preserved at least every flex container in the live DOM. |
| `flexInflation` | `flexSubset / flexLive` — raw ratio. Values above ~1.0 indicate the snapshot invented extra flex wrappers (regression signal). |
| `importerWarnings` | Total `pagx import` warnings parsed from stderr. |
| `flexInferred` | `subset:flex-inferred` warnings — C++ `AbsoluteToFlexInferencePass` recovered a flex layout from absolute children. |
| `flexSkipped` | `subset:flex-inference-skipped` warnings — the C++ pass gave up and left the parent absolute. |

Use corpus averages as trend indicators. Gate hierarchy changes on per-case
geometry, paint-order and animation checks as well as the pixel metrics.

## Why a baseline at all

For hierarchy changes, also run `node eval/hierarchy.js <file.pagx|directory>` from
`tools/html-snapshot`. It reports scene Layer counts, maximum Layer depth, depth
including Group/TextBox containers, mask counts and the depth histogram. Resource
definitions are excluded. Compare each case's pixels and hierarchy separately;
a smaller corpus average must not hide a paint-order or animation regression.

The snapshot captures computed layout and supported animation channels. Browser
and native text rasterization, unsupported CSS and subpixel rounding can still
differ. Keep both a browser reference and a previous native render so a change
can be distinguished from an existing rendering limitation.

## Hierarchy lowering validation

The five `layertest` inputs were evaluated from baseline commit `86898af1a`
through `e2be15541`. HTML depth counts `body` as 1 and treats each inline SVG
as one HTML element. PAGX metrics use the resolved scene, excluding resource
definitions. Structure depth includes Layer, Group and TextBox containers.

| Case | Source HTML depth | Layer depth before → after | Structure depth before → after | Layers before → after |
| --- | --- | --- | --- | --- |
| 23-六月实习报告 | 12 | 14 → 8 | 15 → 9 | 963 → 821 |
| 27-Boltshift销售看板 | 11 | 14 → 10 | 15 → 11 | 640 → 513 |
| 28-真牛逼便利店首页 | 12 | 18 → 13 | 19 → 14 | 329 → 278 |
| 48-文字动效 | 5 | 7 → 5 | 7 → 5 | 71 → 55 |
| 63-看板六视图 | 9 | 11 → 10 | 12 → 11 | 172 → 144 |

Contiguous source decoration/content now share a Layer; padding and absolute-child
reference frames no longer require layout hosts. Safe coordinate ancestors and
clip reset boxes are composed away. Clipping, background blending/tile antialiasing
and animation pivots still retain scopes when their semantics require them. No
private paint-order attributes remain in exported custom data.

Validation completed:

- 98 Jest suites / 1418 tests passed; 1 suite / 12 tests skipped by their existing
  conditions. The current-state subset passed all 40 tests.
- 990 native HTML, optimizer, layout and constraint tests passed, including resize,
  serialization, transform interpolation and independent text/background targets.
  `PAG_SKIP_BASELINE_CHECK=ON` was used; image comparisons ran separately below.
- Browser comparisons: paint order 40/40; background layout and shadows 26/26;
  object layout and text fragments 70/71. The existing elliptical-radius image
  case fails its mean-error threshold (1.3588 versus 0.5). Rebuilding the original
  importer produced byte-identical PAGX for that fixture, confirming it predates
  this change.
- Reimporting the original captured subsets isolates native lowering: cases 23,
  48 and 63 retain identical pixels; cases 27 and 28 change 1224 and 1058 pixels
  respectively where fractional box sizes previously passed through a rounded
  100% wrapper before flex placement.
- Fresh full-source exports reach the depths above. Browser SSIM is respectively
  0.9249, 0.8888, 0.9610, 0.7167 and 0.9540; this is not a claim of exact browser
  fidelity. Final semantic animation binding changes none of their pixels or
  hierarchy compared with the preceding stage.

Local review artifacts are in `out/layertest-hierarchy/`: `index.html`, `report.md`,
per-case HTML/PAGX/PNGs and `hierarchy.json`, plus `animation-binding-audit.json`.
The original `out/layertest/` is preserved. These generated artifacts remain ignored
by Git. Reproduce with `run.js` using the same five-source corpus and
`--pagx-images embed`; run `hierarchy.js` on both output directories to compare structure.
