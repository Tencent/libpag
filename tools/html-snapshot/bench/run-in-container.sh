#!/usr/bin/env bash
# run-in-container.sh — iterate over every non-subset HTML under
# $INPUT_DIR, run snapshot.js for each, capture per-case resource
# usage via sampler.js, write one JSONL line per case + a Markdown
# summary table.
#
# Designed to be the CMD of the bench Docker image. All arguments are
# positional; no flags.
#
# Usage (inside container):
#   /bench/run-in-container.sh <input_dir> <output_dir> [interval_ms]
#
# Output:
#   <output_dir>/results.jsonl      one JSON line per case
#   <output_dir>/summary.md         human-readable summary
#   <output_dir>/host_meta.json     container/host environment info
#   <output_dir>/<rel>/<name>.subset.html   per-case snapshot output
#
# Notes:
#   - Each `snapshot.js` invocation is wrapped by sampler.js so we
#     measure the full Chromium tree, not just the node process.
#   - We launch one snapshot per case (no warm reuse). Puppeteer
#     could in principle keep one browser hot across cases for big
#     wins on per-case wall time, but for a fair "what does it cost
#     to convert one page" number we measure cold starts.
#   - Wall + memory.peak are reset between cases on kernels that
#     support writing to memory.peak (6.5+). On older kernels we
#     emit a monotonically-increasing value; the *delta* field on
#     each row is still correct (we re-read before/after each case
#     inside sampler.js).

set -euo pipefail

INPUT_DIR=${1:-/inputs}
OUT_DIR=${2:-/out}
INTERVAL_MS=${3:-50}

if [[ ! -d "$INPUT_DIR" ]]; then
  echo "run-in-container.sh: input dir not found: $INPUT_DIR" >&2
  exit 2
fi
mkdir -p "$OUT_DIR"

# ---- environment snapshot -------------------------------------------------

# Resolve puppeteer's version from /app (where it's installed); cwd
# here is unrelated. Likewise, chromium's version comes from the
# system binary the snapshot pipeline actually launches.
PUPPETEER_VERSION=$(cd /app && node -e 'console.log(require("puppeteer/package.json").version)' 2>/dev/null || echo unknown)
CHROMIUM_VERSION=$(/usr/bin/chromium --version 2>/dev/null | awk '{print $1" "$2}' || echo unknown)

{
  echo "{"
  printf '  "kernel": "%s",\n'        "$(uname -r)"
  printf '  "arch": "%s",\n'          "$(uname -m)"
  printf '  "cpu_count": %s,\n'       "$(nproc)"
  printf '  "node_version": "%s",\n'  "$(node --version)"
  printf '  "puppeteer_version": "%s",\n' "$PUPPETEER_VERSION"
  printf '  "chromium_version": "%s",\n' "$CHROMIUM_VERSION"
  printf '  "container_memory_max_bytes": "%s",\n' \
    "$(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo unknown)"
  printf '  "container_cpu_max": "%s"\n' \
    "$(cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo unknown)"
  echo "}"
} > "$OUT_DIR/host_meta.json"

# ---- case discovery -------------------------------------------------------

# Build a stable, sorted list of every non-.subset.html under $INPUT_DIR.
# `find -print0` + readarray handles paths with spaces (tmp_case has
# "ima-glm5.0 turbo/", for example).
mapfile -d '' -t CASES < <(
  find "$INPUT_DIR" -type f -name '*.html' ! -name '*.subset.html' -print0 \
    | sort -z
)

if [[ ${#CASES[@]} -eq 0 ]]; then
  echo "run-in-container.sh: no input .html cases under $INPUT_DIR" >&2
  exit 0
fi

echo "Found ${#CASES[@]} input case(s) under $INPUT_DIR"

# ---- run loop -------------------------------------------------------------

RESULTS="$OUT_DIR/results.jsonl"
: > "$RESULTS"

start_all=$(date +%s)

for src in "${CASES[@]}"; do
  rel=${src#"$INPUT_DIR/"}
  out_dir="$OUT_DIR/$(dirname "$rel")"
  mkdir -p "$out_dir"
  out_file="$out_dir/$(basename "$rel" .html).subset.html"

  echo "-> $rel"

  # The sampler writes exactly one JSON line on its stdout (the result
  # row); the child's stdout/stderr are redirected to fd 2 inside
  # sampler.js, so progress logs surface in `docker logs` while the
  # JSONL stays clean. `set +e` keeps the loop going on snapshot
  # failures — the row is still recorded with the non-zero exit code.
  set +e
  node /bench/sampler.js \
    --label "$rel" \
    --interval-ms "$INTERVAL_MS" \
    -- \
    node /app/snapshot.js "$src" -o "$out_file" \
    >> "$RESULTS"
  status=$?
  set -e

  if [[ $status -ne 0 ]]; then
    echo "   (snapshot exited $status; row still recorded for diagnostics)"
  fi
done

elapsed=$(( $(date +%s) - start_all ))
echo "All ${#CASES[@]} cases done in ${elapsed}s"

# ---- summary --------------------------------------------------------------

node /bench/summarize.js "$RESULTS" "$OUT_DIR/host_meta.json" \
  > "$OUT_DIR/summary.md"
echo
echo "Wrote $RESULTS, $OUT_DIR/summary.md"
