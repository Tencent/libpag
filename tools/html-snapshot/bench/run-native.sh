#!/usr/bin/env bash
# run-native.sh — Native (no-Docker) entrypoint for the html-snapshot
# benchmark. Runs the same per-case loop as run-bench.sh but in the
# host environment, so it works on macOS as well as Linux without a
# container runtime.
#
# Prerequisites:
#   - `npm install` already run in tools/html-snapshot/ (provides
#     puppeteer, which on macOS also downloads the bundled
#     Chrome-for-Testing into ~/.cache/puppeteer).
#   - On Linux, either let puppeteer use its own bundled Chrome, or
#     install distro chromium and export
#     `PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium`.
#
# Usage:
#   tools/html-snapshot/bench/run-native.sh <input_dir> [output_dir]
#
# Environment overrides:
#   INTERVAL_MS    sampler tick (ms)  (default: 50)
#
# Caveats vs. the Dockerised run-bench.sh:
#   - No cgroup metrics. On macOS cgroups don't exist; on Linux native
#     the global cgroup is contaminated by other host processes, so
#     the numbers would be misleading. The `cgroup_*` columns in
#     results.jsonl come back null and summary.md renders them as
#     `n/a`.
#   - macOS uses `ps -A` for the process-tree walk and reports VmRSS
#     in KB; PSS isn't exposed by macOS, so `peak_proc_tree_pss_mb`
#     is null. Linux native keeps the full /proc + smaps_rollup path,
#     so PSS is still available.
#   - No `--cpus` / `--memory` envelope. The host runs as configured;
#     numbers reflect that, not a constrained container.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <input_dir> [output_dir]" >&2
  exit 2
fi

INPUT_DIR=$(cd "$1" && pwd)
OUTPUT_DIR=${2:-$PWD/bench-out}
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR=$(cd "$OUTPUT_DIR" && pwd)

INTERVAL_MS=${INTERVAL_MS:-50}

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SNAPSHOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)

if [[ ! -d "$SNAPSHOT_DIR/node_modules/puppeteer" ]]; then
  echo "run-native.sh: puppeteer not installed under $SNAPSHOT_DIR." >&2
  echo "  run: (cd $SNAPSHOT_DIR && npm install)" >&2
  exit 2
fi

echo "INPUT_DIR    = $INPUT_DIR"
echo "OUTPUT_DIR   = $OUTPUT_DIR"
echo "SNAPSHOT_DIR = $SNAPSHOT_DIR"
echo "BENCH_DIR    = $SCRIPT_DIR"
echo "INTERVAL_MS  = $INTERVAL_MS"
echo

# CGROUP=0 forces sampler.js's --no-cgroup; see run-cases.sh header
# for why we suppress cgroup reads outside the bench container.
env CGROUP=0 \
  "$SCRIPT_DIR/run-cases.sh" \
    "$INPUT_DIR" "$OUTPUT_DIR" "$SNAPSHOT_DIR" "$SCRIPT_DIR" "$INTERVAL_MS"

echo
echo "==> results: $OUTPUT_DIR/results.jsonl"
echo "==> summary: $OUTPUT_DIR/summary.md"
