#!/usr/bin/env bash
# One-time environment setup / health check for the html-to-pagx skill.
#
# Verifies the two tools the conversion needs and prepares the snapshot tool:
#   1. node      — required to run the snapshot pipeline
#   2. pagx      — the CLI that imports/renders PAGX (installed via npm if missing)
#   3. tools/html-snapshot — npm deps installed + TypeScript built (dist/)
#   4. headless Chromium — bundled with puppeteer; reported (not auto-installed) if missing
#
# Idempotent: re-running only does work that is still needed. Prints "setup: ready"
# on success. Run from anywhere inside the repository.

set -euo pipefail

say() { printf 'setup: %s\n' "$1"; }
warn() { printf 'setup: WARNING: %s\n' "$1" >&2; }
die() { printf 'setup: ERROR: %s\n' "$1" >&2; exit 1; }

# --- locate the repository root and the snapshot tool ---------------------
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -z "$REPO_ROOT" ]; then
  # Fall back to walking up from this script to find tools/html-snapshot.
  d="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  while [ "$d" != "/" ] && [ ! -d "$d/tools/html-snapshot" ]; do d="$(dirname "$d")"; done
  REPO_ROOT="$d"
fi
TOOL_DIR="$REPO_ROOT/tools/html-snapshot"

# --- node -----------------------------------------------------------------
command -v node >/dev/null 2>&1 || die "node is not installed. Install Node.js (https://nodejs.org) and re-run."
say "node $(node -v)"

# --- pagx -----------------------------------------------------------------
if ! command -v pagx >/dev/null 2>&1; then
  say "pagx not found; installing @libpag/pagx globally via npm..."
  npm install -g @libpag/pagx || die "failed to install pagx. Install it manually: npm install -g @libpag/pagx"
fi
say "pagx $(pagx -v 2>/dev/null | awk '{print $NF}')"

# --- no repository: the snapshot tool is bundled inside @libpag/pagx -------
# When tools/html-snapshot is absent (a user who only `npm install`-ed the
# package), there is nothing to build here: `pagx` carries the snapshot tool
# and installs the headless browser lazily on first use.
if [ ! -d "$TOOL_DIR" ]; then
  say "tools/html-snapshot not present; using the snapshot tool bundled in @libpag/pagx"
  say "the headless browser installs automatically on the first conversion (~150 MB, one-time)"
  say "ready"
  exit 0
fi

# --- snapshot tool: dependencies ------------------------------------------
if [ ! -d "$TOOL_DIR/node_modules" ]; then
  say "installing snapshot tool dependencies (this downloads a Chromium build, ~150 MB)..."
  ( cd "$TOOL_DIR" && npm install )
else
  say "snapshot tool dependencies present"
fi

# --- snapshot tool: build (html2pagx requires dist/) ----------------------
if [ ! -d "$TOOL_DIR/dist/lib" ]; then
  say "building snapshot tool (tsc)..."
  ( cd "$TOOL_DIR" && npm run build )
else
  say "snapshot tool already built"
fi

# --- headless Chromium ----------------------------------------------------
CACHE_DIR="${PUPPETEER_CACHE_DIR:-$HOME/.cache/puppeteer}"
if [ -d "$CACHE_DIR/chrome" ] && [ -n "$(ls -A "$CACHE_DIR/chrome" 2>/dev/null)" ]; then
  say "headless Chromium present"
else
  warn "headless Chromium is missing from $CACHE_DIR/chrome"
  warn "install it with:"
  warn "  PUPPETEER_CACHE_DIR=\"$CACHE_DIR\" npx --prefix \"$TOOL_DIR\" puppeteer browsers install chrome"
  warn "then re-run this script."
  exit 1
fi

say "ready"
