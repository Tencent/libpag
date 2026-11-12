#!/usr/bin/env bash
# Runs the D6 SVG browser-fidelity gate over the supplied PAGX files (or a default layout sample).
# A case may carry an animation seek time as `path.pagx@seconds`; the gate pauses SMIL, seeks to
# that time in the browser and renders the reference with `pagx render --time`.
# Requires a headless Chromium installed by Playwright (tools/html-snapshot).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAGX_BIN="${PAGX_BIN:-$ROOT/cmake-build-debug/pagx}"
OUT_DIR="${SVG_BROWSER_OUT:-$ROOT/test/out/svg-browser-gate}"

CASES=("$@")
if [ ${#CASES[@]} -eq 0 ]; then
  if [ -n "${SVG_BROWSER_CASES:-}" ]; then
    # shellcheck disable=SC2206
    CASES=(${SVG_BROWSER_CASES})
  else
    CASES=("$ROOT/resources/layout/constraint_textbox.pagx")
  fi
fi

STATUS=0
for CASE in "${CASES[@]}"; do
  CASE_FILE="$CASE"
  CASE_TIME=""
  if [[ "$CASE" == *@* ]]; then
    CASE_FILE="${CASE%@*}"
    CASE_TIME="${CASE##*@}"
  fi
  if [ ! -f "$CASE_FILE" ]; then
    echo "svg_browser_gate: missing case '$CASE_FILE'" >&2
    STATUS=2
    continue
  fi
  echo "svg_browser_gate: $CASE_FILE${CASE_TIME:+ @${CASE_TIME}s}"
  TIME_ARGS=()
  if [ -n "$CASE_TIME" ]; then
    TIME_ARGS=(--time "$CASE_TIME")
  fi
  node "$ROOT/test/svg_browser_gate.js" \
    --pagx "$CASE_FILE" \
    --pagx-bin "$PAGX_BIN" \
    --out "$OUT_DIR" \
    "${TIME_ARGS[@]}" || STATUS=$?
done

exit "$STATUS"
