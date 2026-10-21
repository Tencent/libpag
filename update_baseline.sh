#!/usr/bin/env bash

# Refresh the selected backend's local baseline cache from main.
# Usage: ./update_baseline.sh [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL] [--skip-images]
# Screenshot outputs and version metadata remain shared, so run backends sequentially.

set -euo pipefail
cd "$(dirname "$0")"
WORKSPACE=$(pwd)
BACKEND_ARG=""
SKIP_IMAGES=0
for arg in "$@"; do
  case "$arg" in
    --skip-images) SKIP_IMAGES=1 ;;
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

CACHE_DIR="$WORKSPACE/test/baseline/.cache/$BACKEND_NAME"
if [ -f "$CACHE_DIR/version.json" ] && [ -f "$CACHE_DIR/md5.json" ]; then
  MAIN_VERSION=$(git show origin/main:test/baseline/version.json)
  CACHE_VERSION=$(cat "$CACHE_DIR/version.json")
  if [ "$MAIN_VERSION" = "$CACHE_VERSION" ]; then
    echo "Baseline cache for '$BACKEND_NAME' is up to date."
    exit 0
  fi
fi

BUILD_DIR="$WORKSPACE/build-update-baseline"
rm -rf "$BUILD_DIR"
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
CURRENT_COMMIT=$(git rev-parse HEAD)
STASH_BEFORE=$(git rev-parse --verify -q refs/stash || true)
STASH_CREATED=0
WORKSPACE_RESTORED=0

RestoreWorkspace() {
  if [ "$WORKSPACE_RESTORED" -eq 1 ]; then
    return 0
  fi
  cd "$WORKSPACE"
  if [ "$CURRENT_BRANCH" = HEAD ]; then
    git checkout "$CURRENT_COMMIT" --quiet || return 1
  else
    git switch "$CURRENT_BRANCH" --quiet || return 1
  fi
  if [ "$STASH_CREATED" -eq 1 ]; then
    git stash pop --index --quiet || return 1
    STASH_CREATED=0
  fi
  WORKSPACE_RESTORED=1
}

RestoreOnExit() {
  local status=$?
  trap - EXIT
  if ! RestoreWorkspace; then
    echo "Error: could not restore the original workspace. Keep the stash for recovery." >&2
    exit 1
  fi
  exit "$status"
}

trap RestoreOnExit EXIT
git stash push --include-untracked --quiet
STASH_AFTER=$(git rev-parse --verify -q refs/stash || true)
if [ "$STASH_BEFORE" != "$STASH_AFTER" ]; then
  STASH_CREATED=1
fi
git switch main --quiet
./install_tools.sh
depsync

mkdir -p "$BUILD_DIR"
CMAKE_IMAGE_ARGS=(-DPAG_SKIP_GENERATE_BASELINE_IMAGES=OFF)
if [ "$SKIP_IMAGES" -eq 1 ]; then
  CMAKE_IMAGE_ARGS=(-DPAG_SKIP_GENERATE_BASELINE_IMAGES=ON)
fi
cmake -S "$WORKSPACE" -B "$BUILD_DIR" "${CMAKE_BACKEND_ARGS[@]}" "${CMAKE_IMAGE_ARGS[@]}" \
      -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --target "UpdateBaseline_$TARGET_SUFFIX" -- -j 12
if ! "$BUILD_DIR/UpdateBaseline_$TARGET_SUFFIX"; then
  mkdir -p "$WORKSPACE/result"
  if [ -d "$WORKSPACE/test/out" ]; then
    cp -R "$WORKSPACE/test/out" "$WORKSPACE/result/"
  fi
  echo "Baseline cache refresh for '$BACKEND_NAME' failed." >&2
  exit 1
fi
RestoreWorkspace
depsync
rm -rf "$BUILD_DIR"
echo "Baseline cache for '$BACKEND_NAME' refreshed successfully."
