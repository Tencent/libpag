#!/usr/bin/env bash

# Run full tests against the selected backend's cache.
# Usage: ./autotest.sh [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL|USE_WEBGL|USE_WEBGPU] [--skip-images]

set -euo pipefail
cd "$(dirname "$0")"
WORKSPACE=$(pwd)
BACKEND_ARG=""
SKIP_IMAGES=0
for arg in "$@"; do
  case "$arg" in
    --skip-images) SKIP_IMAGES=1 ;;
    USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL|USE_WEBGL|USE_WEBGPU)
      if [ -n "$BACKEND_ARG" ]; then
        echo "Error: specify only one backend." >&2
        exit 1
      fi
      BACKEND_ARG="$arg"
      ;;
    -h|--help)
      echo "Usage: $0 [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL|USE_WEBGL|USE_WEBGPU] [--skip-images]"
      exit 0
      ;;
    *) echo "Error: unknown argument '$arg'." >&2; exit 1 ;;
  esac
done

case "$BACKEND_ARG" in
  USE_WEBGL|USE_WEBGPU)
    # Web lanes run through the Emscripten harness in web/test instead of a native binary.
    ./install_tools.sh
    cd web/test
    [ -d node_modules ] || npm install
    BACKEND_LC=$(echo "${BACKEND_ARG#USE_}" | tr '[:upper:]' '[:lower:]')
    npm run "build:${BACKEND_LC}"
    TEST_RESULT=0
    RUN_OUTPUT="$WORKSPACE/result/web-${BACKEND_LC}-$(date +%s)-$$"
    node run_test.js --backend="$BACKEND_LC" --output="$RUN_OUTPUT" || TEST_RESULT=$?
    cd "$WORKSPACE"
    if [ "$TEST_RESULT" -ne 0 ]; then
      echo "pag-test ($BACKEND_LC) failed." >&2
    else
      echo "pag-test ($BACKEND_LC) passed."
    fi
    exit "$TEST_RESULT"
    ;;
esac

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
set +e
"./PAGFullTest_$TARGET_SUFFIX" --gtest_output=json:PAGFullTest.json 2>&1 | \
  tee "$WORKSPACE/result/PAGFullTest_$TARGET_SUFFIX.log"
TEST_STATUS=("${PIPESTATUS[@]}")
set -e
TEST_RESULT=${TEST_STATUS[0]}
COLLECT_RESULT=${TEST_STATUS[1]}
if [ -f "$WORKSPACE/build/PAGFullTest.json" ]; then
  cp -a "$WORKSPACE/build/"*.json "$WORKSPACE/result/" || COLLECT_RESULT=$?
elif [ "$TEST_RESULT" -eq 0 ]; then
  echo "PAGFullTest_$TARGET_SUFFIX did not produce PAGFullTest.json." >&2
  COLLECT_RESULT=1
fi
if [ -d "$WORKSPACE/test/out" ]; then
  cp -R "$WORKSPACE/test/out" "$WORKSPACE/result/" || COLLECT_RESULT=$?
fi
if [ "$TEST_RESULT" -ne 0 ]; then
  echo "PAGFullTest_$TARGET_SUFFIX ($BACKEND_NAME) failed with exit code $TEST_RESULT." >&2
  exit "$TEST_RESULT"
fi
if [ "$COLLECT_RESULT" -ne 0 ]; then
  echo "Collecting PAGFullTest_$TARGET_SUFFIX results failed." >&2
  exit "$COLLECT_RESULT"
fi
echo "PAGFullTest_$TARGET_SUFFIX ($BACKEND_NAME) passed."
