#!/usr/bin/env bash
# PPT visual-fidelity evaluation driver used by the CMake PPTTest target.
set -u
set -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL_DIR="$ROOT/tools/ppt-eval"
OUT_ROOT="${PPT_EVAL_OUT:-$TOOL_DIR/out}"
PAGX_BIN="${PAGX_BIN:-$ROOT/cmake-build-debug/pagx}"
SOFFICE_BIN="${SOFFICE_BIN:-}"
PDF_RASTERIZER_BIN="${PDF_RASTERIZER_BIN:-}"
CONCURRENCY="${CONCURRENCY:-2}"
PPT_EVAL_ONLY="${PPT_EVAL_ONLY:-}"
PPT_EVAL_CORPORA="${PPT_EVAL_CORPORA:-}"
PPT_EVAL_EXTRA_ARGS="${PPT_EVAL_EXTRA_ARGS:-}"
PPT_EVAL_ALLOW_PNG_FALLBACK="${PPT_EVAL_ALLOW_PNG_FALLBACK:-0}"
PPT_EVAL_UPDATE_BASELINE="${PPT_EVAL_UPDATE_BASELINE:-0}"
PPT_EVAL_REQUIRE_BASELINE="${PPT_EVAL_REQUIRE_BASELINE:-0}"
PPT_BASELINE="${PPT_BASELINE:-$ROOT/resources/ppt/baseline.json}"

DEFAULT_CORPORA=(features layout text cli spec smoke decks)
VALID_CORPORA=(features layout text cli spec smoke decks)
if [ "$#" -gt 0 ]; then
  CORPORA=("$@")
elif [ -n "$PPT_EVAL_CORPORA" ]; then
  read -r -a CORPORA <<< "${PPT_EVAL_CORPORA//,/ }"
else
  CORPORA=("${DEFAULT_CORPORA[@]}")
fi

contains_corpus() {
  local wanted="$1"
  local item
  for item in "${VALID_CORPORA[@]}"; do
    [ "$item" = "$wanted" ] && return 0
  done
  return 1
}

if [ ! -x "$PAGX_BIN" ]; then
  echo "run_ppt_eval: pagx binary not found or not executable: $PAGX_BIN" >&2
  exit 1
fi
if ! command -v node >/dev/null 2>&1; then
  echo "run_ppt_eval: node is required" >&2
  exit 1
fi
if [ -z "$SOFFICE_BIN" ]; then
  SOFFICE_BIN="$(command -v soffice 2>/dev/null || true)"
fi
if [ -z "$SOFFICE_BIN" ] && [ -x "/Applications/LibreOffice.app/Contents/MacOS/soffice" ]; then
  SOFFICE_BIN="/Applications/LibreOffice.app/Contents/MacOS/soffice"
fi
if [ -z "$SOFFICE_BIN" ] || [ ! -x "$SOFFICE_BIN" ]; then
  echo "run_ppt_eval: LibreOffice soffice is required; set SOFFICE_BIN" >&2
  exit 1
fi
if [ -z "$PDF_RASTERIZER_BIN" ]; then
  PDF_RASTERIZER_BIN="$(command -v pdftocairo 2>/dev/null || command -v pdftoppm 2>/dev/null || true)"
fi
if [ -z "$PDF_RASTERIZER_BIN" ] && [ "$PPT_EVAL_ALLOW_PNG_FALLBACK" = "0" ]; then
  echo "run_ppt_eval: pdftocairo or pdftoppm is required for stable multi-page rendering" >&2
  echo "run_ppt_eval: install Poppler, or set PPT_EVAL_ALLOW_PNG_FALLBACK=1 for local single-slide runs" >&2
  exit 1
fi
for corpus in "${CORPORA[@]}"; do
  if ! contains_corpus "$corpus"; then
    echo "run_ppt_eval: unknown corpus '$corpus' (valid: ${VALID_CORPORA[*]})" >&2
    exit 2
  fi
done

echo "run_ppt_eval: installing Node dependencies from package-lock.json..."
(cd "$TOOL_DIR" && npm install) || {
  echo "run_ppt_eval: npm install failed" >&2
  exit 1
}

mkdir -p "$OUT_ROOT"
rm -f "$OUT_ROOT/summary.html"

RUN_STATUS=0
COMPLETED=()
for corpus in "${CORPORA[@]}"; do
  out="$OUT_ROOT/ppt-$corpus"
  rm -f "$out/report.csv" "$out/report.md" "$out/report.json" "$out/index.html"
  args=(
    --manifest "$ROOT/resources/ppt/corpora.json"
    --corpus "$corpus"
    --out "$out"
    --pagx-bin "$PAGX_BIN"
    --soffice "$SOFFICE_BIN"
    --concurrency "$CONCURRENCY"
  )
  if [ -n "$PDF_RASTERIZER_BIN" ]; then
    args+=(--pdf-rasterizer "$PDF_RASTERIZER_BIN")
  fi
  if [ "$PPT_EVAL_ALLOW_PNG_FALLBACK" != "0" ]; then
    args+=(--allow-png-fallback)
  fi
  if [ -n "$PPT_EVAL_ONLY" ]; then
    args+=(--only "$PPT_EVAL_ONLY")
  fi
  echo ""
  echo "=== PPT eval: $corpus ==="
  # shellcheck disable=SC2086
  if node "$TOOL_DIR/run.js" "${args[@]}" $PPT_EVAL_EXTRA_ARGS; then
    COMPLETED+=("$corpus")
  else
    echo "run_ppt_eval: corpus '$corpus' failed" >&2
    RUN_STATUS=1
  fi
done

if [ "${#COMPLETED[@]}" -eq 0 ]; then
  echo "run_ppt_eval: no corpus completed successfully" >&2
  exit 1
fi

summary_args=(--out "$OUT_ROOT" --baseline "$PPT_BASELINE")
if [ "$PPT_EVAL_REQUIRE_BASELINE" != "0" ]; then
  summary_args+=(--require-baseline)
fi
if [ "$PPT_EVAL_UPDATE_BASELINE" != "0" ]; then
  if [ -n "$PPT_EVAL_ONLY" ] || [[ "$PPT_EVAL_EXTRA_ARGS" == *"--only"* ]]; then
    echo "run_ppt_eval: refusing to update baseline from a filtered run" >&2
    exit 1
  fi
  if [ -z "$PDF_RASTERIZER_BIN" ]; then
    echo "run_ppt_eval: refusing to update baseline from the LibreOffice PNG fallback" >&2
    exit 1
  fi
  if [ "$RUN_STATUS" -ne 0 ] || [ "${#COMPLETED[@]}" -ne "${#CORPORA[@]}" ]; then
    echo "run_ppt_eval: refusing to update baseline after a partial or failed run" >&2
    exit 1
  fi
  summary_args+=(--update-baseline)
fi
if ! node "$TOOL_DIR/summary.js" "${summary_args[@]}" "${COMPLETED[@]}"; then
  RUN_STATUS=1
fi

echo ""
echo "PPT reports: $OUT_ROOT/summary.html"
exit "$RUN_STATUS"
