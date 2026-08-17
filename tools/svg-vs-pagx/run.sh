#!/usr/bin/env bash
# svg-vs-pagx: compare an SVG SMIL animation against its PAGX conversion frame by frame.
#
# Pipeline:
#   1. pagx import <svg>       -> <name>.pagx
#   2. pagx render-frames      -> pagx_frames/ (PAGX renderer)
#   3. node svg-frames.js      -> svg_frames/  (Chromium SVG renderer, ground truth)
#   4. node compare.js         -> per-frame metrics + diff images
#
# Usage:
#   ./run.sh <input.svg> [--fps 60] [--out-dir <dir>] [--pagx <path-to-pagx-binary>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAGX_BIN="${PAGX_BIN:-}"
FPS=60
OUT_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fps) FPS="$2"; shift 2 ;;
    --out-dir) OUT_DIR="$2"; shift 2 ;;
    --pagx) PAGX_BIN="$2"; shift 2 ;;
    *) INPUT_SVG="$1"; shift ;;
  esac
done

if [[ -z "${INPUT_SVG:-}" ]]; then
  echo "Usage: $0 <input.svg> [--fps 60] [--out-dir <dir>] [--pagx <binary>]"
  exit 1
fi

if [[ -z "$PAGX_BIN" ]]; then
  # Resolve the pagx CLI relative to the libpag repo root (this script lives in tools/svg-vs-pagx).
  PAGX_BIN="$SCRIPT_DIR/../../cmake-build-debug/pagx"
fi
if [[ ! -x "$PAGX_BIN" ]]; then
  echo "svg-vs-pagx: pagx binary not found at $PAGX_BIN (build it or pass --pagx)"
  exit 1
fi

NAME="$(basename "$INPUT_SVG" .svg)"
WORK_DIR="${OUT_DIR:-/tmp/svg-vs-pagx-$NAME}"
PAGX_FILE="$WORK_DIR/$NAME.pagx"
SVG_FRAMES="$WORK_DIR/svg_frames"
PAGX_FRAMES="$WORK_DIR/pagx_frames"
COMPARE_OUT="$WORK_DIR/compare"

mkdir -p "$WORK_DIR"

echo "=== 1/4 import SVG -> PAGX ==="
"$PAGX_BIN" import --input "$INPUT_SVG" --output "$PAGX_FILE"

echo "=== 2/4 render PAGX frames ==="
"$PAGX_BIN" render-frames "$PAGX_FILE" -o "$PAGX_FRAMES" --fps "$FPS"

echo "=== 3/4 render SVG frames (Chromium) ==="
node "$SCRIPT_DIR/svg-frames.js" --input "$INPUT_SVG" --output "$SVG_FRAMES" --fps "$FPS"

echo "=== 4/4 compare ==="
node "$SCRIPT_DIR/compare.js" --svg-dir "$SVG_FRAMES" --pagx-dir "$PAGX_FRAMES" --output "$COMPARE_OUT"

echo ""
echo "Outputs:"
echo "  PAGX:        $PAGX_FILE"
echo "  SVG frames:  $SVG_FRAMES/"
echo "  PAGX frames: $PAGX_FRAMES/"
echo "  comparison:  $COMPARE_OUT/"
