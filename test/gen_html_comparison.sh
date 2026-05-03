#!/usr/bin/env bash
# Builds a side-by-side rendering comparison page for every sample in
# resources/pagx_to_html/, outputting a self-contained directory that can be
# served or zipped for review.
#
# By default the comparison has three columns:
#
#   column 1 — tgfx native render (2x PNG from `pagx render`)
#   column 2 — the HTML emitted by the test harness (PAGXHtmlTest.BatchConvertAll)
#   column 3 — the HTML emitted by the `pagx export` CLI   (suppressed with --without-cli)
#
# Pass --without-cli to drop column 3 when you only want to compare the
# PAGX-native baseline against the test-harness HTML (e.g. when the CLI
# export path is intentionally broken mid-refactor and you don't want its
# failures to pollute the page).
#
# Output layout (rooted at test/out/html-comparison/):
#
#   html-comparison/
#   ├── index.html
#   ├── pagx-render/<name>.png
#   ├── test/
#   │   ├── <name>.html
#   │   ├── fonts/...          # bundled by BatchConvertAll's CopyBundledFontsTo
#   │   └── static-img/...     # bundled by BatchConvertAll's HTMLExporter
#   └── cli/                   # only when CLI column is included
#       ├── <name>.html
#       ├── fonts/...
#       └── static-img/<name>/...
#
# All references inside index.html and the two iframe-hosted HTMLs are
# relative, so the directory is portable — copy the whole html-comparison/ to
# any machine or HTTP server and the three columns still render.
#
# Preconditions the script enforces:
#   - cmake-build-debug/pagx executable must exist
#   - test/out/PAGXHtmlTest/<name>.html and its sibling fonts/ and static-img/
#     must exist (i.e. run `PAGFullTest --gtest_filter=PAGXHtmlTest.BatchConvertAll`
#     before this script)
#
# Usage:
#   bash test/gen_html_comparison.sh [--with-cli|--without-cli] [--bold-font <path>]
#
# --with-cli is the default (the script runs `pagx export` to produce column 3).
# --without-cli skips all CLI work and emits a two-column index.
# --bold-font lets you point at a local NotoSansSC-Bold file if you have one;
# the default is to skip the bold typeface because the repo doesn't bundle it,
# in which case the CDN fallback in the generated CSS handles Bold rendering
# at runtime.

set -euo pipefail

#-------------------------------------------------------------------------
# Argument parsing
#-------------------------------------------------------------------------

WITH_CLI=1
BOLD_FONT=""

while [ $# -gt 0 ]; do
  case "$1" in
    --with-cli)
      WITH_CLI=1
      shift
      ;;
    --without-cli)
      WITH_CLI=0
      shift
      ;;
    --bold-font)
      BOLD_FONT="$2"
      shift 2
      ;;
    -h|--help)
      sed -n '2,/^$/p' "$0" | sed 's/^# //; s/^#$//'
      exit 0
      ;;
    *)
      echo "error: unknown argument '$1'" >&2
      exit 2
      ;;
  esac
done

#-------------------------------------------------------------------------
# Paths
#-------------------------------------------------------------------------

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAGX_BIN="$REPO/cmake-build-debug/pagx"
PAGX_DIR="$REPO/resources/pagx_to_html"
TEST_OUT="$REPO/test/out/PAGXHtmlTest"
OUT_DIR="$REPO/test/out/html-comparison"

# Local font files. Regular is bundled in the repo; Emoji and Hebrew are optional
# fallback typefaces `pagx render` needs to avoid substituting system fonts when
# a sample uses those scripts.
FONT_REGULAR="$REPO/resources/font/NotoSansSC-Regular.otf"
FONT_EMOJI="$REPO/resources/font/NotoColorEmoji.ttf"
FONT_HEBREW="$REPO/resources/font/NotoSansHebrew-Regular.ttf"

# Upstream Google Fonts CDN URLs — duplicated from WrapHtmlDocument in
# test/src/PAGXHtmlTest.cpp so the CLI-generated HTMLs list the same fallback
# source as the test-embedded ones. If those URLs change, update both sides.
CDN_REGULAR="https://fonts.gstatic.com/s/notosanssc/v40/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaG9_FnYw.ttf"
CDN_BOLD="https://fonts.gstatic.com/s/notosanssc/v40/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaGzjCnYw.ttf"

#-------------------------------------------------------------------------
# Preconditions
#-------------------------------------------------------------------------

if [ ! -x "$PAGX_BIN" ]; then
  echo "error: pagx binary not found at $PAGX_BIN" >&2
  echo "       build it first: cmake --build cmake-build-debug --target pagx" >&2
  exit 1
fi

if ! ls "$TEST_OUT"/*.html >/dev/null 2>&1; then
  echo "error: no test-embedded HTMLs found in $TEST_OUT" >&2
  echo "       run PAGFullTest --gtest_filter=PAGXHtmlTest.BatchConvertAll first" >&2
  exit 1
fi

for font in "$FONT_REGULAR" "$FONT_EMOJI" "$FONT_HEBREW"; do
  if [ ! -f "$font" ]; then
    echo "error: bundled font file not found: $font" >&2
    exit 1
  fi
done

# The optional Bold face: if the user did not pass --bold-font, probe the
# default Downloads location, and fall back to "no Bold" (silent skip) if it
# is not present either. When absent, every column that would otherwise load
# a local Bold face falls back to the CDN Bold URL via the multi-source CSS.
if [ -z "$BOLD_FONT" ]; then
  DEFAULT_BOLD="$HOME/Downloads/Noto_Sans_SC/static/NotoSansSC-Bold.ttf"
  if [ -f "$DEFAULT_BOLD" ]; then
    BOLD_FONT="$DEFAULT_BOLD"
  fi
fi
if [ -n "$BOLD_FONT" ] && [ ! -f "$BOLD_FONT" ]; then
  echo "error: --bold-font file not found: $BOLD_FONT" >&2
  exit 1
fi

#-------------------------------------------------------------------------
# Clean output
#-------------------------------------------------------------------------

# Remove any stale run. We reconstruct the whole directory every time because
# the set of samples and assets changes between pagx revisions, and dangling
# files from an older run would show up in the comparison as ghosts.
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/pagx-render"
mkdir -p "$OUT_DIR/test"
if [ "$WITH_CLI" = "1" ]; then
  mkdir -p "$OUT_DIR/cli"
fi

#-------------------------------------------------------------------------
# Column 1: pagx render (2x PNG)
#-------------------------------------------------------------------------

# Build the --font arg list once. The positional --fallback args mirror the
# CreateFallbackTypefaces() list that the tgfx-native baseline uses, so the
# PNGs this script produces are byte-for-byte close to the ones PAGXTest
# writes into test/out/PAGXHtmlTest/*_pagx.png (see memory
# pagx_render_vs_displaylist_scale for why they are not bit-exact).
RENDER_FONT_ARGS=(
  --font    "$FONT_REGULAR"
  --fallback "$FONT_REGULAR"
  --fallback "$FONT_EMOJI"
  --fallback "$FONT_HEBREW"
)
if [ -n "$BOLD_FONT" ]; then
  RENDER_FONT_ARGS+=( --font "$BOLD_FONT" )
fi

echo "Rendering column 1 (pagx render --scale 2) ..."
count=0
for pagx in "$PAGX_DIR"/*.pagx; do
  name="$(basename "$pagx" .pagx)"
  "$PAGX_BIN" render --scale 2 \
    "${RENDER_FONT_ARGS[@]}" \
    -o "$OUT_DIR/pagx-render/$name.png" \
    "$pagx" >/dev/null
  count=$((count + 1))
done
echo "  rendered $count PNGs"

#-------------------------------------------------------------------------
# Column 2: copy test-embedded HTMLs and their sibling assets
#-------------------------------------------------------------------------

echo "Copying column 2 (test-embedded HTMLs + fonts/ + static-img/) ..."
# Copy every *.html generated by BatchConvertAll. We deliberately skip any
# ancillary files (like the old GenerateComparisonPage's comparison.html)
# by filtering on the sample-name list so the column-2 directory stays
# aligned with the sample set.
for pagx in "$PAGX_DIR"/*.pagx; do
  name="$(basename "$pagx" .pagx)"
  src="$TEST_OUT/$name.html"
  if [ -f "$src" ]; then
    cp "$src" "$OUT_DIR/test/$name.html"
  fi
done
# The wrapper's @font-face rules reference `fonts/...` relative to the HTML,
# and any sample using DiamondGradient/tiled ImagePattern references
# `static-img/...`. Both directories live alongside the test-embedded HTMLs,
# so copying them over keeps the iframe self-contained.
if [ -d "$TEST_OUT/fonts" ]; then
  cp -R "$TEST_OUT/fonts" "$OUT_DIR/test/fonts"
fi
if [ -d "$TEST_OUT/static-img" ]; then
  cp -R "$TEST_OUT/static-img" "$OUT_DIR/test/static-img"
fi

#-------------------------------------------------------------------------
# Column 3: pagx export with multi-source @font-face
#-------------------------------------------------------------------------

if [ "$WITH_CLI" = "1" ]; then
  echo "Exporting column 3 (pagx export with multi-source fonts) ..."

  # Base --html-font list: Regular local + Regular CDN aggregate into one
  # @font-face rule with two sources; Bold CDN is its own rule. If a local
  # Bold was found, we additionally list it as the Bold rule's first source
  # so browsers try the local file before the CDN. All of this relies on the
  # CLI aggregation rule: repeated --html-font flags with the same
  # (family, weight, style) triple fold into a single rule.
  EXPORT_FONT_ARGS=(
    --html-font "$FONT_REGULAR"
    --html-font "${CDN_REGULAR}#family=Noto Sans SC#weight=400#style=normal"
    --html-font "${CDN_BOLD}#family=Noto Sans SC#weight=700#style=normal"
  )
  if [ -n "$BOLD_FONT" ]; then
    # Insert the local Bold before the CDN Bold in the aggregated rule.
    EXPORT_FONT_ARGS=(
      --html-font "$FONT_REGULAR"
      --html-font "${CDN_REGULAR}#family=Noto Sans SC#weight=400#style=normal"
      --html-font "$BOLD_FONT"
      --html-font "${CDN_BOLD}#family=Noto Sans SC#weight=700#style=normal"
    )
  fi

  count=0
  for pagx in "$PAGX_DIR"/*.pagx; do
    name="$(basename "$pagx" .pagx)"
    # After the first sample the shared fonts/ directory already has the
    # bundled font files, so subsequent exports emit a harmless "overwriting"
    # warning for each one; filter those out while letting real errors through.
    "$PAGX_BIN" export \
      --input "$pagx" \
      --output "$OUT_DIR/cli/$name.html" \
      "${EXPORT_FONT_ARGS[@]}" 2>&1 \
      | grep -v "warning: overwriting '$OUT_DIR/cli/fonts/" || true
    count=$((count + 1))
  done
  echo "  exported $count HTMLs"
fi

#-------------------------------------------------------------------------
# Build index.html
#-------------------------------------------------------------------------

echo "Building index.html ..."
# Pass the CLI-column toggle to the Python generator so the index mirrors whatever
# directory layout this shell script produced. Two flags mirror bash's style instead
# of coupling both sides to a numeric "columns" count — adding a new column later
# just means adding a new --with-<thing> toggle on both sides.
if [ "$WITH_CLI" = "1" ]; then
  python3 "$REPO/test/gen_html_comparison.py" "$OUT_DIR" --with-cli
else
  python3 "$REPO/test/gen_html_comparison.py" "$OUT_DIR" --without-cli
fi

INDEX="$OUT_DIR/index.html"
echo ""
echo "Open: $INDEX"
open "$INDEX" 2>/dev/null || true
