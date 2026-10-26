#!/bin/bash

# Accept screenshot baseline changes through the /accept-baseline command only.
# Run this when adding new screenshot tests or refactoring rendering with expected output changes.
# WARNING: Only run after confirming ALL screenshots in test/out/ match expected results.
# Usage: ./accept_baseline.sh <USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL>

set -e
cd "$(dirname "$0")"

if [ "$#" -ne 1 ]; then
  echo "Error: specify exactly one backend." >&2
  echo "Usage: $0 <USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL>" >&2
  exit 1
fi

CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=OFF)
case "$1" in
  USE_METAL)
    CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=ON -DPAG_USE_OPENGL=OFF -DPAG_USE_SWIFTSHADER=OFF)
    TARGET_SUFFIX=Metal
    ;;
  USE_OPENGL_SWIFTSHADER)
    CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=ON)
    TARGET_SUFFIX=OpenGL
    ;;
  USE_OPENGL)
    TARGET_SUFFIX=OpenGL
    ;;
  -h|--help)
    echo "Usage: $0 <USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL>"
    exit 0
    ;;
  *) echo "Error: unknown backend '$1'." >&2; exit 1 ;;
esac

if [ "${GTEST_TOTAL_SHARDS+x}" = x ] || [ "${GTEST_SHARD_INDEX+x}" = x ]; then
  echo "Error: baseline acceptance requires an unsharded test run." >&2
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "Error: python3 is required to validate and merge baseline versions." >&2
  exit 1
fi

BUILD_DIR=cmake-build-debug
TEST_ARGS=(--gtest_filter='*' --gtest_repeat=1 --gtest_fail_fast=0
           --gtest_break_on_failure=0 --gtest_throw_on_failure=0)

echo "Step 1: Building PAGFullTest_$TARGET_SUFFIX..."
cmake -G Ninja "${CMAKE_BACKEND_ARGS[@]}" -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target "PAGFullTest_$TARGET_SUFFIX"

TEMP_DIR=$(mktemp -d "$BUILD_DIR/accept-baseline.XXXXXX")
BASELINE_CHANGED=0
ACCEPTANCE_COMPLETE=0

FinishAcceptance() {
  local status=$?
  trap - EXIT
  if [ "$BASELINE_CHANGED" -eq 1 ] && [ "$ACCEPTANCE_COMPLETE" -eq 0 ]; then
    if ! cp "$TEMP_DIR/previous-ledger.json" test/baseline/version.json; then
      echo "Error: could not restore baseline versions. Backup: $TEMP_DIR/previous-ledger.json" >&2
      exit 1
    fi
  fi
  rm -rf "$TEMP_DIR"
  exit "$status"
}

trap FinishAcceptance EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
cp test/baseline/version.json "$TEMP_DIR/previous-ledger.json"

# A crash must not allow an older run's version output to be accepted.
rm -f test/out/version.json
echo "Step 2: Running the complete PAGFullTest_$TARGET_SUFFIX suite..."
FULL_TEST_RESULT=0
"./$BUILD_DIR/PAGFullTest_$TARGET_SUFFIX" "${TEST_ARGS[@]}" || FULL_TEST_RESULT=$?
if [ "$FULL_TEST_RESULT" -ne 0 ] && [ "$FULL_TEST_RESULT" -ne 1 ]; then
  echo "Error: FullTest terminated abnormally (exit $FULL_TEST_RESULT). Baseline versions are unchanged." >&2
  exit 1
fi

echo "Step 3: Validating test output and merging baseline versions..."
python3 - "$TEMP_DIR/previous-ledger.json" test/out/version.json \
  "$TEMP_DIR/merged-ledger.json" <<'PY'
import json
import sys
from pathlib import Path


def validate_versions(value):
    if not isinstance(value, dict):
        raise ValueError("Version metadata must be an object")
    count = 0
    for item in value.values():
        if isinstance(item, dict):
            count += validate_versions(item)
        elif isinstance(item, str) and item:
            count += 1
        else:
            raise ValueError("Baseline versions must be nonempty strings")
    return count


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


baseline_path, output_path, merged_path = sys.argv[1:]
with Path(baseline_path).open() as source:
    baseline = json.load(source)
with Path(output_path).open() as source:
    output = json.load(source)
validate_versions(baseline)
if validate_versions(output) == 0:
    raise ValueError("No screenshot versions to accept")
merge_versions(baseline, output)
with Path(merged_path).open("w") as destination:
    json.dump(baseline, destination, indent=4, sort_keys=True)
    destination.write("\n")
PY

BASELINE_CHANGED=1
mv "$TEMP_DIR/merged-ledger.json" test/baseline/version.json

echo "Step 4: Building and running UpdateBaseline_$TARGET_SUFFIX..."
cmake --build "$BUILD_DIR" --target "UpdateBaseline_$TARGET_SUFFIX"
"./$BUILD_DIR/UpdateBaseline_$TARGET_SUFFIX" "${TEST_ARGS[@]}"
ACCEPTANCE_COMPLETE=1

echo "Baseline changes accepted successfully!"
