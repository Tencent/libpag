#!/usr/bin/env bash
#
# run_html_anim_eval.sh — per-case animation-fidelity gate.
#
# Runs tools/html-snapshot/eval-animation over the nine `resources/html/animations/css`
# cases that pin the pivot / 3D animation contract (43–51) and gates every case's
# `ssimMean` against resources/html/animations/css/expected.json: a score more than
# `tolerance` below its ledger value fails the run, a score above it is reported as an
# improvement (review it, then update the ledger).
#
# Per-case gating is deliberate here, unlike the corpus-mean gate in
# test/run_html_eval.sh: each of these nine cases exists to lock one specific semantic,
# so a drop on a single one is a regression rather than corpus noise.
#
# Prerequisites: a built pagx binary (default <repo>/cmake-build-debug/pagx, or $PAGX_BIN),
# Node.js, and a Chromium build Playwright can launch. The toolchain (npm deps + dist/ +
# Chromium) is bootstrapped on every run; pass SKIP_BOOTSTRAP=1 to reuse what is installed.
#
# Usage:
#   test/run_html_anim_eval.sh                          # gate the nine cases
#   PAGX_BIN=/path/to/pagx test/run_html_anim_eval.sh
#   SKIP_BOOTSTRAP=1 test/run_html_anim_eval.sh         # reuse dist/ + node_modules
#   CONCURRENCY=1 BROWSER_ENGINE=puppeteer test/run_html_anim_eval.sh
#   HTML_ANIM_EVAL=1 test/run_html_eval.sh              # the same gate from the static driver
#
set -u
set -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SNAPSHOT_DIR="$ROOT/tools/html-snapshot"
ANIM_DIR="$ROOT/resources/html/animations/css"
EXPECTED="$ANIM_DIR/expected.json"
PAGX_BIN="${PAGX_BIN:-$ROOT/cmake-build-debug/pagx}"
# Cases run in parallel; a headless engine handles the browser work.
CONCURRENCY="${CONCURRENCY:-4}"
BROWSER_ENGINE="${BROWSER_ENGINE:-playwright}"
SKIP_BOOTSTRAP="${SKIP_BOOTSTRAP:-0}"
LABEL="${LABEL:-anim-gate}"
OUT="$SNAPSHOT_DIR/eval-animation/out/$LABEL"
REPORT="$OUT/report.csv"

# --- prerequisites ---------------------------------------------------------

if [ ! -x "$PAGX_BIN" ]; then
  echo "run_html_anim_eval: pagx binary not found or not executable: $PAGX_BIN" >&2
  echo "run_html_anim_eval: build it first, e.g. cmake --build cmake-build-debug --target pagx" >&2
  exit 1
fi
# run.js reads $PAGX_BIN, so export it instead of passing --pagx-bin.
export PAGX_BIN

if ! command -v node >/dev/null 2>&1; then
  echo "run_html_anim_eval: node is required but was not found on PATH" >&2
  exit 1
fi

if [ ! -f "$EXPECTED" ]; then
  echo "run_html_anim_eval: expected baseline ledger missing: $EXPECTED" >&2
  exit 1
fi

# --- toolchain bootstrap ---------------------------------------------------

if [ "$SKIP_BOOTSTRAP" = "0" ]; then
  echo "run_html_anim_eval: installing html-snapshot dependencies (npm install)..."
  (cd "$SNAPSHOT_DIR" && npm install) || {
    echo "run_html_anim_eval: npm install failed" >&2
    exit 1
  }
  if [ "$BROWSER_ENGINE" = "playwright" ]; then
    echo "run_html_anim_eval: installing Playwright Chromium..."
    (cd "$SNAPSHOT_DIR" && npx --no-install playwright install chromium) || {
      echo "run_html_anim_eval: Playwright Chromium installation failed" >&2
      exit 1
    }
  fi
  echo "run_html_anim_eval: building html-snapshot (npm run build)..."
  (cd "$SNAPSHOT_DIR" && npm run build) || {
    echo "run_html_anim_eval: npm run build failed" >&2
    exit 1
  }
fi

# --- run -------------------------------------------------------------------

# The ledger is the single source of truth for the case set, so a case cannot silently
# fall out of the gate through a stale copy of the list in this script.
declare -a ONLY_ARGS=()
while IFS= read -r case_name; do
  if [ -n "$case_name" ]; then
    ONLY_ARGS+=(--only "$case_name")
  fi
done < <(node -e 'const fs=require("fs");const j=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));for(const c of j.cases){console.log(c.case);}' "$EXPECTED")

if [ "${#ONLY_ARGS[@]}" -eq 0 ]; then
  echo "run_html_anim_eval: no cases listed in $EXPECTED" >&2
  exit 1
fi

# Do not let a report left by an older run stand in for this one.
rm -f "$REPORT"

echo ""
echo "=== Animation eval: $(basename "$ANIM_DIR")  [concurrency=$CONCURRENCY engine=$BROWSER_ENGINE] ==="
if ! "$SNAPSHOT_DIR/eval-animation/run.sh" --corpus "$ANIM_DIR" --label "$LABEL" \
           --concurrency "$CONCURRENCY" --browser-engine "$BROWSER_ENGINE" "${ONLY_ARGS[@]}"; then
  echo "run_html_anim_eval: eval run failed" >&2
  exit 1
fi
if [ ! -f "$REPORT" ]; then
  echo "run_html_anim_eval: eval finished without a report: $REPORT" >&2
  exit 1
fi

# --- gate ------------------------------------------------------------------

echo ""
echo "=== Animation gate: report.csv vs expected.json ==="
if node - "$EXPECTED" "$REPORT" <<'GATE'
'use strict';

const fs = require('fs');

const [expectedPath, reportPath] = process.argv.slice(2);
const expected = JSON.parse(fs.readFileSync(expectedPath, 'utf8'));
const lines = fs.readFileSync(reportPath, 'utf8').trim().split(/\r?\n/);
const header = lines[0].split(',');
const nameAt = header.indexOf('name');
const ssimAt = header.indexOf('ssimMean');
const errorAt = header.indexOf('error');
if (nameAt < 0 || ssimAt < 0) {
  console.log(`FAIL report.csv is missing the name / ssimMean columns: ${header.join(',')}`);
  process.exit(1);
}

// `camSkipReasons` may itself contain commas, so only the leading columns are split off
// and the (last) error column is re-joined from the remainder.
const rows = new Map();
for (const line of lines.slice(1)) {
  const cells = line.split(',');
  const error = errorAt < 0 ? '' : (errorAt === header.length - 1 ? cells.slice(errorAt).join(',')
                                                                  : cells[errorAt] || '');
  rows.set(cells[nameAt], { ssim: Number(cells[ssimAt]), error: error || '' });
}

let failures = 0;
let improved = 0;
for (const entry of expected.cases) {
  const row = rows.get(entry.case);
  if (row === undefined) {
    console.log(`FAIL ${entry.case}: missing from report.csv`);
    failures++;
    continue;
  }
  if (row.error !== '') {
    console.log(`FAIL ${entry.case}: eval error: ${row.error}`);
    failures++;
    continue;
  }
  if (!Number.isFinite(row.ssim)) {
    console.log(`FAIL ${entry.case}: ssimMean is not a number`);
    failures++;
    continue;
  }
  const delta = row.ssim - entry.ssim;
  if (row.ssim < entry.ssim - entry.tolerance) {
    console.log(`FAIL ${entry.case}: ssimMean ${row.ssim.toFixed(5)} < baseline ` +
                `${entry.ssim.toFixed(5)} - tolerance ${entry.tolerance} (diff ${delta.toFixed(5)})`);
    failures++;
  } else if (delta > entry.tolerance) {
    console.log(`NOTE ${entry.case}: ssimMean ${row.ssim.toFixed(5)} is ${delta.toFixed(5)} above the ` +
                `ledger; if the change is intended, raise its "ssim" in expected.json`);
    improved++;
  } else {
    console.log(`ok   ${entry.case}: ${row.ssim.toFixed(5)} ` +
                `(baseline ${entry.ssim.toFixed(5)} +- ${entry.tolerance})`);
  }
}

console.log('');
console.log(`${expected.cases.length} case(s): ${failures} failed, ${improved} improved.`);
process.exit(failures === 0 ? 0 : 1);
GATE
then
  echo "run_html_anim_eval: all cases within tolerance"
  echo "run_html_anim_eval: report: $OUT/report.md"
  exit 0
else
  echo "run_html_anim_eval: animation fidelity regressed (see the FAIL lines above)" >&2
  echo "run_html_anim_eval: report: $OUT/report.md" >&2
  exit 1
fi
