#!/usr/bin/env bash
# run-bench.sh — host-side entrypoint for the Linux benchmark of
# tools/html-snapshot. Builds the image (if missing), then runs every
# non-subset HTML under $INPUT_DIR through `node snapshot.js` inside a
# fresh container, capturing peak CPU / memory.
#
# Usage:
#   tools/html-snapshot/bench/run-bench.sh <input_dir> [output_dir]
#
# Defaults:
#   output_dir = tools/html-snapshot/bench/out/<label>
#                (mirrors the eval/ harness — every run gets its own
#                 sub-directory under bench/out/ so multiple corpora /
#                 iterations don't clobber each other.)
#   label      = current   (override with LABEL=... or --label NAME)
#
# Environment overrides:
#   LABEL                 sub-directory name under bench/out (default: current)
#   IMAGE_NAME            docker image tag           (default: html-snapshot-bench:latest)
#   CONTAINER_CPUS        --cpus value for the run   (default: 4)
#   CONTAINER_MEMORY      --memory value for the run (default: 4g)
#   INTERVAL_MS           sampler tick (ms)          (default: 50)
#   REBUILD=1             force `docker build` even if image exists
#
# Examples:
#   tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case
#   LABEL=ima-glm5.1 tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case
#   CONTAINER_CPUS=2 CONTAINER_MEMORY=2g REBUILD=1 \
#     tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case /tmp/bench-out

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_CTX=$(cd "$SCRIPT_DIR/.." && pwd)

# Optional --label NAME flag. Anything else stays positional so the
# existing `<input_dir> [output_dir]` calling convention still works.
LABEL=${LABEL:-current}
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

if [[ ${#POSITIONAL[@]} -lt 1 ]]; then
  echo "usage: $0 [--label NAME] <input_dir> [output_dir]" >&2
  exit 2
fi

INPUT_DIR=$(cd "${POSITIONAL[0]}" && pwd)
OUTPUT_DIR=${POSITIONAL[1]:-$SCRIPT_DIR/out/$LABEL}
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR=$(cd "$OUTPUT_DIR" && pwd)

IMAGE_NAME=${IMAGE_NAME:-html-snapshot-bench:latest}
CONTAINER_CPUS=${CONTAINER_CPUS:-4}
CONTAINER_MEMORY=${CONTAINER_MEMORY:-4g}
INTERVAL_MS=${INTERVAL_MS:-50}

echo "INPUT_DIR        = $INPUT_DIR"
echo "OUTPUT_DIR       = $OUTPUT_DIR"
echo "LABEL            = $LABEL"
echo "IMAGE_NAME       = $IMAGE_NAME"
echo "BUILD_CTX        = $BUILD_CTX"
echo "CONTAINER_CPUS   = $CONTAINER_CPUS"
echo "CONTAINER_MEMORY = $CONTAINER_MEMORY"
echo "INTERVAL_MS      = $INTERVAL_MS"
echo

# ---- build image ----------------------------------------------------------

need_build=${REBUILD:-0}
if [[ "$need_build" != "1" ]]; then
  if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    need_build=1
  fi
fi

if [[ "$need_build" == "1" ]]; then
  echo "==> docker build $IMAGE_NAME"
  docker build \
    -f "$SCRIPT_DIR/Dockerfile" \
    -t "$IMAGE_NAME" \
    "$BUILD_CTX"
fi

# ---- run -----------------------------------------------------------------

echo
echo "==> docker run $IMAGE_NAME"
# --init       reaps zombie chromium helper procs (puppeteer warns
#              without it on Linux).
# --cap-add SYS_PTRACE
#              required for /proc/<pid>/smaps_rollup PSS readings to be
#              non-zero across processes our UID didn't spawn. Inside
#              the container we run as root so this is just belt-and-
#              suspenders; if the image is ever switched to a non-root
#              user, this flag becomes necessary.
# --security-opt seccomp=unconfined
#              Chromium's sandbox needs CLONE_NEWUSER and friends.
#              `--no-sandbox` (already passed by snapshot.js? — no, it
#              isn't; see note in this script) is the alternative; we
#              loosen seccomp instead so the headless shell behaves
#              like it would on a CI runner that bind-mounts the host
#              kernel.
docker run --rm \
  --init \
  --cpus "$CONTAINER_CPUS" \
  --memory "$CONTAINER_MEMORY" \
  --cap-add SYS_PTRACE \
  --security-opt seccomp=unconfined \
  -v "$INPUT_DIR":/inputs:ro \
  -v "$OUTPUT_DIR":/out \
  "$IMAGE_NAME" \
  /bench/run-in-container.sh /inputs /out "$INTERVAL_MS"

echo
echo "==> results: $OUTPUT_DIR/results.jsonl"
echo "==> summary: $OUTPUT_DIR/summary.md"
