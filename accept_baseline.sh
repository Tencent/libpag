#!/bin/bash

# Accept screenshot baseline changes through the /accept-baseline command only.
# Run this when adding new screenshot tests or refactoring rendering with expected output changes.
# WARNING: Only run after confirming ALL screenshots in test/out/ match expected results.
# Usage: ./accept_baseline.sh [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL]

set -e
cd "$(dirname "$0")"

if [ "$#" -gt 1 ]; then
  echo "Error: specify only one backend." >&2
  exit 1
fi

CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=OFF)
case "${1:-USE_OPENGL}" in
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
    echo "Usage: $0 [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL]"
    exit 0
    ;;
  *) echo "Error: unknown backend '$1'." >&2; exit 1 ;;
esac

BUILD_DIR=cmake-build-debug

echo "Step 1: Building PAGFullTest_$TARGET_SUFFIX..."
cmake -G Ninja "${CMAKE_BACKEND_ARGS[@]}" -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target "PAGFullTest_$TARGET_SUFFIX"

echo "Step 2: Running PAGFullTest_$TARGET_SUFFIX..."
"./$BUILD_DIR/PAGFullTest_$TARGET_SUFFIX" || true

echo "Step 3: Copying version.json to baseline..."
cp test/out/version.json test/baseline/

echo "Step 4: Building and running UpdateBaseline_$TARGET_SUFFIX..."
cmake --build "$BUILD_DIR" --target "UpdateBaseline_$TARGET_SUFFIX"
"./$BUILD_DIR/UpdateBaseline_$TARGET_SUFFIX"

echo "Baseline changes accepted successfully!"
