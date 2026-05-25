#!/usr/bin/env bash
# run-cases.sh — generic per-case runner for the html-snapshot bench.
#
# Iterates every non-`*.subset.html` file under <input_dir>, runs
# `snapshot.js` for each one under `sampler.js`, and writes:
#   <output_dir>/host_meta.json
#   <output_dir>/results.jsonl
#   <output_dir>/summary.md
#   <output_dir>/<rel>/<name>.subset.html
#
# This script is environment-agnostic: it works both inside the bench
# Docker container (called via run-in-container.sh) and natively on a
# Linux or macOS host (called via run-native.sh). All paths are
# explicit positional arguments — there is no hard-coded `/app` /
# `/bench` here, and no docker dependency.
#
# Usage:
#   run-cases.sh <input_dir> <output_dir> <snapshot_dir> <bench_dir> [interval_ms]
#
# Arguments:
#   snapshot_dir   directory containing snapshot.js + node_modules
#                  (e.g. /app in the container, or tools/html-snapshot
#                  on the host)
#   bench_dir      directory containing sampler.js + summarize.js
#                  (e.g. /bench in the container, or this script's dir)
#
# Environment overrides:
#   CGROUP=1|0     pass --cgroup / --no-cgroup to sampler.js. Default
#                  is 1 (collect) inside the Docker container and 0
#                  (skip) elsewhere; the wrapper scripts set this
#                  explicitly so behaviour is deterministic.
#   BASELINE_RUNS  number of "browser opens about:blank" baseline runs
#                  prepended to the case loop (default 1). Each run
#                  emits a row labelled `__baseline:blank/<n>` so
#                  reports can show the floor browser cost separately
#                  from real snapshot work. Set to 0 to skip.
#   BASELINE_HOLD_MS
#                  ms baseline-blank.js holds the page open after
#                  navigation (default 200) so sampler.js gets at
#                  least one steady-state tick on fast machines.
#
# Notes:
#   - Bash 3.2 compatible (no `mapfile`, no `sort -z`). The case-list
#     is collected via a NUL-terminated find + while-read loop, which
#     handles spaces in directory names ("ima-glm5.0 turbo/") but not
#     newlines in filenames (pathological, unsupported).
#   - Each snapshot.js invocation is wrapped by sampler.js so we
#     measure the full Chromium tree, not just the node process.
#   - Cold start per case: a fresh node + chromium is launched for
#     every input. This is the fair "what does one page cost in
#     isolation" measurement; batched/warm conversion would be faster.

set -euo pipefail

if [[ $# -lt 4 ]]; then
  echo "usage: $0 <input_dir> <output_dir> <snapshot_dir> <bench_dir> [interval_ms]" >&2
  exit 2
fi

INPUT_DIR=$1
OUT_DIR=$2
SNAPSHOT_DIR=$3
BENCH_DIR=$4
INTERVAL_MS=${5:-50}

if [[ ! -d "$INPUT_DIR" ]]; then
  echo "run-cases.sh: input dir not found: $INPUT_DIR" >&2
  exit 2
fi
if [[ ! -f "$SNAPSHOT_DIR/snapshot.js" ]]; then
  echo "run-cases.sh: snapshot.js not found under $SNAPSHOT_DIR" >&2
  exit 2
fi
if [[ ! -f "$BENCH_DIR/sampler.js" ]]; then
  echo "run-cases.sh: sampler.js not found under $BENCH_DIR" >&2
  exit 2
fi
if [[ ! -f "$BENCH_DIR/baseline-blank.js" ]]; then
  echo "run-cases.sh: baseline-blank.js not found under $BENCH_DIR" >&2
  exit 2
fi
mkdir -p "$OUT_DIR"

# ---- environment snapshot -------------------------------------------------

# Resolve puppeteer's version from the snapshot dir (where its
# node_modules live); cwd here is unrelated.
PUPPETEER_VERSION=$(
  cd "$SNAPSHOT_DIR" \
    && node -e 'console.log(require("puppeteer/package.json").version)' 2>/dev/null \
    || echo unknown
)

# Chromium version: prefer the binary puppeteer is actually going to
# launch (PUPPETEER_EXECUTABLE_PATH, set in the Docker image), fall
# back to distro chromium, then to puppeteer's bundled Chrome-for-
# Testing under the snapshot dir's cache (the common macOS path).
# We take the whole `--version` line as-is rather than slicing to
# the first two whitespace-separated tokens — distro Chromium emits
# "Chromium 124.0.x", but Chrome-for-Testing emits "Google Chrome
# for Testing 131.0.x", and the latter loses its version under any
# fixed-column slice.
chromium_probe() {
  local bin=$1
  if [[ -z "$bin" || ! -x "$bin" ]]; then
    return 1
  fi
  "$bin" --version 2>/dev/null | head -n 1 | tr -d '\r' | sed 's/[[:space:]]*$//'
}

CHROMIUM_VERSION=unknown
if v=$(chromium_probe "${PUPPETEER_EXECUTABLE_PATH:-}"); then
  CHROMIUM_VERSION=$v
elif v=$(chromium_probe /usr/bin/chromium); then
  CHROMIUM_VERSION=$v
else
  PUPPETEER_BUNDLED=$(
    cd "$SNAPSHOT_DIR" \
      && node -e 'try { console.log(require("puppeteer").executablePath()); } catch (e) {}' 2>/dev/null
  ) || PUPPETEER_BUNDLED=""
  if v=$(chromium_probe "$PUPPETEER_BUNDLED"); then
    CHROMIUM_VERSION=$v
  fi
fi
[[ -z "$CHROMIUM_VERSION" ]] && CHROMIUM_VERSION=unknown

OS=$(uname -s)
case "$OS" in
  Linux)
    CPU_COUNT=$(nproc 2>/dev/null || echo unknown)
    CGROUP_MEM=$(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo unsupported)
    CGROUP_CPU=$(cat /sys/fs/cgroup/cpu.max 2>/dev/null || echo unsupported)
    ;;
  Darwin)
    CPU_COUNT=$(sysctl -n hw.ncpu 2>/dev/null || echo unknown)
    CGROUP_MEM=unsupported
    CGROUP_CPU=unsupported
    ;;
  *)
    CPU_COUNT=unknown
    CGROUP_MEM=unsupported
    CGROUP_CPU=unsupported
    ;;
esac

# Default cgroup collection: ON inside the bench container, OFF in
# the native wrappers. CGROUP env var lets each wrapper be explicit
# without round-tripping through sampler.js argv parsing.
COLLECT_CGROUP=${CGROUP:-0}

{
  echo "{"
  printf '  "kernel": "%s",\n'           "$(uname -r)"
  printf '  "arch": "%s",\n'             "$(uname -m)"
  printf '  "platform": "%s",\n'         "$OS"
  printf '  "cpu_count": "%s",\n'        "$CPU_COUNT"
  printf '  "node_version": "%s",\n'     "$(node --version)"
  printf '  "puppeteer_version": "%s",\n' "$PUPPETEER_VERSION"
  printf '  "chromium_version": "%s",\n' "$CHROMIUM_VERSION"
  printf '  "container_memory_max_bytes": "%s",\n' "$CGROUP_MEM"
  printf '  "container_cpu_max": "%s"\n' "$CGROUP_CPU"
  echo "}"
} > "$OUT_DIR/host_meta.json"

# ---- case discovery -------------------------------------------------------

CASES=()
while IFS= read -r -d '' f; do
  CASES+=("$f")
done < <(
  find "$INPUT_DIR" -type f -name '*.html' ! -name '*.subset.html' -print0
)

if [[ ${#CASES[@]} -eq 0 ]]; then
  echo "run-cases.sh: no input .html cases under $INPUT_DIR" >&2
  exit 0
fi

# Sort the array in-shell (avoids relying on `sort -z`, which BSD
# sort on macOS doesn't ship).
IFS=$'\n' CASES=($(printf '%s\n' "${CASES[@]}" | LC_ALL=C sort)); unset IFS

echo "Found ${#CASES[@]} input case(s) under $INPUT_DIR"

# ---- run loop -------------------------------------------------------------

RESULTS="$OUT_DIR/results.jsonl"
: > "$RESULTS"

SAMPLER_FLAGS=()
if [[ "$COLLECT_CGROUP" == "1" ]]; then
  SAMPLER_FLAGS+=(--cgroup)
else
  SAMPLER_FLAGS+=(--no-cgroup)
fi

# ---- baseline runs --------------------------------------------------------
# Run the "browser opens about:blank" floor measurement first so subsequent
# rows are directly comparable. Goes through the same sampler wrapper as
# real cases, so the resulting JSONL row has the identical schema and
# slots cleanly into results.jsonl. summarize.js / summarize-html.js
# detect the `__baseline:blank/` label prefix and surface these rows in
# their own section above the aggregate stats.
#
# Caveat: cgroup memory.peak is monotonic on kernels < 6.5, so on those
# kernels the baseline's peak becomes the floor for every later case's
# `cgroup_memory_peak_delta_mb`. The proc-tree numbers stay per-case
# correct regardless. See README's "Gotchas" section.
BASELINE_RUNS=${BASELINE_RUNS:-1}
BASELINE_HOLD_MS=${BASELINE_HOLD_MS:-200}

if [[ "$BASELINE_RUNS" -gt 0 ]]; then
  echo "Running $BASELINE_RUNS baseline (browser opens about:blank) sample(s)"
  for i in $(seq 1 "$BASELINE_RUNS"); do
    label="__baseline:blank/$i"
    echo "-> $label"
    set +e
    node "$BENCH_DIR/sampler.js" \
      --label "$label" \
      --interval-ms "$INTERVAL_MS" \
      "${SAMPLER_FLAGS[@]}" \
      -- \
      node "$BENCH_DIR/baseline-blank.js" \
        --snapshot-dir "$SNAPSHOT_DIR" \
        --hold-ms "$BASELINE_HOLD_MS" \
      >> "$RESULTS"
    status=$?
    set -e
    if [[ $status -ne 0 ]]; then
      echo "   (baseline exited $status; row still recorded for diagnostics)"
    fi
  done
fi

start_all=$(date +%s)

for src in "${CASES[@]}"; do
  rel=${src#"$INPUT_DIR/"}
  out_sub="$OUT_DIR/$(dirname "$rel")"
  mkdir -p "$out_sub"
  out_file="$out_sub/$(basename "$rel" .html).subset.html"

  echo "-> $rel"

  # The sampler writes exactly one JSON line on its stdout (the
  # result row); the child's stdout/stderr go to fd 2 inside
  # sampler.js, so progress logs surface in the parent terminal /
  # docker logs while the JSONL stays clean. `set +e` keeps the loop
  # going on snapshot failures — the row is still recorded with the
  # non-zero exit code so it shows up in summary.md's Failures table.
  set +e
  node "$BENCH_DIR/sampler.js" \
    --label "$rel" \
    --interval-ms "$INTERVAL_MS" \
    "${SAMPLER_FLAGS[@]}" \
    -- \
    node "$SNAPSHOT_DIR/snapshot.js" "$src" -o "$out_file" \
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

node "$BENCH_DIR/summarize.js" "$RESULTS" "$OUT_DIR/host_meta.json" \
  > "$OUT_DIR/summary.md"
echo
echo "Wrote $RESULTS, $OUT_DIR/summary.md"
