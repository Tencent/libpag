#!/usr/bin/env bash
# Builds a side-by-side rendering comparison page that covers every PAGX sample
# available in the repo. The samples live in five separate directories, each
# with a different focus (see "Section layout" below). The page is organised as
# a section per source directory with a sticky top tab bar for quick navigation.
#
# Section layout and column count:
#
#   pagx_to_html  — three columns (all PNG screenshots):
#     col 1 — tgfx native render (2x PNG from `pagx render`)
#     col 2 — test-embedded HTML screenshot (Puppeteer capture of BatchConvertAll output)
#     col 3 — CLI-exported HTML screenshot (Puppeteer capture of `pagx export` output)
#
#   cli / layout / text / spec — two columns:
#     col 1 — tgfx native render (2x PNG)
#     col 2 — CLI-exported HTML screenshot
#
# Pass --without-cli to drop the CLI column on every section (leaves pagx_to_html
# with two columns — native PNG + test-embedded HTML screenshot — and the other
# four sections with only the native PNG column).
#
# Output layout (rooted at test/out/html-comparison/):
#
#   html-comparison/
#   ├── index.html
#   ├── fonts/                  # shared by every HTML below
#   ├── pagx-render/<section>/<name>.png
#   ├── test/                   # only for the pagx_to_html section
#   │   ├── <name>.html
#   │   └── <name>/             # sibling assets dir (rasterized PNGs) per HTML
#   ├── test-screenshots/<name>.png         # Puppeteer capture of test/ HTMLs
#   ├── cli/                    # one subdir per section to avoid name collisions
#   │   ├── <section>/
#   │   │   ├── <name>.html
#   │   │   └── <name>/         # sibling assets dir per HTML
#   └── cli-screenshots/<section>/<name>.png  # Puppeteer capture of cli/ HTMLs
#
# Preconditions the script enforces:
#   - cmake-build-debug/pagx executable must exist (any section)
#   - for the pagx_to_html section's col 2: test/out/PAGXHtmlTest/<name>.html
#     must exist. Run
#     `PAGFullTest --gtest_filter=PAGXHtmlTest.BatchConvertAll` first.
#     When these are missing the pagx_to_html section simply drops col 2.
#
# Usage:
#   bash test/gen_html_comparison.sh [--with-cli|--without-cli]

set -euo pipefail

#-------------------------------------------------------------------------
# Argument parsing
#-------------------------------------------------------------------------

WITH_CLI=1

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
SCREENSHOT_JS="$REPO/test/screenshot.js"

# Section definitions.
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

#-------------------------------------------------------------------------
# Font files for pagx render
#-------------------------------------------------------------------------

# System Arial fonts — register Regular, Bold, Italic, Bold Italic so that
# pagx render uses the real typeface files (matched by fontFamily + fontStyle)
# instead of relying on tgfx's MakeFromName system font lookup.
ARIAL_REGULAR="/System/Library/Fonts/Supplemental/Arial.ttf"
ARIAL_BOLD="/System/Library/Fonts/Supplemental/Arial Bold.ttf"
ARIAL_ITALIC="/System/Library/Fonts/Supplemental/Arial Italic.ttf"
ARIAL_BOLD_ITALIC="/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf"

# Repo-bundled fallback fonts for non-Latin scripts.
FONT_REGULAR="$REPO/resources/font/NotoSansSC-Regular.otf"
FONT_EMOJI="$REPO/resources/font/NotoColorEmoji.ttf"
FONT_HEBREW="$REPO/resources/font/NotoSansHebrew-Regular.ttf"

# Build the --font / --fallback arg list.
RENDER_FONT_ARGS=()
for f in "$ARIAL_REGULAR" "$ARIAL_BOLD" "$ARIAL_ITALIC" "$ARIAL_BOLD_ITALIC"; do
  if [ -f "$f" ]; then
    RENDER_FONT_ARGS+=( --font "$f" )
  fi
done
for f in "$FONT_REGULAR" "$FONT_EMOJI" "$FONT_HEBREW"; do
  if [ -f "$f" ]; then
    RENDER_FONT_ARGS+=( --fallback "$f" )
  fi
done

#-------------------------------------------------------------------------
# Preconditions
#-------------------------------------------------------------------------

if [ ! -x "$PAGX_BIN" ]; then
  echo "error: pagx binary not found at $PAGX_BIN" >&2
  echo "       build it first: cmake --build cmake-build-debug --target pagx" >&2
  exit 1
fi

#-------------------------------------------------------------------------
# Clean output
#-------------------------------------------------------------------------

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

#-------------------------------------------------------------------------
# Helper: extract width and height from a .pagx file
#-------------------------------------------------------------------------

pagx_dims() {
  python3 -c "
import re, sys
t = open(sys.argv[1]).read()
m = re.search(r'<pagx[^>]*\bwidth=\"(\d+)\"[^>]*\bheight=\"(\d+)\"', t)
if not m: sys.exit(1)
print(m.group(1), m.group(2))
" "$1"
}

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

  # Column 1: pagx render (2x PNG) with registered Arial + fallback fonts.
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
      rm -f "$OUT_DIR/pagx-render/$section/$name.png"
    fi
  done
  total_rendered=$((total_rendered + rendered))
  if [ "$skipped" -gt 0 ]; then
    echo "    rendered $rendered PNGs ($skipped sample(s) skipped — non-renderable inputs)"
  else
    echo "    rendered $rendered PNGs"
  fi

  # CLI column: pagx export
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
      "$PAGX_BIN" export \
          --input "$pagx" \
          --output "$OUT_DIR/cli/$section/$name.html" \
          >/tmp/gen_html_comparison_last_export.log 2>&1 || true
      grep -v "warning:" \
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
      if [ -d "$TEST_OUT/$name" ]; then
        cp -R "$TEST_OUT/$name" "$OUT_DIR/test/$name"
      fi
      total_test_copied=$((total_test_copied + 1))
    fi
  done
  echo "  copied $total_test_copied test-embedded HTMLs"
else
  echo ""
  echo "note: $TEST_OUT has no HTMLs — pagx_to_html section will drop col 2."
  echo "      run: ./cmake-build-debug/PAGFullTest --gtest_filter=PAGXHtmlTest.BatchConvertAll"
  echo "      to populate it, then re-run this script."
fi

#-------------------------------------------------------------------------
# Consolidate per-section fonts/ directories into a single top-level fonts/
#-------------------------------------------------------------------------

echo ""
echo "== consolidating per-section fonts/ into top-level fonts/ =="
GLOBAL_FONTS_DIR="$OUT_DIR/fonts"
mkdir -p "$GLOBAL_FONTS_DIR"

for dir in "$OUT_DIR"/cli/*/fonts "$OUT_DIR"/test/fonts; do
  if [ -d "$dir" ]; then
    cp -R -n "$dir"/* "$GLOBAL_FONTS_DIR"/ 2>/dev/null || true
  fi
done

if [ "$(uname)" = "Darwin" ]; then
  SED_INPLACE=(-i '')
else
  SED_INPLACE=(-i)
fi
find "$OUT_DIR"/cli -maxdepth 2 -name "*.html" -type f -exec \
  sed "${SED_INPLACE[@]}" "s|url('fonts/|url('../../fonts/|g" {} +
if [ -d "$OUT_DIR"/test ]; then
  find "$OUT_DIR"/test -maxdepth 1 -name "*.html" -type f -exec \
    sed "${SED_INPLACE[@]}" "s|url('fonts/|url('../fonts/|g" {} +
fi

rm -rf "$OUT_DIR"/cli/*/fonts "$OUT_DIR"/test/fonts

font_count=$(ls "$GLOBAL_FONTS_DIR" 2>/dev/null | wc -l | tr -d ' ')
font_size=$(du -sh "$GLOBAL_FONTS_DIR" 2>/dev/null | cut -f1)
echo "  shared fonts/ contains $font_count file(s), $font_size"

#-------------------------------------------------------------------------
# Screenshot HTML columns via Puppeteer
#-------------------------------------------------------------------------

echo ""
echo "== screenshotting HTML columns via Puppeteer =="

TASKS_JSON="$OUT_DIR/.screenshot_tasks.json"
task_count=0

# Start JSON array
printf '[\n' > "$TASKS_JSON"

# Test-embedded screenshots (pagx_to_html section only)
if [ -d "$OUT_DIR/test" ]; then
  mkdir -p "$OUT_DIR/test-screenshots"
  shopt -s nullglob
  test_htmls=( "$OUT_DIR"/test/*.html )
  shopt -u nullglob
  for html_file in "${test_htmls[@]}"; do
    name="$(basename "$html_file" .html)"
    pagx="$REPO/resources/pagx_to_html/$name.pagx"
    if [ ! -f "$pagx" ]; then continue; fi
    dims=$(pagx_dims "$pagx" 2>/dev/null) || continue
    read -r w h <<< "$dims"
    [ "$task_count" -gt 0 ] && printf ',\n' >> "$TASKS_JSON"
    printf '{"html":"%s","png":"%s","width":%s,"height":%s,"scale":2}' \
      "$html_file" "$OUT_DIR/test-screenshots/$name.png" "$w" "$h" >> "$TASKS_JSON"
    task_count=$((task_count + 1))
  done
fi

# CLI screenshots (all sections)
if [ "$WITH_CLI" = "1" ]; then
  for i in "${!SECTION_IDS[@]}"; do
    section="${SECTION_IDS[$i]}"
    cli_section_dir="$OUT_DIR/cli/$section"
    if [ ! -d "$cli_section_dir" ]; then continue; fi
    srcdir="$REPO/${SECTION_SRCDIRS[$i]}"
    mkdir -p "$OUT_DIR/cli-screenshots/$section"
    shopt -s nullglob
    cli_htmls=( "$cli_section_dir"/*.html )
    shopt -u nullglob
    for html_file in "${cli_htmls[@]}"; do
      name="$(basename "$html_file" .html)"
      pagx="$srcdir/$name.pagx"
      if [ ! -f "$pagx" ]; then continue; fi
      dims=$(pagx_dims "$pagx" 2>/dev/null) || continue
      read -r w h <<< "$dims"
      [ "$task_count" -gt 0 ] && printf ',\n' >> "$TASKS_JSON"
      printf '{"html":"%s","png":"%s","width":%s,"height":%s,"scale":2}' \
        "$html_file" "$OUT_DIR/cli-screenshots/$section/$name.png" "$w" "$h" >> "$TASKS_JSON"
      task_count=$((task_count + 1))
    done
  done
fi

printf '\n]\n' >> "$TASKS_JSON"

if [ "$task_count" -gt 0 ]; then
  echo "  $task_count HTML screenshots queued"
  node "$SCREENSHOT_JS" --batch "$TASKS_JSON"
  # Count successful screenshots
  test_ss=$(find "$OUT_DIR/test-screenshots" -name "*.png" 2>/dev/null | wc -l | tr -d ' ')
  cli_ss=$(find "$OUT_DIR/cli-screenshots" -name "*.png" 2>/dev/null | wc -l | tr -d ' ')
  echo "  captured $test_ss test-embedded + $cli_ss CLI screenshots"
else
  echo "  no HTML files to screenshot"
fi

rm -f "$TASKS_JSON"

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
