#!/usr/bin/env bash
#
# End-to-end driver for the visual comparison report. Renders every PAGX
# fixture twice (PathA via LayerBuilder, PathB via Inflater), writes
# webp outputs + status sidecars under test/out/comparison/, then
# generates test/out/render_compare.html.
#
# Stages:
#   1. cmake --build <build-dir> --target PAGFullTest   (skip with --no-build)
#   2. PAGFullTest ComparisonDumpTest.DISABLED_DumpAllSections   (skip with --no-render)
#   3. python3 tools/render_compare.py
#
# Common usage:
#   tools/gen_pagx_pag_compare.sh                     # full pipeline
#   tools/gen_pagx_pag_compare.sh --no-build          # skip rebuild, use existing PAGFullTest
#   tools/gen_pagx_pag_compare.sh --no-render         # skip rendering, just regenerate HTML
#                                                     # from existing webp outputs
#   BUILD_DIR=cmake-build-debuglocal tools/gen_pagx_pag_compare.sh   # use a different build dir

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-cmake-build-debug}"
DO_BUILD=1
DO_RENDER=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      DO_BUILD=0
      shift
      ;;
    --no-render)
      DO_RENDER=0
      shift
      ;;
    -h|--help)
      sed -n '3,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//;s/^#$//'
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      echo "see --help" >&2
      exit 2
      ;;
  esac
done

cd "$REPO_ROOT"

if [[ "$DO_BUILD" == "1" ]]; then
  echo "==> Building PAGFullTest in $BUILD_DIR"
  cmake --build "$BUILD_DIR" --target PAGFullTest
fi

if [[ "$DO_RENDER" == "1" ]]; then
  TEST_BIN="$BUILD_DIR/PAGFullTest"
  if [[ ! -x "$TEST_BIN" ]]; then
    echo "PAGFullTest not found at $TEST_BIN" >&2
    echo "Run without --no-build first, or set BUILD_DIR to your build directory." >&2
    exit 1
  fi
  echo "==> Rendering 279 sample pairs via ComparisonDumpTest"
  "$TEST_BIN" \
    --gtest_also_run_disabled_tests \
    --gtest_filter='ComparisonDumpTest.DISABLED_DumpAllSections'
fi

echo "==> Generating render_compare.html"
python3 tools/render_compare.py
