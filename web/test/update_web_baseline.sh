#!/bin/bash

# Internal script for updating the web baseline cache. DO NOT call directly.
# Use the project root entry point instead:
#   ./update_baseline.sh USE_WEBGL [--skip-images]
#   ./update_baseline.sh USE_WEBGPU [--skip-images]
#
# Invoked by the root update_baseline.sh with a backend argument, and optionally
# --skip-images to omit generating `_base.webp` reference images (cache-only mode).
#
# The cache is seeded from origin/main once the web/test harness itself has landed
# there. Before that, main has neither the harness nor the Emscripten portability
# fixes in test/src, so the cache is seeded from the current branch instead; re-run
# this script after the harness merges to main to refresh with true main baselines.

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
CACHE_DIR="$PROJECT_ROOT/test/baseline/.cache/${BACKEND}"
CACHE_VERSION_FILE="$CACHE_DIR/version.json"

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

git fetch origin main --quiet
MAIN_COMMIT=$(git rev-parse --verify 'origin/main^{commit}')

# Decide where the baselines come from: main once the harness is there, the
# current branch otherwise (see the header comment).
SEED_FROM_MAIN=0
if git cat-file -e "$MAIN_COMMIT:web/test/CMakeLists.txt" 2>/dev/null; then
    SEED_FROM_MAIN=1
    SEED_LABEL="main"
else
    SEED_LABEL="current branch"
fi

# Skip the rebuild when the cache already matches the seed revision's version.json.
if [ -f "$CACHE_VERSION_FILE" ]; then
    if [ "$SEED_FROM_MAIN" -eq 1 ]; then
        REF_VERSION=$(git show "$MAIN_COMMIT:test/baseline/version.json" 2>/dev/null || true)
    else
        REF_VERSION=$(cat "$PROJECT_ROOT/test/baseline/version.json" 2>/dev/null || true)
    fi
    if [ -n "$REF_VERSION" ] && [ "$REF_VERSION" = "$(cat "$CACHE_VERSION_FILE")" ]; then
        echo "Baseline cache for '$BACKEND' is up to date."
        exit 0
    fi
fi

echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND) Start~~~~~~~~~~~~~~~~~~~~~"
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
CURRENT_COMMIT=$(git rev-parse HEAD)

if [ "$SEED_FROM_MAIN" -eq 1 ]; then
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

    git switch --detach "$MAIN_COMMIT" --quiet
else
    echo "web/test is not on main yet; seeding the '$BACKEND' cache from the current branch."
fi

cd "$WEB_TEST_DIR"

echo ""
echo "Building web tests on $SEED_LABEL (UPDATE_BASELINE mode, backend: $BACKEND)..."
rm -rf "$BUILD_DIR"

CMAKE_ARGS="-DPAG_UPDATE_BASELINE=ON"
if [ "$BACKEND" = "webgpu" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DPAG_USE_WEBGPU=ON"
fi

emcmake cmake -G Ninja -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release $CMAKE_ARGS $SKIP_IMAGES_FLAG
cmake --build "$BUILD_DIR" -- -j 8
npm run "copy:${BACKEND}"

echo ""
echo "Running web tests on $SEED_LABEL..."
node run_test.js --backend="$BACKEND" || true

cd "$PROJECT_ROOT"

echo "~~~~~~~~~~~~~~~~~~~Update Baseline ($BACKEND) Success~~~~~~~~~~~~~~~~~~~~~"
echo "Baseline cache updated at: test/baseline/.cache/${BACKEND}/"
