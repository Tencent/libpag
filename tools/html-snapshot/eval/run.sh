#!/usr/bin/env bash
# run.sh — thin wrapper around run.js. Forwards every argument verbatim and
# defaults the label so a bare invocation produces a reproducible directory.
#
# Examples:
#   ./run.sh                      # full corpus, label=current
#   ./run.sh --label baseline-v1  # full corpus, label=baseline-v1
#   ./run.sh --only meitu         # subset of cases for fast iteration
set -u
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec node "$SCRIPT_DIR/run.js" "$@"
