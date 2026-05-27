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
#   - For `--engine playwright`: ensure `playwright` is installed
#     (`npm install playwright`, or rely on the optionalDependency
#     pulled in by the default `npm install`). On hosts without
#     distro chromium, run `npx playwright install chromium` once so
#     playwright has a browser to launch; or export
#     `PLAYWRIGHT_EXECUTABLE_PATH=/path/to/chromium` to reuse an
#     existing one.
#
# Usage:
#   tools/html-snapshot/bench/run-native.sh <input_dir> [output_dir]
#
# Defaults:
#   output_dir = tools/html-snapshot/bench/out/<label>
#                (mirrors the eval/ harness — every run gets its own
#                 sub-directory under bench/out/ so multiple corpora /
#                 iterations don't clobber each other.)
#   label      = current   (override with LABEL=... or --label NAME)
#
# Environment overrides:
#   LABEL              sub-directory name under bench/out (default: current)
#   BROWSER_ENGINE     puppeteer|playwright; which headless driver
#                      snapshot.js + baseline-blank.js launch under
#                      the sampler. Default: puppeteer.
#                      Also settable via `--engine NAME`.
#   INTERVAL_MS        sampler tick (ms)                  (default: 50)
#   BASELINE_RUNS      "browser opens about:blank" floor runs prepended
#                      to the case loop (default 1, set 0 to skip)
#   BASELINE_HOLD_MS   ms each baseline run holds the page open (default 200)
#
# Examples:
#   tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case
#   LABEL=ima-glm5.1 tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case
#   tools/html-snapshot/bench/run-native.sh --engine playwright ~/Desktop/tmp_case
#   BROWSER_ENGINE=playwright LABEL=pw-1 \
#     tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case
#
# Caveats vs. the Dockerised run-bench.sh:
#   - No cgroup memory accounting. On macOS cgroups don't exist; on
#     Linux native the global cgroup is contaminated by other host
#     processes, so the numbers would be misleading. The
#     `cgroup_memory_*` columns in results.jsonl come back null and
#     summary.md renders them as `n/a`.
#   - CPU is reported via the per-PID proc-tree fallback that sampler.js
#     computes from /proc/<pid>/stat (Linux native) or `ps -A -o time=`
#     (macOS), stored in `proc_tree_cpu_*`. The summary picks this up
#     transparently — the CPU columns and tiles now populate outside
#     Docker. Caveat: a process whose entire lifetime fits between two
#     sampler ticks is invisible to this stream; cgroup cpu.stat (Docker
#     mode) is still the holes-free source.
#   - macOS uses `ps -A` for the process-tree walk and reports VmRSS
#     in KB; PSS isn't exposed by macOS, so `peak_proc_tree_pss_mb`
#     is null. macOS `ps` also reports CPU as a single user+system
#     total, so `proc_tree_cpu_user_sec` / `proc_tree_cpu_system_sec`
#     are null on Darwin (only the combined `proc_tree_cpu_total_sec`
#     and the % field are populated). Linux native keeps the full
#     /proc + smaps_rollup path, so PSS and the user/system split are
#     still available.
#   - No `--cpus` / `--memory` envelope. The host runs as configured;
#     numbers reflect that, not a constrained container.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SNAPSHOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)

LABEL=${LABEL:-current}
BROWSER_ENGINE=${BROWSER_ENGINE:-puppeteer}
POSITIONAL=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --label)
      [[ $# -ge 2 ]] || { echo "--label requires a value" >&2; exit 2; }
      LABEL=$2
      shift 2
      ;;
    --label=*)
      LABEL=${1#--label=}
      shift
      ;;
    --engine|--browser-engine)
      [[ $# -ge 2 ]] || { echo "--engine requires a value" >&2; exit 2; }
      BROWSER_ENGINE=$2
      shift 2
      ;;
    --engine=*|--browser-engine=*)
      BROWSER_ENGINE=${1#*=}
      shift
      ;;
    -h|--help)
      sed -n '2,30p' "$0"
      exit 0
      ;;
    *)
      POSITIONAL+=("$1")
      shift
      ;;
  esac
done

case "$BROWSER_ENGINE" in
  puppeteer|playwright) ;;
  *)
    echo "unsupported --engine '$BROWSER_ENGINE' (expected puppeteer|playwright)" >&2
    exit 2
    ;;
esac

if [[ ${#POSITIONAL[@]} -lt 1 ]]; then
  echo "usage: $0 [--label NAME] [--engine puppeteer|playwright] <input_dir> [output_dir]" >&2
  exit 2
fi

INPUT_DIR=$(cd "${POSITIONAL[0]}" && pwd)
OUTPUT_DIR=${POSITIONAL[1]:-$SCRIPT_DIR/out/$LABEL}
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR=$(cd "$OUTPUT_DIR" && pwd)

INTERVAL_MS=${INTERVAL_MS:-50}

if [[ ! -d "$SNAPSHOT_DIR/node_modules/puppeteer" ]]; then
  echo "run-native.sh: puppeteer not installed under $SNAPSHOT_DIR." >&2
  echo "  run: (cd $SNAPSHOT_DIR && npm install)" >&2
  exit 2
fi

# Playwright is an optionalDependency, so `npm install` usually pulls
# it in, but we still surface a clear error when the user picks it
# explicitly and it's missing — rather than burning the whole case
# loop only to fail at the first sampler invocation. run-cases.sh
# performs the same check; doing it here too keeps the error close to
# the flag the user typed.
if [[ "$BROWSER_ENGINE" == "playwright" ]]; then
  if [[ ! -d "$SNAPSHOT_DIR/node_modules/playwright" ]]; then
    echo "run-native.sh: --engine playwright but playwright is not installed under $SNAPSHOT_DIR." >&2
    echo "  run: (cd $SNAPSHOT_DIR && npm install playwright)" >&2
    echo "  and, on a host without distro chromium, npx playwright install chromium" >&2
    exit 2
  fi
fi

echo "INPUT_DIR      = $INPUT_DIR"
echo "OUTPUT_DIR     = $OUTPUT_DIR"
echo "LABEL          = $LABEL"
echo "BROWSER_ENGINE = $BROWSER_ENGINE"
echo "SNAPSHOT_DIR   = $SNAPSHOT_DIR"
echo "BENCH_DIR      = $SCRIPT_DIR"
echo "INTERVAL_MS    = $INTERVAL_MS"
echo

# CGROUP=0 forces sampler.js's --no-cgroup; see run-cases.sh header
# for why we suppress cgroup reads outside the bench container.
env CGROUP=0 \
    BROWSER_ENGINE="$BROWSER_ENGINE" \
  "$SCRIPT_DIR/run-cases.sh" \
    "$INPUT_DIR" "$OUTPUT_DIR" "$SNAPSHOT_DIR" "$SCRIPT_DIR" "$INTERVAL_MS"

echo
echo "==> results: $OUTPUT_DIR/results.jsonl"
echo "==> summary: $OUTPUT_DIR/summary.md"
echo "==> report:  $OUTPUT_DIR/summary.html"
