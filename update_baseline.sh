#!/usr/bin/env bash

# Refresh the selected backend's local baseline cache from origin/main.
# Usage: ./update_baseline.sh [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL|USE_WEBGL|USE_WEBGPU] [--skip-images]
# Screenshot outputs and version metadata remain shared, so run backends sequentially.

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
    # Web lanes run through the Emscripten harness in web/test.
    export PAG_WEB_BASELINE_INVOKED=1
    SKIP_FLAG=""
    if [ "$SKIP_IMAGES" -eq 1 ]; then SKIP_FLAG="--skip-images"; fi
    WEB_BACKEND=$(echo "${BACKEND_ARG#USE_}" | tr '[:upper:]' '[:lower:]')
    exec "$WORKSPACE/web/test/update_web_baseline.sh" "$WEB_BACKEND" "$SKIP_FLAG"
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

git fetch origin main
MAIN_COMMIT=$(git rev-parse --verify 'origin/main^{commit}')
MAIN_VERSION=$(git show "$MAIN_COMMIT:test/baseline/version.json")

CACHE_DIR="$WORKSPACE/test/baseline/.cache/$BACKEND_NAME"
if [ -f "$CACHE_DIR/version.json" ] && [ -f "$CACHE_DIR/md5.json" ]; then
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
git switch --detach "$MAIN_COMMIT" --quiet
./install_tools.sh
depsync

mkdir -p "$BUILD_DIR"
CMAKE_IMAGE_ARGS=(-DPAG_SKIP_GENERATE_BASELINE_IMAGES=OFF)
if [ "$SKIP_IMAGES" -eq 1 ]; then
  CMAKE_IMAGE_ARGS=(-DPAG_SKIP_GENERATE_BASELINE_IMAGES=ON)
fi
cmake -S "$WORKSPACE" -B "$BUILD_DIR" "${CMAKE_BACKEND_ARGS[@]}" "${CMAKE_IMAGE_ARGS[@]}" \
      -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DCMAKE_BUILD_TYPE=Debug
BASELINE_TARGET="UpdateBaseline_$TARGET_SUFFIX"
GENERATED_CACHE_DIR="$CACHE_DIR"
TARGET_LIST="$BUILD_DIR/CMakeFiles/TargetDirectories.txt"
if ! grep -Fxq "$BUILD_DIR/CMakeFiles/$BASELINE_TARGET.dir" "$TARGET_LIST"; then
  if [ "$TARGET_SUFFIX" = OpenGL ] &&
     grep -Fxq "$BUILD_DIR/CMakeFiles/UpdateBaseline.dir" "$TARGET_LIST"; then
    BASELINE_TARGET=UpdateBaseline
    GENERATED_CACHE_DIR="$WORKSPACE/test/baseline/.cache"
    echo "Using the legacy OpenGL baseline target on main."
  else
    echo "Error: main does not provide $BASELINE_TARGET." >&2
    exit 1
  fi
fi

cmake --build "$BUILD_DIR" --target "$BASELINE_TARGET" -- -j 12
# Older main revisions write their OpenGL results to the shared cache. Remove the previous MD5
# file so a failed or incomplete refresh cannot be imported as a fresh per-backend cache.
rm -f "$GENERATED_CACHE_DIR/md5.json"
if ! "$BUILD_DIR/$BASELINE_TARGET"; then
  mkdir -p "$WORKSPACE/result"
  if [ -d "$WORKSPACE/test/out" ]; then
    cp -R "$WORKSPACE/test/out" "$WORKSPACE/result/"
  fi
  echo "Baseline cache refresh for '$BACKEND_NAME' failed." >&2
  exit 1
fi
if [ ! -s "$GENERATED_CACHE_DIR/md5.json" ] ||
   [ ! -s "$GENERATED_CACHE_DIR/HEAD" ] ||
   ! cmp -s "$WORKSPACE/test/baseline/version.json" "$GENERATED_CACHE_DIR/version.json"; then
  echo "Error: $BASELINE_TARGET did not produce a complete baseline cache." >&2
  exit 1
fi
if [ "$GENERATED_CACHE_DIR" != "$CACHE_DIR" ]; then
  mkdir -p "$CACHE_DIR"
  cp "$GENERATED_CACHE_DIR/md5.json" "$GENERATED_CACHE_DIR/version.json" \
     "$GENERATED_CACHE_DIR/HEAD" "$CACHE_DIR/"
  echo "Imported the legacy OpenGL cache into '$BACKEND_NAME'."
fi
RestoreWorkspace
depsync
rm -rf "$BUILD_DIR"
echo "Baseline cache for '$BACKEND_NAME' refreshed successfully."
