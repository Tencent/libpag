#!/usr/bin/env bash

# Run full tests against the selected backend's cache.
# Usage: ./autotest.sh [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL] [--skip-images]

set -euo pipefail
cd "$(dirname "$0")"
WORKSPACE=$(pwd)
BACKEND_ARG=""
for arg in "$@"; do
  case "$arg" in
    --skip-images) ;;
    USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL)
      if [ -n "$BACKEND_ARG" ]; then
        echo "Error: specify only one backend." >&2
        exit 1
      fi
      BACKEND_ARG="$arg"
      ;;
    -h|--help)
      echo "Usage: $0 [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL] [--skip-images]"
      exit 0
      ;;
    *) echo "Error: unknown argument '$arg'." >&2; exit 1 ;;
  esac
done

CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=OFF)
case "$BACKEND_ARG" in
  USE_METAL)
    CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=ON -DPAG_USE_OPENGL=OFF -DPAG_USE_SWIFTSHADER=OFF)
    BACKEND_NAME=metal
    TARGET_SUFFIX=Metal
    ;;
  USE_OPENGL_SWIFTSHADER)
    CMAKE_BACKEND_ARGS=(-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=ON)
    BACKEND_NAME=opengl-swiftshader
    TARGET_SUFFIX=OpenGL
    ;;
  USE_OPENGL|"")
    BACKEND_NAME=opengl
    TARGET_SUFFIX=OpenGL
    ;;
esac

./install_tools.sh
./update_baseline.sh "$@"
rm -rf "$WORKSPACE/result" "$WORKSPACE/build"
mkdir -p "$WORKSPACE/result" "$WORKSPACE/build"
cp -R "$WORKSPACE/test/baseline" "$WORKSPACE/result/"
cmake -S "$WORKSPACE" -B "$WORKSPACE/build" "${CMAKE_BACKEND_ARGS[@]}" \
      -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage -g -O0" \
      -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$WORKSPACE/build" --target "PAGFullTest_$TARGET_SUFFIX" -- -j 12

cd "$WORKSPACE/build"
TEST_RESULT=0
"./PAGFullTest_$TARGET_SUFFIX" --gtest_output=json:PAGFullTest.json || TEST_RESULT=$?
cp -a "$WORKSPACE/build/"*.json "$WORKSPACE/result/"
if [ -d "$WORKSPACE/test/out" ]; then
  cp -R "$WORKSPACE/test/out" "$WORKSPACE/result/"
fi
if [ "$TEST_RESULT" -ne 0 ]; then
  echo "PAGFullTest_$TARGET_SUFFIX ($BACKEND_NAME) failed." >&2
  exit "$TEST_RESULT"
fi
echo "PAGFullTest_$TARGET_SUFFIX ($BACKEND_NAME) passed."
