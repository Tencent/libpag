#!/usr/bin/env bash
# Phase 15 (docs/pagx_to_pag_v2_design.md §18.13) — clang source-based
# coverage driver for PAGX→PAG v2. Builds a dedicated cmake-build-coverage
# with -fprofile-instr-generate -fcoverage-mapping, runs PAGFullTest plus a
# short fuzz-seed pass over PAGDecodeFuzz / PAGInflaterFuzz, merges the
# resulting .profraw files, renders HTML into coverage/html/, and enforces
# the 85% line-coverage gate on src/pagx/ + include/pagx/.
#
# Requires xcrun llvm-profdata / llvm-cov (Xcode command line tools) or
# Homebrew llvm on PATH. Fails loudly on any error — no silent downgrade.
set -euo pipefail

cd "$(dirname "$0")/.."
REPO_ROOT="$(pwd)"

BUILD_DIR="cmake-build-coverage"
OUT_DIR="coverage"
HTML_DIR="${OUT_DIR}/html"
PROFRAW_DIR="${OUT_DIR}/profraw"
MERGED_PROFDATA="${OUT_DIR}/merged.profdata"
THRESHOLD_PCT=85

# --- 1. Resolve llvm toolchain ---------------------------------------------
if command -v llvm-profdata >/dev/null 2>&1 && command -v llvm-cov >/dev/null 2>&1; then
    LLVM_PROFDATA="$(command -v llvm-profdata)"
    LLVM_COV="$(command -v llvm-cov)"
elif command -v xcrun >/dev/null 2>&1; then
    LLVM_PROFDATA="$(xcrun -f llvm-profdata)"
    LLVM_COV="$(xcrun -f llvm-cov)"
else
    echo "error: neither llvm-profdata/llvm-cov nor xcrun is on PATH." >&2
    echo "       install Xcode command-line tools or Homebrew llvm." >&2
    exit 2
fi
echo "==> llvm-profdata: ${LLVM_PROFDATA}"
echo "==> llvm-cov:      ${LLVM_COV}"

# --- 2. Configure + build ---------------------------------------------------
echo "==> configuring ${BUILD_DIR} (Debug + coverage + fuzz)"
cmake -G Ninja \
      -DPAG_BUILD_TESTS=ON \
      -DPAG_BUILD_FUZZ=ON \
      -DPAG_COVERAGE=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -B "${BUILD_DIR}"

echo "==> building targets: PAGFullTest PAGDecodeFuzz PAGInflaterFuzz"
cmake --build "${BUILD_DIR}" --target PAGFullTest PAGDecodeFuzz PAGInflaterFuzz

# --- 3. Reset profile output directory -------------------------------------
rm -rf "${OUT_DIR}"
mkdir -p "${PROFRAW_DIR}"

# --- 4. Run binaries with per-pid profraw output ---------------------------
# %p in LLVM_PROFILE_FILE expands to the binary's PID so concurrent /
# sequential runs never overwrite each other's counters.
FULL_TEST_BIN="${BUILD_DIR}/PAGFullTest"
DECODE_FUZZ_BIN="${BUILD_DIR}/PAGDecodeFuzz"
INFLATER_FUZZ_BIN="${BUILD_DIR}/PAGInflaterFuzz"

for bin in "${FULL_TEST_BIN}" "${DECODE_FUZZ_BIN}" "${INFLATER_FUZZ_BIN}"; do
    if [ ! -x "${bin}" ]; then
        echo "error: missing binary ${bin}" >&2
        exit 3
    fi
done

echo "==> running PAGFullTest"
LLVM_PROFILE_FILE="${PROFRAW_DIR}/full-%p.profraw" "${FULL_TEST_BIN}"

echo "==> running PAGDecodeFuzz over test/fuzz_corpus/decode_seeds"
LLVM_PROFILE_FILE="${PROFRAW_DIR}/decode-%p.profraw" \
    "${DECODE_FUZZ_BIN}" test/fuzz_corpus/decode_seeds

echo "==> running PAGInflaterFuzz over test/fuzz_corpus/inflater_seeds"
# inflater_seeds is currently empty (Phase 12 left it as a placeholder);
# the standalone runner just reports "0 seeds" and exits cleanly, which
# still contributes entry-point coverage for the harness itself.
LLVM_PROFILE_FILE="${PROFRAW_DIR}/inflater-%p.profraw" \
    "${INFLATER_FUZZ_BIN}" test/fuzz_corpus/inflater_seeds

# --- 5. Merge profraw -> profdata ------------------------------------------
echo "==> merging .profraw into ${MERGED_PROFDATA}"
"${LLVM_PROFDATA}" merge -sparse \
    "${PROFRAW_DIR}"/*.profraw \
    -o "${MERGED_PROFDATA}"

# --- 6. Shared llvm-cov source filters -------------------------------------
# Restrict the report to new PAGX→PAG v2 code. Everything under
# src/pagx/ and include/pagx/ is in scope; libpag v1 code is out of
# scope per the Phase 15 product selection.
SOURCE_FILTERS=(
    "${REPO_ROOT}/src/pagx"
    "${REPO_ROOT}/include/pagx"
)

OBJECTS=(
    "${FULL_TEST_BIN}"
    -object "${DECODE_FUZZ_BIN}"
    -object "${INFLATER_FUZZ_BIN}"
)

# --- 7. Render HTML ---------------------------------------------------------
echo "==> rendering HTML into ${HTML_DIR}/"
"${LLVM_COV}" show "${OBJECTS[@]}" \
    -instr-profile="${MERGED_PROFDATA}" \
    -format=html \
    -output-dir="${HTML_DIR}" \
    -show-branches=count \
    -show-line-counts-or-regions \
    "${SOURCE_FILTERS[@]}"

# --- 8. Print + parse summary ----------------------------------------------
echo ""
echo "==================== COVERAGE SUMMARY (src/pagx + include/pagx) ===================="
REPORT_TXT="${OUT_DIR}/report.txt"
"${LLVM_COV}" report "${OBJECTS[@]}" \
    -instr-profile="${MERGED_PROFDATA}" \
    "${SOURCE_FILTERS[@]}" \
    | tee "${REPORT_TXT}"
echo "===================================================================================="

# Parse the TOTAL line. llvm-cov report's TOTAL row layout (clang 15+):
#   TOTAL  <regions> <missed> <cover%>  <funcs> <missed> <cover%>  <lines> <missed> <cover%>  <branches> <missed> <cover%>
# We want the *line* coverage column (column 10 after TOTAL token).
TOTAL_LINE="$(grep -E '^TOTAL' "${REPORT_TXT}" || true)"
if [ -z "${TOTAL_LINE}" ]; then
    echo "error: could not locate TOTAL row in coverage report." >&2
    exit 4
fi

# Extract percentages (e.g. "87.23%"), pick the 3rd one = line coverage.
# Column order in llvm-cov report: regions%, functions%, lines%, branches%.
PCTS=($(echo "${TOTAL_LINE}" | grep -oE '[0-9]+\.[0-9]+%' | tr -d '%'))
if [ "${#PCTS[@]}" -lt 3 ]; then
    echo "error: unexpected TOTAL row layout: ${TOTAL_LINE}" >&2
    exit 5
fi
LINE_PCT="${PCTS[2]}"

echo ""
echo "line coverage: ${LINE_PCT}%  (threshold ${THRESHOLD_PCT}%)"
echo "HTML report:   ${REPO_ROOT}/${HTML_DIR}/index.html"

# Float compare via awk (bash can't do it natively).
if awk -v a="${LINE_PCT}" -v b="${THRESHOLD_PCT}" 'BEGIN { exit !(a + 0 >= b + 0) }'; then
    echo "PASS: line coverage meets the ${THRESHOLD_PCT}% gate."
    exit 0
else
    echo "FAIL: line coverage below ${THRESHOLD_PCT}% gate." >&2
    exit 1
fi
