#!/usr/bin/env bash
# run.sh — thin wrapper around run.js. Forwards every argument verbatim and defaults the label so a
# bare invocation produces a reproducible directory.
#
# Examples:
#   ./run.sh --corpus ~/anim_cases                 # full corpus, label=current
#   ./run.sh --corpus ~/anim_cases --label v1      # tag the run for diffing
#   ./run.sh --corpus ~/anim_cases --only fade      # subset of cases for fast iteration
#   ./run.sh --corpus ~/anim_cases --samples 9      # finer timeline sampling
set -u
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec node "$SCRIPT_DIR/run.js" "$@"
