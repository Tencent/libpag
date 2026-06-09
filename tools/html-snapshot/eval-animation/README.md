# html-snapshot eval-animation

Quantify how faithfully the **animation** pipeline —
`snapshot capture → pagx import → pagx render --time` — reproduces a page's motion. It is the
moving-picture sibling of [`../eval`](../eval/README.md): that harness diffs a single static frame,
this one diffs a short timeline.

## What it measures

The subset's animation contract is CSS `@keyframes` + the `animation` shorthand (see
`spec/html_subset.md` §13). The snapshot tool normalises every animation source it can reach
(plain CSS, the Web Animations API, GSAP / anime.js) into that canonical form
(`lib/animation-capture.ts`); the importer maps it onto the PAGX animation model
(`src/pagx/html/importer/HTMLAnimationBuilder.cpp`); the runtime plays it back. This harness
exercises that whole chain end to end and reports a per-frame fidelity score.

## How it works

For each original HTML case:

| Step | Tool | Output |
|------|------|--------|
| 1. baseline frames | `baseline-frames.js` | the original page seeked to N playback times and screenshotted — the ground-truth timeline |
| 2. snapshot | `snapshot.js` | `subset.html` with the animation normalised to `@keyframes` + `animation` |
| 3. import | `pagx import` | `subset.pagx` with PAGX `<Animations>` |
| 4. resolve | `pagx resolve` | inline svg / imports resolved |
| 5. render | `pagx render --time <s>` | `render/frame-*.png` at the **same** sample times as step 1 |
| 6. compare | `eval/compare.js` | per-frame SSIM + pixel diff, averaged per case |

The sample grid comes from a single *global* timeline duration — the longest single-iteration
period (`delay + duration`) across the page's animations — so a page mixing a 2s and a 4s loop is
sampled over the full 4s. `baseline-frames.js` writes the chosen times (seconds) to
`baseline/samples.json`; `run.js` passes the identical values to `pagx render --time`, so the
baseline and the PAGX render stay on one clock. Looping animations realign every period, so a
single period is enough.

`pagx render --time <seconds>` was added for this harness: it evaluates the document's timelines at
the given playback time through the runtime scene (`PAGScene` + `PAGTimeline`) and renders that
frame, instead of the static layer-tree render the plain `pagx render` does.

## Usage

```bash
# Full run over a directory of animated HTML files.
./run.sh --corpus ~/anim_cases

# Tag the run for diff-friendly comparison across iterations.
./run.sh --corpus ~/anim_cases --label baseline-v1

# Iterate on a single case (matches by filename substring).
./run.sh --corpus ~/anim_cases --only fade

# Finer timeline sampling (default 5 frames: 0%, 25%, 50%, 75%, 100%).
./run.sh --corpus ~/anim_cases --samples 9

# Reuse already-generated frames (only re-runs metrics + reports).
./run.sh --corpus ~/anim_cases --skip-existing
```

| Option | Description |
|--------|-------------|
| `--corpus <dir>` | Directory of original HTML files (required; or set `$EVAL_CORPUS`) |
| `--out <dir>` | Output directory (default: `eval-animation/out/<label>`) |
| `--pagx-bin <path>` | pagx binary (default: `$PAGX_BIN` or `cmake-build-debug/pagx`) |
| `--samples <n>` | Frames sampled across the timeline (default: 5) |
| `--scale <float>` | `pagx render` scale (default: 1.0) |
| `--no-infer-flex` | Disable the C++ `AbsoluteToFlexInferencePass` during import |
| `--skip-existing` | Reuse existing baseline / render frames if present |
| `--only <substr>` | Only run cases whose relative path contains `<substr>` |
| `--label <name>` | Sub-directory name under `out/` (default: `current`) |
| `--recursive, -r` | Recurse into sub-directories of `--corpus` |
| `--concurrency, -j N` | Process N cases in parallel (default: 1) |
| `--browser-engine N` | Headless driver forwarded to `baseline-frames.js` / `snapshot.js` |

Outputs land in `eval-animation/out/<label>/`:

- `report.csv` — one row per case (means across frames).
- `report.md` — markdown table with a corpus summary on top.
- `index.html` — per-case `baseline / pagx / diff` frame strips; open in a browser.
- `<case>/baseline/*.png`, `render/*.png`, `diff/*.png`, `subset.html`, `subset.pagx`, `*.stderr.txt`.

## Metrics

| Field | Meaning |
|-------|---------|
| `frames` | Number of timeline samples compared. |
| `ssimMean` | Per-frame luma SSIM averaged across the timeline. Higher is better. |
| `pixelDiffMean` | Per-frame `pixelmatch` differing-pixel fraction, averaged. Lower is better. |
| `motion` | `1 − mean SSIM between consecutive baseline frames`. ~0 means the captured page barely moved (a near-static case); higher means more movement. Use it to tell "PAGX matched a still page" from "PAGX matched a moving one". |
| `animCaptured` | Animations the snapshot tool normalised (parsed from its log). |
| `animWarnings` | `subset:animation-*` importer diagnostics. |
| `animDropped` | `subset:animation-unknown-keyframes` (animation referenced a missing `@keyframes`; dropped). |

The acceptance criterion mirrors `../eval`: "corpus-level mean improves" across iterations rather
than a per-case gate. Lean on the summary rows, and on `ssimMovingMean` (SSIM restricted to cases
that actually animate) so still cases don't inflate the score.

## Caveats

- The runtime plays only the channels the subset supports — `opacity`, `transform: translate`,
  `color`, `background-color`. A page animating scale / rotation / non-translate transforms will
  diff against a baseline that *does* show them, so its SSIM is bounded below 1.0 by design (same
  philosophy as the static eval, which strips JS and subpixel detail).
- Animation seeking uses `document.getAnimations()`, which surfaces CSS + WAAPI + Motion One.
  Purely rAF-driven libraries (some GSAP / anime.js setups) are captured by the snapshot tool's
  sampler but are **not** deterministically seekable in the baseline, so those cases fall back to a
  best-effort timeline and may diff poorly.
