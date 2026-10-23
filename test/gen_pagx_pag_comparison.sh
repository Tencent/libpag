#!/usr/bin/env bash
# Builds the PAGX / PAG v4 rendering comparison page (design doc §10.3):
#
#   col 1 - PAGX direct render (2x PNG from 'pagx render')
#   col 2 - v4 binary render ('pagx export --format pag' then 'pagx render')
#   col 3 - pixel diff of the two (diff pixels highlighted, with max / mean /
#           diff-pixel ratio); a sample passes when the two renders are exactly
#           identical, matching the Baseline::Compare screenshot threshold.
#
# Sections (one tab per source directory): the five repo sample directories plus
# every subdirectory of resources/pagx_to_pag/.
#
# Usage:
#   bash test/gen_pagx_pag_comparison.sh [--only <section>[,<section>...]]

set -euo pipefail

ONLY=""
while [ $# -gt 0 ]; do
  case "$1" in
    --only)
      ONLY="$2"; shift 2 ;;
    -h|--help)
      sed -n '2,/^$/p' "$0" | sed 's/^# //; s/^#$//'; exit 0 ;;
    *)
      echo "error: unknown argument '$1'" >&2; exit 2 ;;
  esac
done

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAGX_BIN="$REPO/cmake-build-debug/pagx"
OUT_DIR="$REPO/test/out/pagx_pag_comparison"

SECTION_IDS=(pagx_to_html cli layout text spec)
SECTION_SRCDIRS=(
  "resources/pagx_to_html"
  "resources/cli"
  "resources/layout"
  "resources/text"
  "spec/samples"
)
SECTION_TITLES=(pagx_to_html cli layout text spec)

shopt -s nullglob
for dir in "$REPO"/resources/pagx_to_pag/*/; do
  name="$(basename "$dir")"
  SECTION_IDS+=("ptp_$name")
  SECTION_SRCDIRS+=("resources/pagx_to_pag/$name")
  SECTION_TITLES+=("pagx_to_pag/$name")
done
shopt -u nullglob

if [ -n "$ONLY" ]; then
  FILTERED_IDS=(); FILTERED_SRCDIRS=(); FILTERED_TITLES=()
  IFS=',' read -ra ONLY_LIST <<< "$ONLY"
  for i in "${!SECTION_IDS[@]}"; do
    for want in "${ONLY_LIST[@]}"; do
      if [ "${SECTION_IDS[$i]}" = "$want" ] || [ "${SECTION_TITLES[$i]}" = "$want" ]; then
        FILTERED_IDS+=("${SECTION_IDS[$i]}")
        FILTERED_SRCDIRS+=("${SECTION_SRCDIRS[$i]}")
        FILTERED_TITLES+=("${SECTION_TITLES[$i]}")
      fi
    done
  done
  if [ ${#FILTERED_IDS[@]} -eq 0 ]; then
    echo "error: no section matches --only '$ONLY'" >&2; exit 1
  fi
  SECTION_IDS=("${FILTERED_IDS[@]}")
  SECTION_SRCDIRS=("${FILTERED_SRCDIRS[@]}")
  SECTION_TITLES=("${FILTERED_TITLES[@]}")
fi

# Font files for pagx render (same registration as gen_html_comparison.sh) so both
# columns measure text with the exact Arial typefaces on macOS.
ARIAL_REGULAR="/System/Library/Fonts/Supplemental/Arial.ttf"
ARIAL_BOLD="/System/Library/Fonts/Supplemental/Arial Bold.ttf"
ARIAL_ITALIC="/System/Library/Fonts/Supplemental/Arial Italic.ttf"
ARIAL_BOLD_ITALIC="/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf"
RENDER_FONT_ARGS=()
for f in "$ARIAL_REGULAR" "$ARIAL_BOLD" "$ARIAL_ITALIC" "$ARIAL_BOLD_ITALIC"; do
  if [ -f "$f" ]; then
    RENDER_FONT_ARGS+=( --font "$f" )
  fi
done

echo "== rebuilding pagx and PAGFullTest targets =="
cmake --build "$REPO/cmake-build-debug" --target pagx PAGFullTest 2>&1 | tail -5
echo ""
if [ ! -x "$PAGX_BIN" ]; then
  echo "error: pagx binary not found at $PAGX_BIN" >&2
  exit 1
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# Simple TSV manifest: section id, section title, sample name, source (repo-relative).
MANIFEST="$OUT_DIR/manifest.tsv"
rm -f "$MANIFEST"

total_samples=0
total_compared=0
total_failed=0

for i in "${!SECTION_IDS[@]}"; do
  section="${SECTION_IDS[$i]}"
  srcdir="$REPO/${SECTION_SRCDIRS[$i]}"

  if [ ! -d "$srcdir" ]; then
    echo "warn: section '$section' source dir missing: $srcdir - skipping"
    continue
  fi

  shopt -s nullglob
  pagx_files=( "$srcdir"/*.pagx )
  shopt -u nullglob
  if [ ${#pagx_files[@]} -eq 0 ]; then
    continue
  fi

  echo ""
  echo "== section: ${SECTION_TITLES[$i]} (${#pagx_files[@]} samples) =="

  mkdir -p "$OUT_DIR/pagx-render/$section" "$OUT_DIR/pag/$section" \
           "$OUT_DIR/pag-render/$section" "$OUT_DIR/reports/$section"

  section_compared=0
  section_failed=0

  for pagx in "${pagx_files[@]}"; do
    name="$(basename "$pagx" .pagx)"
    total_samples=$((total_samples + 1))
    printf '%s\t%s\t%s\t%s\n' "$section" "${SECTION_TITLES[$i]}" "$name" "${SECTION_SRCDIRS[$i]}/$name.pagx" >> "$MANIFEST"

    # Column 1: PAGX direct render (external sub PAGX files load from its directory).
    if ! "$PAGX_BIN" render --scale 2 "${RENDER_FONT_ARGS[@]}" \
        -o "$OUT_DIR/pagx-render/$section/$name.png" "$pagx" >/dev/null 2>&1; then
      rm -f "$OUT_DIR/pagx-render/$section/$name.png"
      section_failed=$((section_failed + 1))
      continue
    fi

    # Export to the v4 binary with a deterministic report, then render it (column 2).
    if ! "$PAGX_BIN" export --input "$pagx" \
        --output "$OUT_DIR/pag/$section/$name.pag" \
        --report "$OUT_DIR/reports/$section/$name.json" >/dev/null 2>&1; then
      rm -f "$OUT_DIR/pag/$section/$name.pag" "$OUT_DIR/reports/$section/$name.json"
      section_failed=$((section_failed + 1))
      continue
    fi
    if ! "$PAGX_BIN" render --scale 2 "${RENDER_FONT_ARGS[@]}" \
        -o "$OUT_DIR/pag-render/$section/$name.png" \
        "$OUT_DIR/pag/$section/$name.pag" >/dev/null 2>&1; then
      rm -f "$OUT_DIR/pag-render/$section/$name.png"
      section_failed=$((section_failed + 1))
      continue
    fi
    section_compared=$((section_compared + 1))
  done

  total_compared=$((total_compared + section_compared))
  total_failed=$((total_failed + section_failed))
  echo "  compared $section_compared sample(s), $section_failed failed/skipped"
done

echo ""
echo "Building index.html ..."
python3 "$REPO/test/gen_pagx_pag_comparison.py" "$OUT_DIR" "$REPO"

echo ""
echo "Summary: $total_samples sample(s), $total_compared compared, $total_failed failed/skipped"
echo "Open: $OUT_DIR/index.html"
open "$OUT_DIR/index.html" 2>/dev/null || true
