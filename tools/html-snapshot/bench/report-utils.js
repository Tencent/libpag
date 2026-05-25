'use strict';
//
// report-utils.js — shared helpers for summarize.js (Markdown) and
// summarize-html.js (HTML). Keeps the two reports byte-equivalent in
// numeric output by funneling all stats / formatting through one
// implementation.
//
// Public surface (CommonJS, deliberately minimal):
//   - loadReport(resultsPath, hostMetaPath?) → { rows, hostMeta, ok, fail }
//   - percentile(arr, p) → number | null
//   - stat(field, src) → { min, p50, p95, max, mean } | { all-null }
//   - fmt(v, suffix?) → string  (n/a-aware, integer >= 1000)
//   - topBy(rows, field, n)     → rows sorted desc by field, top-n only
//   - cpuTotalSec(row)          → user+sys cgroup CPU (0-default)
//
// All numeric helpers ignore non-number entries (`null`, `undefined`,
// strings) so the same code path works for both fully-populated Docker
// runs and reduced native runs where cgroup_* / pss are null.

const fs = require('node:fs');

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

  const ok = rows.filter((r) => r.exit_code === 0);
  const fail = rows.filter((r) => r.exit_code !== 0);
  return { rows, hostMeta, ok, fail };
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
function statValues(vals) {
  vals = vals.filter((v) => typeof v === 'number');
  if (!vals.length) {
    return { min: null, p50: null, p95: null, max: null, mean: null };
  }
  const sum = vals.reduce((a, b) => a + b, 0);
  return {
    min: minOf(vals),
    p50: percentile(vals, 0.5),
    p95: percentile(vals, 0.95),
    max: maxOf(vals),
    mean: +(sum / vals.length).toFixed(2),
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
  return (row.cgroup_cpu_user_sec || 0) + (row.cgroup_cpu_system_sec || 0);
}

module.exports = {
  loadReport,
  percentile,
  stat,
  statValues,
  fmt,
  topBy,
  cpuTotalSec,
};
