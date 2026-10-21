#!/usr/bin/env bash

# Accept screenshot baseline changes through the /accept-baseline command only.
# Prerequisites: run PAGFullTest_{Backend} and inspect screenshots in the shared test/out/.
# Merge the last run's version metadata, preserve untested keys, and refresh native caches.
# Like tgfx on macOS, acceptance refreshes the native OpenGL cache. Refresh other approved
# backend caches with update_baseline.sh after their version metadata is available on main.

set -euo pipefail
cd "$(dirname "$0")"

if [ ! -f test/out/version.json ]; then
  echo "Error: test/out/version.json not found. Run PAGFullTest_{Backend} first." >&2
  exit 1
fi

echo "Step 1: Merging reviewed test output into the shared version metadata..."
python3 - <<'PY'
import json
from pathlib import Path

baseline_path = Path("test/baseline/version.json")
output_path = Path("test/out/version.json")
with baseline_path.open() as source:
    baseline = json.load(source)
with output_path.open() as source:
    output = json.load(source)


def validate_versions(value):
    if not isinstance(value, dict):
        raise ValueError("Version metadata must be an object")
    for item in value.values():
        if isinstance(item, dict):
            validate_versions(item)
        elif not isinstance(item, str) or not item:
            raise ValueError("Baseline versions must be nonempty strings")


def merge_versions(target, changes):
    for key, value in changes.items():
        if isinstance(value, dict):
            if key not in target:
                target[key] = {}
            if not isinstance(target[key], dict):
                raise ValueError("Conflicting version metadata at " + key)
            merge_versions(target[key], value)
        else:
            if isinstance(target.get(key), dict):
                raise ValueError("Conflicting version metadata at " + key)
            target[key] = value


validate_versions(baseline)
validate_versions(output)
if not output:
    raise ValueError("No screenshot versions to accept")
merge_versions(baseline, output)
with baseline_path.open("w") as destination:
    json.dump(baseline, destination, indent=4, sort_keys=True)
    destination.write("\n")
PY

echo "Step 2: Refreshing the native OpenGL baseline cache..."
BUILD_DIR=cmake-build-accept-baseline-OpenGL
rm -rf "$BUILD_DIR"
cmake -G Ninja -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=OFF \
      -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DCMAKE_BUILD_TYPE=Debug -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target UpdateBaseline_OpenGL
"./$BUILD_DIR/UpdateBaseline_OpenGL"

echo "Baseline changes accepted. Commit test/baseline/version.json."
