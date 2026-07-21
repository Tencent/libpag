#!/usr/bin/env bash
#
# run_html_eval.sh — HTML browser-fidelity eval driver.
#
# Runs the full snapshot -> pagx import -> pagx render -> compare pipeline
# (tools/html-snapshot/eval) over every HTML corpus committed under
# resources/html/, and emits a per-corpus report. This is the entry point
# behind the CMake `HTMLTest` target.
#
# It is report-only: it never fails on low SSIM. Browser fidelity is a
# corpus-level mean metric, not a hard pass/fail gate, so inspect the
# generated report.md / index.html by hand.
#
# Prerequisites:
#   - a built `pagx` binary (default: <repo>/cmake-build-debug/pagx, or $PAGX_BIN)
#   - Node.js + a headless Chromium (installed via `npm install` in
#     tools/html-snapshot; the script bootstraps it on first run)
#   - network access for `corpus/websites` and `corpus/generated` (they pull
#     CDN CSS / web fonts / external images at eval time)
#
# Usage:
#   test/run_html_eval.sh                 # all corpora
#   test/run_html_eval.sh cases cli       # only the named corpora
#   PAGX_BIN=/path/to/pagx test/run_html_eval.sh
#   EVAL_EXTRA_ARGS="--only bilibili" test/run_html_eval.sh websites
#   CONCURRENCY=8 BROWSER_ENGINE=puppeteer test/run_html_eval.sh
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

ALL_CORPORA="cases cli websites generated"
CORPORA="${*:-$ALL_CORPORA}"

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

# Always refresh the html-snapshot toolchain (deps + compiled dist/) before a run.
echo "run_html_eval: installing html-snapshot dependencies (npm install)..."
(cd "$SNAPSHOT_DIR" && npm install) || { echo "run_html_eval: npm install failed" >&2; exit 1; }
echo "run_html_eval: building html-snapshot (npm run build)..."
(cd "$SNAPSHOT_DIR" && npm run build) || { echo "run_html_eval: npm run build failed" >&2; exit 1; }

# --- run -------------------------------------------------------------------

declare -a REPORTS=()
declare -a LABELS=()
for name in $CORPORA; do
  rel="$(corpus_dir "$name")"
  if [ -z "$rel" ]; then
    echo "run_html_eval: unknown corpus '$name' (valid: $ALL_CORPORA)" >&2
    continue
  fi
  dir="$ROOT/resources/html/$rel"
  if [ ! -d "$dir" ]; then
    echo "run_html_eval: corpus dir missing, skipping: $dir" >&2
    continue
  fi
  label="html-$name"

  echo ""
  echo "=== HTML eval: $name  ($rel)  [concurrency=$CONCURRENCY engine=$BROWSER_ENGINE] ==="
  # Aligned with: run.sh --recursive --corpus <dir> --label <name> \
  #                       --concurrency 16 --browser-engine playwright
  # shellcheck disable=SC2086
  "$EVAL" --recursive --corpus "$dir" --label "$label" \
          --concurrency "$CONCURRENCY" --browser-engine "$BROWSER_ENGINE" $EVAL_EXTRA_ARGS

  REPORTS+=("$SNAPSHOT_DIR/eval/out/$label")
  LABELS+=("$label")
done

# --- summary ---------------------------------------------------------------

echo ""
echo "=== HTML eval reports (report-only, no pass/fail gate) ==="
for out in "${REPORTS[@]}"; do
  echo "  report:  $out/report.md"
  echo "  viewer:  $out/index.html"
done

# Aggregate every corpus that ran into one cross-corpus summary: print the
# rolled-up table to the console and write out/summary.html.
if [ "${#LABELS[@]}" -gt 0 ]; then
  node "$SNAPSHOT_DIR/eval/summary.js" --out "$SNAPSHOT_DIR/eval/out" "${LABELS[@]}"
  echo ""
  echo "Open the summary:      open $SNAPSHOT_DIR/eval/out/summary.html"
  echo "Open a corpus viewer:  open ${REPORTS[0]}/index.html"
fi
