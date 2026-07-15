#!/usr/bin/env bash
#
# Assemble and pack the @libpag/pagx npm package.
#
# The published package layout (what package.json's "bin" + "files" expect) is:
#
#   bin/pagx.js                platform-dispatch wrapper (tracked source)
#   bin/<platform>/pagx        native CLI binary (e.g. bin/darwin/pagx)
#   bin/<platform>-<arch>/pagx arch-specific native binary (preferred by the
#                              wrapper, e.g. bin/linux-arm64/pagx)
#   html-snapshot/             bundled HTML->PAGX snapshot runtime (generated)
#   README.md
#
# Native binaries come from the CMake `pagx-cli` target (OUTPUT_NAME=pagx) and
# are staged under cli/npm/bin/{darwin,linux,win32}/ — the gitignored drop
# locations. The wrapper bin/pagx.js prefers bin/<platform>-<arch>/ then falls
# back to bin/<platform>/, so a single-arch host build can land in the plain
# bin/<platform>/ directory.
#
# The html-snapshot/ runtime and the multi-platform binary check are NOT done by
# this script directly: package.json wires them into npm lifecycle hooks, so
# `npm pack` runs `prepack` (scripts/build-html-snapshot.js) and `npm publish`
# additionally runs `prepublishOnly` (scripts/check-binaries.js). This script
# only builds/stages the host binary (with --build) and invokes npm.
#
# Usage:
#   ./pack.sh                 # pack using already-staged binaries
#   ./pack.sh --build         # build host platform via CMake, stage it, then pack
#   ./pack.sh --sync          # run ../../sync_deps.sh first (one-time deps setup)
#   ./pack.sh --publish       # build/stage, then npm publish --access public
#   ./pack.sh --build-dir DIR # use DIR as the CMake build directory (with --build)
#
# Third-party dependencies must be synced once before --build can succeed (see
# the repo README). That step installs global tooling (depsync, emscripten) and
# needs network + elevated permissions, so it is NOT run automatically. Run it
# yourself once with `./sync_deps.sh` from the repo root, or pass --sync here.
#
# A full multi-platform release requires building pagx on each OS and staging
# all binaries (e.g. copy CI artifacts into cli/npm/bin/{darwin,linux,win32}/)
# before running this script without --build.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN_DIR="$SCRIPT_DIR/bin"

DO_BUILD=0
DO_SYNC=0
DO_PUBLISH=0
# Dedicated build dir: avoids inheriting a stale/inconsistent cache from the
# user's own cmake-build-* directories (e.g. one configured with a shared
# `pag`, against which the CLI cannot link — see the static-link note below).
BUILD_DIR="$REPO_ROOT/cmake-build-pagx-npm"

usage() {
  sed -n '2,44p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build) DO_BUILD=1; shift ;;
    --sync) DO_SYNC=1; shift ;;
    --publish) DO_PUBLISH=1; shift ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

# Map `uname` to the Node `process.platform` strings the wrapper uses.
host_platform() {
  case "$(uname -s)" in
    Darwin) echo darwin ;;
    Linux) echo linux ;;
    MINGW*|MSYS*|CYGWIN*) echo win32 ;;
    *) echo "unsupported host platform: $(uname -s)" >&2; exit 1 ;;
  esac
}

bin_name_for() {
  [[ "$1" == win32 ]] && echo "pagx.exe" || echo "pagx"
}

if [[ $DO_SYNC -eq 1 ]]; then
  echo "==> Syncing third-party dependencies (./sync_deps.sh)"
  (cd "$REPO_ROOT" && ./sync_deps.sh)
fi

if [[ $DO_BUILD -eq 1 ]]; then
  plat="$(host_platform)"
  binname="$(bin_name_for "$plat")"

  # The pagx-cli target links the `pag` library directly. As a SHARED library
  # / macOS framework, `pag` only exports its public PAG_API surface — the
  # internal pagx::* symbols the CLI needs are hidden, so the link fails with
  # "undefined symbols". Building `pag` as a STATIC archive (PAG_BUILD_SHARED=OFF,
  # which also requires PAG_BUILD_FRAMEWORK=OFF on macOS) makes every symbol
  # available to the CLI at link time.
  #
  # PAG_BUILD_PAGX is forced ON here too: PAG_BUILD_CLI only flips it via a
  # non-cache set(), so a stale cached OFF would otherwise drop the PAGX
  # sources from `pag`.
  echo "==> Configuring CMake (CLI=ON, PAGX=ON, static pag) in $BUILD_DIR"
  cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPAG_BUILD_CLI=ON -DPAG_BUILD_PAGX=ON \
    -DPAG_BUILD_SHARED=OFF -DPAG_BUILD_FRAMEWORK=OFF

  echo "==> Building pagx-cli"
  cmake --build "$BUILD_DIR" --target pagx-cli

  # Ninja (single-config) emits the binary at the build-dir root; multi-config
  # generators (e.g. Visual Studio) nest it under a config subdir.
  built=""
  for cand in "$BUILD_DIR/$binname" "$BUILD_DIR/Release/$binname"; do
    [[ -f "$cand" ]] && built="$cand" && break
  done
  if [[ -z "$built" ]]; then
    echo "error: could not locate built $binname under $BUILD_DIR" >&2
    exit 1
  fi

  # Stage into the gitignored drop-zone bin/<platform>/ (the wrapper falls back
  # to this when no arch-specific bin/<platform>-<arch>/ directory is present).
  echo "==> Staging bin/$plat/$binname"
  mkdir -p "$BIN_DIR/$plat"
  cp "$built" "$BIN_DIR/$plat/$binname"
  chmod +x "$BIN_DIR/$plat/$binname" 2>/dev/null || true
fi

# bin/pagx.js is a tracked source file, so never wipe bin/ wholesale. Just make
# the platform binaries executable and report what is currently staged. The
# wrapper accepts both bin/<platform>/ and bin/<platform>-<arch>/ layouts.
if [[ ! -f "$BIN_DIR/pagx.js" ]]; then
  echo "error: $BIN_DIR/pagx.js is missing (it is tracked source — restore it)" >&2
  exit 1
fi

found=0
shopt -s nullglob
for dir in "$BIN_DIR"/*/; do
  plat="$(basename "$dir")"
  base="${plat%%-*}" # strip any -<arch> suffix
  binname="$(bin_name_for "$base")"
  src="$dir$binname"
  if [[ -f "$src" ]]; then
    chmod +x "$src" 2>/dev/null || true
    echo "    + bin/$plat/$binname"
    found=1
  fi
done
shopt -u nullglob

if [[ $found -eq 0 ]]; then
  echo "error: no platform binaries found under $BIN_DIR/<platform>[-<arch>]/" >&2
  echo "       run with --build to build the host platform, or stage prebuilt" >&2
  echo "       binaries (e.g. CI artifacts) into bin/{darwin,linux,win32}/ first." >&2
  exit 1
fi

# npm runs the lifecycle hooks for us: `prepack` builds html-snapshot/ and, for
# publish, `prepublishOnly` runs the multi-platform binary check.
cd "$SCRIPT_DIR"
if [[ $DO_PUBLISH -eq 1 ]]; then
  echo "==> npm publish --access public"
  npm publish --access public
else
  echo "==> npm pack"
  npm pack
fi
