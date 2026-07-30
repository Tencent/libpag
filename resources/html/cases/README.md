# HTML importer feature test corpus

Single-feature HTML test cases for the PAGX HTML importer
(`src/pagx/html/importer`) and the `tools/html-snapshot` pipeline.

Each file isolates **one** HTML/CSS feature so a regression can be traced to a
specific construct. Cases are self-contained (no network, no JS, no animation),
have a fixed `<body>` width/height, and use plain colour blocks / placeholder
text as neutral context.

## Two tiers

The importer accepts a documented subset (`spec/html_subset.md`); real front-end
markup routinely goes beyond it. Cases are therefore split into two tiers:

| Tier | Entry point | Expectation | How it is checked |
|------|-------------|-------------|-------------------|
| `subset`   | `HTMLImporter` directly (`PAGX_HTML_SNAPSHOT=0 pagx import`) | Faithful, near-lossless mapping | `pagx import` + `pagx verify`; asserted diagnostics match `expected_warnings` |
| `snapshot` | `snapshot.js` → import → render (real Chromium) | Graceful degradation / visual fidelity | `tools/html-snapshot/eval` SSIM + pixel diff |

`subset` cases must import with **only** the warning codes listed in
`manifest.csv` (empty = zero warnings). `snapshot` cases exercise constructs the
importer drops or downgrades (grid, float, `display:none`, `z-index`, …); they
only render meaningfully after the Chromium snapshot flattens them.

## Layout

```
cases/
  manifest.csv          # id, category, feature, tier, expected_warnings, notes
  coverage.md           # property -> case matrix (kept in sync with the CSS allow-list)
  validate.sh           # subset-tier import + verify + warning assertion harness
  01-structure/   02-box-model/   03-sizing-units/  04-display/   05-flexbox/
  06-grid/        07-position/    08-color/         09-background/ 10-border-radius/
  11-shadow-filter/ 12-opacity-blend/ 13-transform/ 14-overflow-clip/
  15-typography/  16-text-tricks/ 17-image/         18-svg-inline/ 19-css-delivery/
  20-components/
```

Sub-directory numbering mirrors the feature taxonomy in the test plan. The
corpus currently holds **187 cases** across 20 categories (167 `subset` +
20 `snapshot`); `validate.sh` reports **162 subset pass, 5 XFAIL, 0 fail** (the
5 XFAILs are known importer/SVG-resolver `pagx verify` defects, listed in
`coverage.md`).

## Naming

`<feature>__<variant>.html`, e.g. `justify-content__space-between.html`. With
`eval --recursive`, the path becomes the case id
(`05-flexbox/justify-content__space-between.html` →
`05-flexbox__justify-content__space-between`).

## Running

Tier 1 (subset, direct importer — no browser needed):

```bash
PAGX_HTML_SNAPSHOT=0 ./build/pagx import \
  --input resources/html/cases/05-flexbox/gap__single.html \
  --output /tmp/out.pagx -v      # -v prints subset:* warnings
./build/pagx verify --input /tmp/out.pagx
```

Validate every `subset`-tier case at once (import + verify + assert the
`subset:*` warnings match `manifest.csv`):

```bash
resources/html/cases/validate.sh            # exits non-zero on any regression
```

Cases whose `notes` contain `known-verify-diagnostic` are reported as `XFAIL`
(a known importer defect surfaced by `pagx verify`, not a corpus error) and do
not fail the run. Currently the two `text-decoration` cases are XFAIL: the
underline/strike overlay rectangle shares a single `<Group>` with the text
painter, which `pagx verify` flags as "painter leaks geometry from prior
painters".

Tier 2 (snapshot fidelity — needs Chromium):

```bash
cd tools/html-snapshot/eval
./run.sh --corpus ../../resources/html/cases --recursive --label features
open out/features/index.html
```
