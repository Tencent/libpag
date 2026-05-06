#!/usr/bin/env bash
# Builds a side-by-side rendering comparison page that covers every PAGX sample
# available in the repo. The samples live in five separate directories, each
# with a different focus (see "Section layout" below). The page is organised as
# a section per source directory with a sticky top tab bar for quick navigation.
#
# Section layout and column count:
#
#   pagx_to_html  — three columns:
#     col 1 — tgfx native render (2x PNG from `pagx render`)
#     col 2 — test-embedded HTML (copied from PAGXHtmlTest.BatchConvertAll output)
#     col 3 — CLI-exported HTML (`pagx export` with multi-source @font-face)
#
#   cli / layout / text / spec — two columns (these samples have no
#   test-embedded equivalent, and comparing PAGX native against the CLI export
#   is the useful thing to see):
#     col 1 — tgfx native render (2x PNG)
#     col 2 — CLI-exported HTML
#
# Pass --without-cli to drop the CLI column on every section (leaves pagx_to_html
# with two columns — native PNG + test-embedded HTML — and the other four
# sections with only the native PNG column).
#
# Output layout (rooted at test/out/html-comparison/):
#
#   html-comparison/
#   ├── index.html
#   ├── pagx-render/<section>/<name>.png
#   ├── test/                   # only for the pagx_to_html section
#   │   ├── <name>.html
#   │   ├── fonts/...
#   │   └── static-img/...
#   └── cli/                    # one subdir per section to avoid name collisions
#       ├── <section>/
#       │   ├── <name>.html
#       │   ├── fonts/...
#       │   └── static-img/...
#
# Subdirectories keep the five sections namespace-isolated because a handful
# of sample names (text_path, text_box, group, repeater, text_modifier) exist
# in both pagx_to_html and spec/samples.
#
# Preconditions the script enforces:
#   - cmake-build-debug/pagx executable must exist (any section)
#   - for the pagx_to_html section's col 2: test/out/PAGXHtmlTest/<name>.html
#     and its sibling fonts/ and static-img/ must exist. Run
#     `PAGFullTest --gtest_filter=PAGXHtmlTest.BatchConvertAll` first.
#     When these are missing the pagx_to_html section simply drops col 2 and
#     renders the remaining columns — other sections are unaffected.
#
# Usage:
#   bash test/gen_html_comparison.sh [--with-cli|--without-cli] [--bold-font <path>]
#
# --with-cli is the default (the script runs `pagx export` on every sample).
# --without-cli skips all CLI work.
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
TEST_OUT="$REPO/test/out/PAGXHtmlTest"
OUT_DIR="$REPO/test/out/html-comparison"

# Section definitions. The "section id" is the short stable name used in
# filesystem paths and DOM ids; the "source dir" is repo-relative; the "title"
# is the display label shown in the tab bar and the section heading.
#
# These entries drive both the bash-side iteration here and the python-side
# index builder (which re-discovers section dirs under pagx-render/ — so the
# two sides don't need to share literal order as long as the set matches).
SECTION_IDS=(pagx_to_html cli layout text spec)
SECTION_SRCDIRS=(
  "resources/pagx_to_html"
  "resources/cli"
  "resources/layout"
  "resources/text"
  "spec/samples"
)
SECTION_TITLES=(
  "pagx_to_html (HTML export focused)"
  "cli (CLI tool samples)"
  "layout (layout engine samples)"
  "text (text-focused samples)"
  "spec (PAGX spec samples)"
)

# Local font files. Regular is bundled in the repo; Emoji and Hebrew are optional
# fallback typefaces `pagx render` needs to avoid substituting system fonts when
# a sample uses those scripts. Bold is also bundled so fauxBold text wraps
# identically to PAGX native (CSS font-synthesis widens glyphs ~6-8%, causing
# early line breaks that don't match tgfx's zero-advance fauxBold).
FONT_REGULAR="$REPO/resources/font/NotoSansSC-Regular.otf"
FONT_BOLD="$REPO/resources/font/NotoSansSC-Bold.ttf"
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

for font in "$FONT_REGULAR" "$FONT_EMOJI" "$FONT_HEBREW"; do
  if [ ! -f "$font" ]; then
    echo "error: bundled font file not found: $font" >&2
    exit 1
  fi
done

# The optional Bold face: prefer the repo-bundled file; fall back to the user's
# Downloads directory; finally fall back to "no local Bold" (CDN Bold via the
# multi-source CSS). When absent, CSS font-synthesis adds ~6-8% advance width,
# causing line-break divergence from PAGX native.
if [ -z "$BOLD_FONT" ]; then
  if [ -f "$FONT_BOLD" ]; then
    BOLD_FONT="$FONT_BOLD"
  else
    DEFAULT_BOLD="$HOME/Downloads/Noto_Sans_SC/static/NotoSansSC-Bold.ttf"
    if [ -f "$DEFAULT_BOLD" ]; then
      BOLD_FONT="$DEFAULT_BOLD"
    fi
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
mkdir -p "$OUT_DIR"

#-------------------------------------------------------------------------
# Shared args
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

# Base --html-font list: Regular local + Regular CDN aggregate into one
# @font-face rule with two sources; Bold CDN is its own rule. If a local
# Bold was found, we additionally list it as the Bold rule's first source.
EXPORT_FONT_ARGS=(
  --html-font "$FONT_REGULAR"
  --html-font "${CDN_REGULAR}#family=Noto Sans SC#weight=400#style=normal"
  --html-font "${CDN_BOLD}#family=Noto Sans SC#weight=700#style=normal"
)
if [ -n "$BOLD_FONT" ]; then
  EXPORT_FONT_ARGS=(
    --html-font "$FONT_REGULAR"
    --html-font "${CDN_REGULAR}#family=Noto Sans SC#weight=400#style=normal"
    --html-font "$BOLD_FONT"
    --html-font "${CDN_BOLD}#family=Noto Sans SC#weight=700#style=normal"
  )
fi

#-------------------------------------------------------------------------
# Per-section processing
#-------------------------------------------------------------------------

total_rendered=0
total_exported=0
total_test_copied=0

for i in "${!SECTION_IDS[@]}"; do
  section="${SECTION_IDS[$i]}"
  srcdir="$REPO/${SECTION_SRCDIRS[$i]}"

  if [ ! -d "$srcdir" ]; then
    echo "warn: section '$section' source dir missing: $srcdir — skipping"
    continue
  fi

  shopt -s nullglob
  pagx_files=( "$srcdir"/*.pagx )
  shopt -u nullglob
  if [ ${#pagx_files[@]} -eq 0 ]; then
    echo "warn: section '$section' has no .pagx files under $srcdir — skipping"
    continue
  fi

  echo ""
  echo "== section: $section (${#pagx_files[@]} samples) =="

  mkdir -p "$OUT_DIR/pagx-render/$section"
  if [ "$WITH_CLI" = "1" ]; then
    mkdir -p "$OUT_DIR/cli/$section"
  fi

  # Column 1: pagx render (2x PNG)
  #
  # Some samples are structurally invalid for rendering on purpose: the cli
  # section includes import_* samples with unresolved <import> directives (they
  # need `pagx resolve` to run first) and verify_not_xml.pagx tests parser error
  # paths. We silently skip failed renders — the Python index builder keys on
  # the rendered PNG directory, so skipped samples simply drop out of the page.
  echo "  rendering col 1 (pagx render --scale 2) ..."
  rendered=0
  skipped=0
  for pagx in "${pagx_files[@]}"; do
    name="$(basename "$pagx" .pagx)"
    if "$PAGX_BIN" render --scale 2 \
        "${RENDER_FONT_ARGS[@]}" \
        -o "$OUT_DIR/pagx-render/$section/$name.png" \
        "$pagx" >/dev/null 2>&1; then
      rendered=$((rendered + 1))
    else
      skipped=$((skipped + 1))
      # Drop any partial PNG so the Python builder doesn't pick up a truncated
      # file. `render` is atomic-write in practice but this is a cheap safety
      # net and makes the "skipped" count authoritative.
      rm -f "$OUT_DIR/pagx-render/$section/$name.png"
    fi
  done
  total_rendered=$((total_rendered + rendered))
  if [ "$skipped" -gt 0 ]; then
    echo "    rendered $rendered PNGs ($skipped sample(s) skipped — non-renderable inputs)"
  else
    echo "    rendered $rendered PNGs"
  fi

  # Column 3 (or 2 for non-pagx_to_html sections): pagx export
  #
  # We only try to export samples whose PNG rendered successfully above; the
  # unresolved-import and verify_not_xml samples would fail here too for the
  # same structural reasons.
  if [ "$WITH_CLI" = "1" ]; then
    echo "  exporting CLI column (pagx export) ..."
    exported=0
    export_skipped=0
    for pagx in "${pagx_files[@]}"; do
      name="$(basename "$pagx" .pagx)"
      if [ ! -f "$OUT_DIR/pagx-render/$section/$name.png" ]; then
        export_skipped=$((export_skipped + 1))
        continue
      fi
      # Capture pagx export output, filter the per-font "overwriting" warnings
      # (which fire once per font file after the first sample since everyone
      # shares fonts/), then decide success by the presence of the HTML file
      # rather than the exit status — the pipe-to-grep pipeline can swallow
      # the `export` exit code in some shells.
      "$PAGX_BIN" export \
          --input "$pagx" \
          --output "$OUT_DIR/cli/$section/$name.html" \
          "${EXPORT_FONT_ARGS[@]}" >/tmp/gen_html_comparison_last_export.log 2>&1 || true
      grep -v "warning: overwriting '$OUT_DIR/cli/$section/fonts/" \
          /tmp/gen_html_comparison_last_export.log \
          > /tmp/gen_html_comparison_last_export.filtered || true
      if [ -f "$OUT_DIR/cli/$section/$name.html" ]; then
        exported=$((exported + 1))
      else
        export_skipped=$((export_skipped + 1))
        echo "    export failed: $pagx"
        sed 's/^/      /' /tmp/gen_html_comparison_last_export.filtered | tail -3
      fi
    done
    total_exported=$((total_exported + exported))
    if [ "$export_skipped" -gt 0 ]; then
      echo "    exported $exported HTMLs ($export_skipped sample(s) skipped)"
    else
      echo "    exported $exported HTMLs"
    fi
  fi
done

#-------------------------------------------------------------------------
# pagx_to_html section also gets col 2 (test-embedded HTML from BatchConvertAll)
#-------------------------------------------------------------------------

if ls "$TEST_OUT"/*.html >/dev/null 2>&1; then
  echo ""
  echo "== copying test-embedded HTMLs for pagx_to_html section =="
  mkdir -p "$OUT_DIR/test"
  for pagx in "$REPO/resources/pagx_to_html"/*.pagx; do
    name="$(basename "$pagx" .pagx)"
    src="$TEST_OUT/$name.html"
    if [ -f "$src" ]; then
      cp "$src" "$OUT_DIR/test/$name.html"
      total_test_copied=$((total_test_copied + 1))
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
  echo "  copied $total_test_copied test-embedded HTMLs"
else
  echo ""
  echo "note: $TEST_OUT has no HTMLs — pagx_to_html section will drop col 2."
  echo "      run: ./cmake-build-debug/PAGFullTest --gtest_filter=PAGXHtmlTest.BatchConvertAll"
  echo "      to populate it, then re-run this script."
fi

#-------------------------------------------------------------------------
# Build index.html
#-------------------------------------------------------------------------

echo ""
echo "Building index.html ..."
if [ "$WITH_CLI" = "1" ]; then
  python3 "$REPO/test/gen_html_comparison.py" "$OUT_DIR" --with-cli
else
  python3 "$REPO/test/gen_html_comparison.py" "$OUT_DIR" --without-cli
fi

INDEX="$OUT_DIR/index.html"
echo ""
echo "Summary: $total_rendered PNGs, $total_exported CLI HTMLs, $total_test_copied test-embedded HTMLs"
echo "Open: $INDEX"
open "$INDEX" 2>/dev/null || true
