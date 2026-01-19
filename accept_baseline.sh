#!/bin/bash

# Accept screenshot baseline changes.
# Run this when adding new screenshot tests or refactoring rendering with expected output changes.
# WARNING: Only run after confirming ALL screenshots in test/out/ match expected results.

set -e

echo "Step 1: Building PAGFullTest..."
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest

echo "Step 2: Running PAGFullTest..."
./cmake-build-debug/PAGFullTest || true

echo "Step 3: Copying version.json to baseline..."
cp test/out/version.json test/baseline/

echo "Step 4: Building and running UpdateBaseline..."
cmake --build cmake-build-debug --target UpdateBaseline
./cmake-build-debug/UpdateBaseline

echo "Baseline changes accepted successfully!"
