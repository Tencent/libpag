#!/bin/bash

# Accept screenshot baseline changes.
# Run this when adding new screenshot tests or refactoring rendering with expected output changes.
# WARNING: Only run after confirming ALL screenshots in test/out/ match expected results.
#
# This script automatically runs UpdateBaseline for all backends available on the current
# platform. Per-backend caches live under test/baseline/.cache/<backend>/ (aligned with tgfx),
# so each backend refreshes its own md5/version snapshots without clobbering the others.
#
# Platform coverage:
#   macOS:   OpenGL (+ Metal once the Metal backend is fully supported)
#   Linux:   OpenGL
#   Windows: OpenGL (+ Vulkan / D3D12 in future)
#
# No arguments required.

set -e
cd "$(dirname "$0")"

if [ ! -f "test/out/version.json" ]; then
  echo "Error: test/out/version.json not found."
  echo "Please run PAGFullTest_<Backend> first, confirm the screenshots, then run this script."
  exit 1
fi

echo "Step 1: Merging version.json (update existing keys, add new keys, preserve others)..."

# Merge test/out/version.json INTO test/baseline/version.json:
#   - Keys present in out: update with new value.
#   - Keys only in baseline: preserve as-is.
#   - Keys only in out: add to baseline.
# This makes the "accept" idempotent across multi-backend runs — accepting after a GL run
# never drops keys that only a Metal / Vulkan run would emit.
python3 -c "
import json

baseline_path = 'test/baseline/version.json'
out_path = 'test/out/version.json'

with open(baseline_path, 'r') as f:
    baseline = json.load(f)
with open(out_path, 'r') as f:
    out = json.load(f)

# Deep merge: out overwrites baseline at the key level.
for category, keys in out.items():
    if category not in baseline:
        baseline[category] = {}
    for key, value in keys.items():
        baseline[category][key] = value

# Sort for deterministic output.
sorted_baseline = {}
for category in sorted(baseline.keys()):
    sorted_baseline[category] = dict(sorted(baseline[category].items()))

with open(baseline_path, 'w') as f:
    json.dump(sorted_baseline, f, indent=4)
    f.write('\n')

print(f'  Merged {sum(len(v) for v in out.values())} keys from out into baseline.')
print(f'  Baseline now has {sum(len(v) for v in sorted_baseline.values())} total keys.')
"

# Determine which backends to refresh based on current platform.
# Format: "TargetSuffix:CMakeArgs"; leave CMakeArgs empty for the default backend.
OS=$(uname -s)
case "$OS" in
  Darwin)
    # macOS supports both OpenGL and Metal via the pag::Devices glue layer. Each backend keeps
    # its own .cache/<backend>/ so refreshing both here does not conflict.
    BACKENDS=("OpenGL:" "Metal:-DPAG_USE_METAL=ON -DPAG_USE_OPENGL=OFF")
    ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT)
    BACKENDS=("OpenGL:")
    ;;
  *)
    BACKENDS=("OpenGL:")
    ;;
esac

echo "Step 2: Refreshing .cache for all local backends..."

for entry in "${BACKENDS[@]}"; do
  TARGET_SUFFIX="${entry%%:*}"
  CMAKE_ARGS="${entry#*:}"
  BUILD_DIR="cmake-build-accept-baseline-${TARGET_SUFFIX}"

  echo ""
  echo "--- UpdateBaseline_${TARGET_SUFFIX} (${CMAKE_ARGS:-default}) ---"

  # Clean stale build directory from previous interrupted runs.
  rm -rf "$BUILD_DIR"

  cmake -G Ninja $CMAKE_ARGS -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON \
        -DCMAKE_BUILD_TYPE=Debug -B "$BUILD_DIR"
  cmake --build "$BUILD_DIR" --target UpdateBaseline_${TARGET_SUFFIX}
  ./"$BUILD_DIR"/UpdateBaseline_${TARGET_SUFFIX}

  echo "--- UpdateBaseline_${TARGET_SUFFIX} done ---"
done

echo ""
echo "Baseline accepted for all local backends. Commit:"
echo "  git add test/baseline/version.json && git commit -m 'Update baseline.'"
