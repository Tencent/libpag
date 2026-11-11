#!/usr/bin/env bash
# Runs the D6 SVG browser-fidelity gate over the supplied PAGX files (or a default layout sample).
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
  if [ ! -f "$CASE" ]; then
    echo "svg_browser_gate: missing case '$CASE'" >&2
    STATUS=2
    continue
  fi
  echo "svg_browser_gate: $CASE"
  node "$ROOT/test/svg_browser_gate.js" \
    --pagx "$CASE" \
    --pagx-bin "$PAGX_BIN" \
    --out "$OUT_DIR" || STATUS=$?
done

exit "$STATUS"
