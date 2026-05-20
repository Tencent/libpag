# html-snapshot eval

Quantify the fidelity of the `snapshot.js → pagx import → pagx render`
pipeline against the source HTML corpus. Used to drive iteration on the
snapshot's layout-preserving rewrites.

## Layout

| File | Role |
|------|------|
| `baseline.js` | Render an original HTML in headless Chromium, save a PNG cropped to the body's measured rect — this is the ground truth. |
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
```

Outputs land in `eval/out/<label>/`:

- `report.csv` — machine-readable per-case row.
- `report.md` — markdown table with corpus-level means at the top.
- `index.html` — side-by-side `baseline / subset / diff` viewer; open in a browser.
- `<case>/baseline.png`, `subset.png`, `diff.png`, `subset.html`, `subset.pagx`,
  `import.stderr.txt`, `snapshot.stderr.txt`, …

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

The acceptance criterion is "corpus-level mean improves" (no per-case
regression gate), so use the markdown summary rows at the top of `report.md`
for direction.

## Why a baseline at all

The snapshot pipeline strips JS, animations, and any subpixel detail Chromium
paints differently from PAG. A naive perfect-fidelity comparison is therefore
unattainable; what matters is the *delta* across iterations of `snapshot.js`,
which is exactly what the labelled `out/<label>/` directories let us track.
