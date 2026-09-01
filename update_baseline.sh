#!/usr/bin/env bash

# Update local baseline cache from remote changes.
# Run this after pulling main branch that contains baseline changes from others.
# Without updating the cache, affected tests will be skipped, leading to inaccurate results.
#
# Usage:
#   ./update_baseline.sh [<BACKEND>] [--skip-images]
#
# BACKEND (order-insensitive with --skip-images):
#   USE_OPENGL                 OpenGL (real GPU, default)
#   USE_OPENGL_SWIFTSHADER     OpenGL (SwiftShader software renderer)
#   USE_METAL                  Metal (Apple GPU, macOS/iOS)
#   USE_VULKAN                 Vulkan (real GPU)
#   USE_D3D12                  D3D12 (Windows)
#
# Options:
#   --skip-images   Skip writing baseline `_base.webp` images. The cache
#                   (md5.json + version.json) is still refreshed, which is
#                   all the CI pipelines need. Local runs omit this to also
#                   produce baseline-out/ images for visual inspection.

{
  # Parse arguments: separate the backend keyword from the --skip-images flag so callers may
  # pass them in any order (e.g. `./update_baseline.sh --skip-images USE_METAL`).
  BACKEND_ARG=""
  SKIP_IMAGES=0
  for arg in "$@"; do
    case "$arg" in
      --skip-images) SKIP_IMAGES=1 ;;
      *) BACKEND_ARG="$arg" ;;
    esac
  done

  # Determine cmake args, backend name, and target suffix.
  case "$BACKEND_ARG" in
    USE_OPENGL_SWIFTSHADER)
      CMAKE_BACKEND_ARGS="-DPAG_USE_SWIFTSHADER=ON"
      BACKEND_NAME="opengl-swiftshader"
      TARGET_SUFFIX="OpenGL" ;;
    USE_METAL)
      CMAKE_BACKEND_ARGS="-DPAG_USE_METAL=ON -DPAG_USE_OPENGL=OFF"
      BACKEND_NAME="metal"
      TARGET_SUFFIX="Metal" ;;
    USE_VULKAN)
      CMAKE_BACKEND_ARGS="-DPAG_USE_VULKAN=ON -DPAG_USE_OPENGL=OFF"
      BACKEND_NAME="vulkan"
      TARGET_SUFFIX="Vulkan" ;;
    USE_D3D12)
      CMAKE_BACKEND_ARGS="-DPAG_USE_D3D12=ON -DPAG_USE_OPENGL=OFF"
      BACKEND_NAME="d3d12"
      TARGET_SUFFIX="D3D12" ;;
    USE_OPENGL|"")
      CMAKE_BACKEND_ARGS=""
      BACKEND_NAME="opengl"
      TARGET_SUFFIX="OpenGL" ;;
    *)
      echo "Error: unknown BACKEND '$BACKEND_ARG'."
      echo "Supported: USE_OPENGL, USE_OPENGL_SWIFTSHADER, USE_METAL, USE_VULKAN, USE_D3D12."
      exit 1 ;;
  esac

  CACHE_VERSION_FILE=./test/baseline/.cache/$BACKEND_NAME/version.json

  # Check if cache is up to date with origin/main.
  if [ -f "$CACHE_VERSION_FILE" ]; then
    MAIN_VERSION=$(git show origin/main:test/baseline/version.json 2>/dev/null)
    if [ -n "$MAIN_VERSION" ]; then
      CACHE_CONTENT=$(cat "$CACHE_VERSION_FILE")
      if [ "$MAIN_VERSION" = "$CACHE_CONTENT" ]; then
        exit 0
      fi
    fi
  fi

  echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND_NAME) Start~~~~~~~~~~~~~~~~~~~~~"
  CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  CURRENT_COMMIT=$(git rev-parse HEAD)
  BUILD_DIR=build-update-baseline
  # Remove build artifacts before stash to avoid "already exists" conflicts on pop.
  rm -rf ${BUILD_DIR}
  STASH_LIST_BEFORE=$(git stash list)
  git stash push --include-untracked --quiet
  STASH_LIST_AFTER=$(git stash list)
  git switch main --quiet

  ./install_tools.sh
  depsync

  mkdir ${BUILD_DIR}
  cd ${BUILD_DIR}

  # Default to generating baseline images so local users get baseline-out/ for inspection.
  # CI pipelines pass --skip-images because they only need the cache (md5.json + version.json).
  SKIP_IMAGES_FLAG=""
  if [ "$SKIP_IMAGES" = "1" ]; then
    SKIP_IMAGES_FLAG="-DPAG_SKIP_GENERATE_BASELINE_IMAGES=ON"
  fi

  cmake -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage -g -O0" $CMAKE_BACKEND_ARGS \
        $SKIP_IMAGES_FLAG \
        -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON \
        -DCMAKE_BUILD_TYPE=Debug ../

  cmake --build . --target UpdateBaseline_${TARGET_SUFFIX} -- -j 12
  ./UpdateBaseline_${TARGET_SUFFIX}

  if test $? -eq 0; then
     echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND_NAME) Success~~~~~~~~~~~~~~~~~~~~~"
  else
    echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND_NAME) Failed~~~~~~~~~~~~~~~~~~"
    COMPLIE_RESULT=false
  fi

  cd ..

  if [[ $CURRENT_BRANCH == "HEAD" ]]; then
      git checkout $CURRENT_COMMIT --quiet
  else
      git switch $CURRENT_BRANCH --quiet
  fi
  if [[ $STASH_LIST_BEFORE != "$STASH_LIST_AFTER" ]]; then
    git stash pop --index --quiet
  fi

  depsync

  if [ "$COMPLIE_RESULT" == false ]; then
    mkdir -p result
    # Copy test output for CI diagnostic upload (if it exists).
    # UpdateBaseline may not produce test/out/ — it writes to .cache/ instead.
    if [ -d test/out ]; then
      cp -r test/out result
    fi
    exit 1
  fi
  rm -rf ${BUILD_DIR}
}
