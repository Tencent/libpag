#!/usr/bin/env bash
# run-in-container.sh — container-side shim around run-cases.sh.
#
# Designed to be the CMD of the bench Docker image. All real work
# (case iteration, sampling, summary) lives in run-cases.sh; this
# script just plumbs in the container-specific paths and turns on
# cgroup collection (CGROUP=1) because inside a `--cpus`/`--memory`
# limited container the cgroup numbers are a useful kernel-accounted
# cross-check.
#
# Usage (inside container):
#   /bench/run-in-container.sh <input_dir> <output_dir> [interval_ms]

set -euo pipefail

INPUT_DIR=${1:-/inputs}
OUT_DIR=${2:-/out}
INTERVAL_MS=${3:-50}

exec env CGROUP=1 \
  /bench/run-cases.sh "$INPUT_DIR" "$OUT_DIR" /app /bench "$INTERVAL_MS"
