#!/bin/bash

# Internal script for updating the web baseline cache. DO NOT call directly.
# Use the project root entry point instead:
#   ./update_baseline.sh USE_WEBGL [--skip-images]
#   ./update_baseline.sh USE_WEBGPU [--skip-images]
#
# Invoked by the root update_baseline.sh with a backend argument, and optionally
# --skip-images to omit generating `_base.webp` reference images (cache-only mode).

set -e

# Guard against direct invocation.
if [ -z "$PAG_WEB_BASELINE_INVOKED" ]; then
    echo "Error: Do not run this script directly."
    echo "Use from project root:"
    echo "  ./update_baseline.sh USE_WEBGL"
    echo "  ./update_baseline.sh USE_WEBGPU"
    exit 1
fi

BACKEND=${1:-webgl}
SKIP_IMAGES_FLAG=""
if [ "$2" = "--skip-images" ]; then
    SKIP_IMAGES_FLAG="-DPAG_SKIP_GENERATE_BASELINE_IMAGES=ON"
fi

if [ "$BACKEND" != "webgl" ] && [ "$BACKEND" != "webgpu" ]; then
    echo "Error: Invalid backend '$BACKEND'. Must be 'webgl' or 'webgpu'." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WEB_TEST_DIR="$SCRIPT_DIR"
BUILD_DIR="$WEB_TEST_DIR/build-${BACKEND}"
CACHE_VERSION_FILE="$PROJECT_ROOT/test/baseline/.cache/${BACKEND}/version.json"

# Auto-detect emsdk if not already active.
if [ -z "$EMSDK" ]; then
    if [ -f "$SCRIPT_DIR/../emsdk/emsdk_env.sh" ]; then
        # shellcheck source=/dev/null
        . "$SCRIPT_DIR/../emsdk/emsdk_env.sh"
    else
        echo "Error: EMSDK is not active and web/emsdk is not installed." >&2
        echo "Run: node web/script/setup.emsdk.js && source web/emsdk/emsdk_env.sh" >&2
        exit 1
    fi
fi

cd "$PROJECT_ROOT"

# Check if the cache is up to date with origin/main.
if [ -f "$CACHE_VERSION_FILE" ]; then
    MAIN_VERSION=$(git show origin/main:test/baseline/version.json 2>/dev/null || true)
    if [ -n "$MAIN_VERSION" ]; then
        CACHE_CONTENT=$(cat "$CACHE_VERSION_FILE")
        if [ "$MAIN_VERSION" = "$CACHE_CONTENT" ]; then
            echo "Baseline cache for '$BACKEND' is up to date."
            exit 0
        fi
    fi
fi

echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND) Start~~~~~~~~~~~~~~~~~~~~~"
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
CURRENT_COMMIT=$(git rev-parse HEAD)
# Remove build artifacts before stash to avoid "already exists" conflicts on pop.
rm -rf "$BUILD_DIR"
STASH_LIST_BEFORE=$(git stash list)
git stash push --include-untracked --quiet
STASH_LIST_AFTER=$(git stash list)

restore_branch() {
    cd "$PROJECT_ROOT"
    if [[ $CURRENT_BRANCH == "HEAD" ]]; then
        git checkout "$CURRENT_COMMIT" --quiet 2>/dev/null || true
    else
        git switch "$CURRENT_BRANCH" --quiet 2>/dev/null || true
    fi
    if [[ $STASH_LIST_BEFORE != $STASH_LIST_AFTER ]]; then
        git stash pop --index --quiet 2>/dev/null || true
    fi
}
trap restore_branch EXIT

git switch main --quiet

cd "$WEB_TEST_DIR"

echo ""
echo "Building web tests on main (UPDATE_BASELINE mode, backend: $BACKEND)..."
rm -rf "$BUILD_DIR"

CMAKE_ARGS="-DPAG_UPDATE_BASELINE=ON"
if [ "$BACKEND" = "webgpu" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DPAG_USE_WEBGPU=ON"
fi

emcmake cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release $CMAKE_ARGS $SKIP_IMAGES_FLAG
cmake --build "$BUILD_DIR" -- -j 8
npm run "copy:${BACKEND}"

echo ""
echo "Running web tests on main..."
node run_test.js --backend="$BACKEND" || true

cd "$PROJECT_ROOT"

echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND) Success~~~~~~~~~~~~~~~~~~~~~"
echo "Baseline cache updated at: test/baseline/.cache/${BACKEND}/"
