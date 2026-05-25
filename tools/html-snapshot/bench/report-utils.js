'use strict';
//
// report-utils.js — shared helpers for summarize.js (Markdown) and
// summarize-html.js (HTML). Keeps the two reports byte-equivalent in
// numeric output by funneling all stats / formatting through one
// implementation.
//
// Public surface (CommonJS, deliberately minimal):
//   - loadReport(resultsPath, hostMetaPath?)
//       → { rows, hostMeta, ok, fail, baseline, cases }
//       where `ok` / `fail` partition the **non-baseline** case rows.
//       `baseline` holds the floor-cost rows produced by
//       `bench/baseline-blank.js`; they're filtered out of `ok`/`fail`
//       so they don't pollute aggregate p50/p95 stats.
//   - isBaselineRow(row) → boolean
//   - percentile(arr, p) → number | null
//   - stat(field, src) → { min, p50, p95, max, mean } | { all-null }
//   - fmt(v, suffix?) → string  (n/a-aware, integer >= 1000)
//   - topBy(rows, field, n)     → rows sorted desc by field, top-n only
//   - cpuTotalSec(row)          → user+sys cgroup CPU; null if both
//                                  inputs are null (e.g. native macOS
//                                  has no cgroups, so reporting 0
//                                  would falsely imply "measured zero
//                                  CPU" rather than "not measured")
//
// All numeric helpers ignore non-number entries (`null`, `undefined`,
// strings) so the same code path works for both fully-populated Docker
// runs and reduced native runs where cgroup_* / pss are null.

const fs = require('node:fs');

// Rows produced by bench/baseline-blank.js carry this label prefix so
// report consumers can split them off from real cases without an
// out-of-band channel. The full label is `__baseline:blank/<n>` (n = 1
// for the default single-run mode); we match on the `__baseline:`
// prefix to leave room for future baseline scenarios (e.g.
// `__baseline:warm-blank/1` once we add a warm-browser path).
const BASELINE_LABEL_PREFIX = '__baseline:';

function isBaselineRow(row) {
  return !!row && typeof row.label === 'string'
    && row.label.startsWith(BASELINE_LABEL_PREFIX);
}

function loadReport(resultsPath, hostMetaPath) {
  if (!resultsPath) {
    throw new Error('loadReport: resultsPath is required');
  }
  const rows = fs
    .readFileSync(resultsPath, 'utf8')
    .split('\n')
    .map((s) => s.trim())
    .filter(Boolean)
    .map((s) => JSON.parse(s));

  let hostMeta = {};
  if (hostMetaPath) {
    try {
      hostMeta = JSON.parse(fs.readFileSync(hostMetaPath, 'utf8'));
    } catch {
      // Tolerate a missing or malformed host_meta.json — the report
      // still renders, the Environment section just shows 'n/a'.
    }
  }

  const baseline = rows.filter(isBaselineRow);
  const cases = rows.filter((r) => !isBaselineRow(r));
  // ok / fail intentionally exclude baseline rows so existing callers
  // that do `stat('wall_ms', ok)` keep computing per-case statistics
  // without picking up the (much cheaper) baseline samples.
  const ok = cases.filter((r) => r.exit_code === 0);
  const fail = cases.filter((r) => r.exit_code !== 0);
  return { rows, hostMeta, ok, fail, baseline, cases };
}

function percentile(arr, p) {
  if (!arr.length) return null;
  const sorted = arr.slice().sort((a, b) => a - b);
  const idx = Math.min(sorted.length - 1, Math.floor(p * sorted.length));
  return sorted[idx];
}

// Use reduce-based min/max instead of Math.min(...vals) / Math.max(...vals)
// so we don't blow the call stack on large case sets (V8 spreads
// arguments to the platform stack; ~100k+ values is enough to RangeError).
function minOf(vals) {
  let m = vals[0];
  for (let i = 1; i < vals.length; i++) if (vals[i] < m) m = vals[i];
  return m;
}
function maxOf(vals) {
  let m = vals[0];
  for (let i = 1; i < vals.length; i++) if (vals[i] > m) m = vals[i];
  return m;
}

function stat(field, src) {
  const vals = src.map((r) => r[field]).filter((v) => typeof v === 'number');
  return statValues(vals);
}

// Same as stat() but takes an already-extracted array of numbers.
// Useful for derived fields (e.g. user+sys CPU total per case) where
// dressing them up as fake records just to pass through stat() is
// noisy.
//
// Implementation note: we sort once and read min/p50/p95/max off the
// sorted array, instead of calling minOf + percentile(0.5) + percentile(0.95)
// + maxOf which would scan the array four times (and sort it twice via
// percentile()). For corpora with thousands of cases this halves the
// per-run sort cost.
function statValues(vals) {
  vals = vals.filter((v) => typeof v === 'number');
  if (!vals.length) {
    return { min: null, p50: null, p95: null, max: null, mean: null };
  }
  const sorted = vals.slice().sort((a, b) => a - b);
  const n = sorted.length;
  let sum = 0;
  for (let i = 0; i < n; i++) sum += sorted[i];
  const pickPct = (p) => sorted[Math.min(n - 1, Math.floor(p * n))];
  return {
    min: sorted[0],
    p50: pickPct(0.5),
    p95: pickPct(0.95),
    max: sorted[n - 1],
    mean: +(sum / n).toFixed(2),
  };
}

function fmt(v, suffix = '') {
  if (v == null) return 'n/a';
  if (typeof v === 'number') {
    return (v >= 1000 ? v.toFixed(0) : v.toFixed(1)) + suffix;
  }
  return String(v) + suffix;
}

function topBy(rows, field, n) {
  return rows
    .slice()
    .sort((a, b) => (b[field] || 0) - (a[field] || 0))
    .slice(0, n);
}

function cpuTotalSec(row) {
  const u = row.cgroup_cpu_user_sec;
  const s = row.cgroup_cpu_system_sec;
  // Both unmeasured → return null so downstream stat()/fmt() show
  // 'n/a' instead of a misleading "0.0s" tile or table cell.
  if (u == null && s == null) return null;
  return (u || 0) + (s || 0);
}

module.exports = {
  BASELINE_LABEL_PREFIX,
  isBaselineRow,
  loadReport,
  percentile,
  stat,
  statValues,
  fmt,
  topBy,
  cpuTotalSec,
  minOf,
  maxOf,
};
