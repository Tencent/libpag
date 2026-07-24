#!/usr/bin/env bash
#
# run_html_eval.sh — HTML browser-fidelity eval driver.
#
# Runs the full snapshot -> pagx import -> pagx render -> compare pipeline
# (tools/html-snapshot/eval) over every HTML corpus committed under
# resources/html/, and emits a per-corpus report. This is the entry point
# behind the CMake `HTMLTest` target.
#
# Per case it is report-only: it never fails on a single low SSIM. Browser
# fidelity is a corpus-level mean metric, so the only pass/fail gate is on the
# per-corpus means (SSIM / pixel-diff / RGB-delta) compared against the
# committed baseline resources/html/baseline.json. A corpus with no baseline
# entry stays report-only. Inspect report.md / index.html by hand for detail.
#
# Prerequisites:
#   - a built `pagx` binary (default: <repo>/cmake-build-debug/pagx, or $PAGX_BIN)
#   - Node.js. The script installs html-snapshot dependencies on every run and,
#     when Playwright is selected, also installs its Chromium binary.
#   - network access for `corpus/websites` and `corpus/generated` (they pull
#     CDN CSS / web fonts / external images at eval time)
#
# Usage:
#   test/run_html_eval.sh                 # all corpora
#   test/run_html_eval.sh cases cli       # only the named corpora
#   PAGX_BIN=/path/to/pagx test/run_html_eval.sh
#   EVAL_EXTRA_ARGS="--only bilibili" test/run_html_eval.sh websites
#   CONCURRENCY=8 BROWSER_ENGINE=puppeteer test/run_html_eval.sh
#   HTML_EVAL_UPDATE_BASELINE=1 test/run_html_eval.sh   # re-seed the baseline
#   HTML_BASELINE=/path/to/baseline.json test/run_html_eval.sh
#
set -u
set -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SNAPSHOT_DIR="$ROOT/tools/html-snapshot"
EVAL="$SNAPSHOT_DIR/eval/run.sh"
PAGX_BIN="${PAGX_BIN:-$ROOT/cmake-build-debug/pagx}"
EVAL_EXTRA_ARGS="${EVAL_EXTRA_ARGS:-}"
# Cases run in parallel; a headless engine handles the browser work.
CONCURRENCY="${CONCURRENCY:-16}"
BROWSER_ENGINE="${BROWSER_ENGINE:-playwright}"
# Corpus-level mean baseline gate. HTML_BASELINE points at the committed
# baseline; set HTML_EVAL_UPDATE_BASELINE=1 to re-seed it from this run instead
# of gating against it.
HTML_BASELINE="${HTML_BASELINE:-$ROOT/resources/html/baseline.json}"
HTML_EVAL_UPDATE_BASELINE="${HTML_EVAL_UPDATE_BASELINE:-0}"

# corpus name  ->  relative dir under resources/html
corpus_dir() {
  case "$1" in
    cases)     echo "cases" ;;
    cli)       echo "corpus/cli" ;;
    websites)  echo "corpus/websites" ;;
    generated) echo "corpus/generated" ;;
    *)         echo "" ;;
  esac
}

# Single source of truth for the corpus set; the error hint derives from it.
DEFAULT_CORPORA=(cases cli websites generated)
ALL_CORPORA="${DEFAULT_CORPORA[*]}"
if [ "$#" -gt 0 ]; then
  CORPORA=("$@")
else
  CORPORA=("${DEFAULT_CORPORA[@]}")
fi

# --- prerequisites ---------------------------------------------------------

if [ ! -x "$PAGX_BIN" ]; then
  echo "run_html_eval: pagx binary not found or not executable: $PAGX_BIN" >&2
  echo "run_html_eval: build it first, e.g. cmake --build cmake-build-debug --target pagx" >&2
  exit 1
fi
# run.js reads $PAGX_BIN, so export it instead of passing --pagx-bin (keeps the
# per-corpus command aligned with the bare eval/run.sh invocation).
export PAGX_BIN

if ! command -v node >/dev/null 2>&1; then
  echo "run_html_eval: node is required but was not found on PATH" >&2
  exit 1
fi

# Reject bad corpus names/directories before doing the comparatively expensive
# npm/browser bootstrap. A typo must not turn into a successful no-op run.
declare -a CORPUS_DIRS=()
for name in "${CORPORA[@]}"; do
  rel="$(corpus_dir "$name")"
  if [ -z "$rel" ]; then
    echo "run_html_eval: unknown corpus '$name' (valid: $ALL_CORPORA)" >&2
    exit 2
  fi
  dir="$ROOT/resources/html/$rel"
  if [ ! -d "$dir" ]; then
    echo "run_html_eval: corpus dir missing: $dir" >&2
    exit 1
  fi
  CORPUS_DIRS+=("$dir")
done

# PNG fixtures are tracked by Git LFS. If a clone has not materialized them,
# Chromium would render broken images and still produce superficially valid
# fidelity reports. Detect pointer files before spending time on npm/eval.
MISSING_LFS=0
for corpus_root in "${CORPUS_DIRS[@]}"; do
  while IFS= read -r -d '' asset; do
    if dd if="$asset" bs=128 count=1 2>/dev/null \
        | grep -q '^version https://git-lfs.github.com/spec/v1'; then
      echo "run_html_eval: Git LFS asset is not materialized: $asset" >&2
      MISSING_LFS=1
    fi
  done < <(find "$corpus_root" -type f -name '*.png' -print0)
done
if [ "$MISSING_LFS" -ne 0 ]; then
  echo "run_html_eval: run 'git lfs pull' before HTML evaluation" >&2
  exit 1
fi

# Always refresh the html-snapshot toolchain (deps + compiled dist/) before a run.
echo "run_html_eval: installing html-snapshot dependencies (npm install)..."
(cd "$SNAPSHOT_DIR" && npm install) || { echo "run_html_eval: npm install failed" >&2; exit 1; }

# Installing the playwright npm package does not guarantee that its matching
# Chromium binary exists on a fresh machine. Keep the default Playwright path
# self-contained; --no-install prevents npx from fetching an undeclared package.
if [ "$BROWSER_ENGINE" = "playwright" ]; then
  echo "run_html_eval: installing Playwright Chromium..."
  (cd "$SNAPSHOT_DIR" && npx --no-install playwright install chromium) || {
    echo "run_html_eval: Playwright Chromium installation failed" >&2
    exit 1
  }
fi

echo "run_html_eval: building html-snapshot (npm run build)..."
(cd "$SNAPSHOT_DIR" && npm run build) || { echo "run_html_eval: npm run build failed" >&2; exit 1; }

# --- run -------------------------------------------------------------------

declare -a REPORTS=()
declare -a LABELS=()
declare -a FAILED_CORPORA=()
RUN_STATUS=0
OUT_ROOT="$SNAPSHOT_DIR/eval/out"

# Do not leave a previous all-corpus summary looking like the result of this run.
rm -f "$OUT_ROOT/summary.html"

for name in "${CORPORA[@]}"; do
  rel="$(corpus_dir "$name")"
  dir="$ROOT/resources/html/$rel"
  label="html-$name"
  out="$OUT_ROOT/$label"

  echo ""
  echo "=== HTML eval: $name  ($rel)  [concurrency=$CONCURRENCY engine=$BROWSER_ENGINE] ==="
  # A failed invocation must not be summarized from a report left by an older
  # run. Preserve per-case artifacts (needed by --skip-existing), but invalidate
  # the three run-level reports until eval recreates them.
  rm -f "$out/report.csv" "$out/report.md" "$out/index.html"
  # Aligned with: run.sh --recursive --corpus <dir> --label <name> \
  #                       --concurrency 16 --browser-engine playwright
  # shellcheck disable=SC2086
  if "$EVAL" --recursive --corpus "$dir" --label "$label" \
             --concurrency "$CONCURRENCY" --browser-engine "$BROWSER_ENGINE" $EVAL_EXTRA_ARGS; then
    if [ -f "$out/report.csv" ] && [ -f "$out/report.md" ] && [ -f "$out/index.html" ]; then
      REPORTS+=("$out")
      LABELS+=("$label")
    else
      echo "run_html_eval: '$name' completed without all expected report files" >&2
      FAILED_CORPORA+=("$name")
      RUN_STATUS=1
    fi
  else
    echo "run_html_eval: eval failed for corpus '$name'" >&2
    FAILED_CORPORA+=("$name")
    RUN_STATUS=1
  fi
done

# --- summary ---------------------------------------------------------------

echo ""
echo "=== HTML eval reports (report-only, no pass/fail gate) ==="
if [ "${#REPORTS[@]}" -gt 0 ]; then
  for out in "${REPORTS[@]}"; do
    echo "  report:  $out/report.md"
    echo "  viewer:  $out/index.html"
  done
else
  echo "  (none generated successfully)"
fi

# Aggregate every corpus that ran into one cross-corpus summary: print the
# rolled-up table to the console, write out/summary.html, and gate the
# per-corpus means against the committed baseline (or re-seed it on request).
if [ "${#LABELS[@]}" -gt 0 ]; then
  declare -a SUMMARY_ARGS=(--out "$OUT_ROOT" --baseline "$HTML_BASELINE")
  if [ "$HTML_EVAL_UPDATE_BASELINE" != "0" ]; then
    SUMMARY_ARGS+=(--update-baseline)
    echo "run_html_eval: re-seeding baseline from this run -> $HTML_BASELINE" >&2
  fi
  # A partial run (some corpora failed) must never rewrite the baseline: the
  # new file would drop the missing corpora's entries.
  if [ "$HTML_EVAL_UPDATE_BASELINE" != "0" ] && [ "${#FAILED_CORPORA[@]}" -gt 0 ]; then
    echo "run_html_eval: refusing to update baseline; corpora failed this run: ${FAILED_CORPORA[*]}" >&2
    RUN_STATUS=1
  elif node "$SNAPSHOT_DIR/eval/summary.js" "${SUMMARY_ARGS[@]}" "${LABELS[@]}"; then
    echo ""
    echo "Open the summary:      open $OUT_ROOT/summary.html"
    echo "Open a corpus viewer:  open ${REPORTS[0]}/index.html"
  else
    # summary.js exits non-zero on a baseline-gate regression as well as on a
    # genuine summary failure; both must fail the overall run.
    echo "run_html_eval: cross-corpus summary/baseline gate failed" >&2
    RUN_STATUS=1
  fi
else
  echo "run_html_eval: no corpus completed successfully; no summary was generated" >&2
  RUN_STATUS=1
fi

if [ "${#FAILED_CORPORA[@]}" -gt 0 ]; then
  echo "" >&2
  echo "run_html_eval: failed corpora: ${FAILED_CORPORA[*]}" >&2
fi

exit "$RUN_STATUS"
