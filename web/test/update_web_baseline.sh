#!/bin/bash

# Internal cache refresh entry point. This does not accept repository baseline changes.
# Only a complete successful run of origin/main may replace the local backend cache.
set -eo pipefail

if [ "${PAG_WEB_BASELINE_INVOKED:-}" != "1" ]; then
    echo "Error: use the project root baseline entry point." >&2
    exit 1
fi

BACKEND=${1:-webgl}
if [ "$BACKEND" != "webgl" ] && [ "$BACKEND" != "webgpu" ]; then
    echo "Error: invalid backend '$BACKEND'." >&2
    exit 1
fi
SKIP_IMAGES=OFF
if [ "${2:-}" = "--skip-images" ]; then
    SKIP_IMAGES=ON
elif [ -n "${2:-}" ]; then
    echo "Error: unknown option '$2'." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [ -z "${EMSDK:-}" ]; then
    if [ ! -f "$SCRIPT_DIR/../emsdk/emsdk_env.sh" ]; then
        echo "Error: activate EMSDK before refreshing the cache." >&2
        exit 1
    fi
    . "$SCRIPT_DIR/../emsdk/emsdk_env.sh"
fi

cd "$PROJECT_ROOT"
git fetch origin main --quiet
MAIN_COMMIT=$(git rev-parse --verify 'origin/main^{commit}')
if ! git cat-file -e "$MAIN_COMMIT:web/test/artifacts.js" 2>/dev/null; then
    echo "Error: origin/main does not contain the strict Web harness yet." >&2
    echo "Refusing to seed regression expectations from the branch under test." >&2
    exit 1
fi
CURRENT_BRANCH=$(git symbolic-ref --short -q HEAD || true)
CURRENT_COMMIT=$(git rev-parse HEAD)
STASH_BEFORE=$(git rev-parse --verify -q refs/stash || true)
STASH_CREATED=0
restore_branch() {
    local status=$?
    trap - EXIT
    cd "$PROJECT_ROOT"
    if [ -n "$CURRENT_BRANCH" ]; then
        git switch "$CURRENT_BRANCH" --quiet || exit 1
    else
        git switch --detach "$CURRENT_COMMIT" --quiet || exit 1
    fi
    if [ "$STASH_CREATED" -eq 1 ]; then
        if ! git stash pop --index --quiet; then
            echo "Error: could not restore the stashed changes. Keep the stash for recovery." >&2
            exit 1
        fi
    fi
    exit "$status"
}
trap restore_branch EXIT
# Stash local changes (including untracked files) so the build runs on a pristine origin/main
# checkout; the trap above restores them on exit.
git stash push --include-untracked --quiet
STASH_AFTER=$(git rev-parse --verify -q refs/stash || true)
if [ "$STASH_BEFORE" != "$STASH_AFTER" ]; then
    STASH_CREATED=1
fi
git switch --detach "$MAIN_COMMIT" --quiet

cd "$SCRIPT_DIR"
node cache-source.js
BUILD_PARENT="$SCRIPT_DIR/build-baseline-${BACKEND}"
mkdir -p "$BUILD_PARENT"
# A fresh CMake tree cannot inherit another TGFX_DIR or a stale merged libpag archive.
BUILD_DIR=$(mktemp -d "$BUILD_PARENT/run.XXXXXX")
USE_WEBGPU=OFF
if [ "$BACKEND" = "webgpu" ]; then
    USE_WEBGPU=ON
fi
emcmake cmake -G Ninja -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DTGFX_DIR="$PROJECT_ROOT/third_party/tgfx" \
    -DPAG_UPDATE_BASELINE=ON -DPAG_USE_WEBGPU="$USE_WEBGPU" \
    -DPAG_SKIP_GENERATE_BASELINE_IMAGES="$SKIP_IMAGES"
cmake --build "$BUILD_DIR" -- -j 8
node artifacts.js "$BACKEND" update "$BUILD_DIR"
node run_test.js --backend="$BACKEND" --mode=update

echo "Baseline cache refreshed from origin/main for $BACKEND."
