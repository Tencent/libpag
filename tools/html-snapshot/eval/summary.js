#!/usr/bin/env node
'use strict';

/**
 * summary.js — aggregate multiple eval runs into one cross-corpus summary.
 *
 * Reads each `<out>/<label>/report.csv` produced by run.js, computes per-corpus
 * and overall means, prints a table to the console, and writes a combined
 * `<out>/summary.html` linking to each corpus's own index.html viewer.
 *
 * Usage:
 *   node summary.js [--out <dir>] [--title <t>] <label> [<label> ...]
 *
 * Example:
 *   node summary.js --out out html-cases html-cli html-websites html-generated
 */

const fs = require('fs');
const path = require('path');

function parseArgs(argv) {
  const opts = {
    outDir: path.join(__dirname, 'out'),
    title: 'HTML eval summary',
    labels: [],
    baseline: '',
    updateBaseline: false,
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--out') opts.outDir = argv[++i];
    else if (a === '--title') opts.title = argv[++i];
    else if (a === '--baseline') opts.baseline = argv[++i];
    else if (a === '--update-baseline') opts.updateBaseline = true;
    else if (a === '-h' || a === '--help') {
      console.log(
        'Usage: node summary.js [--out <dir>] [--title <t>] [--baseline <file>] [--update-baseline] <label> [<label> ...]'
      );
      process.exit(0);
    } else opts.labels.push(a);
  }
  return opts;
}

// Minimal RFC-4180-ish CSV parser (handles quoted fields with commas/quotes).
function parseCsv(text) {
  const rows = [];
  let row = [];
  let field = '';
  let inQuotes = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (inQuotes) {
      if (c === '"') {
        if (text[i + 1] === '"') { field += '"'; i++; }
        else inQuotes = false;
      } else field += c;
    } else if (c === '"') inQuotes = true;
    else if (c === ',') { row.push(field); field = ''; }
    else if (c === '\n') { row.push(field); rows.push(row); row = []; field = ''; }
    else if (c === '\r') { /* skip */ }
    else field += c;
  }
  if (field.length || row.length) { row.push(field); rows.push(row); }
  return rows;
}

function readRows(csvPath) {
  const raw = fs.readFileSync(csvPath, 'utf8');
  const table = parseCsv(raw).filter((r) => r.length > 1 || (r.length === 1 && r[0] !== ''));
  if (!table.length) return [];
  const headers = table[0];
  return table.slice(1).map((cols) => {
    const o = {};
    headers.forEach((h, i) => { o[h] = cols[i] !== undefined ? cols[i] : ''; });
    return o;
  });
}

const num = (v) => { const n = parseFloat(v); return Number.isFinite(n) ? n : NaN; };
const mean = (arr) => { const v = arr.filter(Number.isFinite); return v.length ? v.reduce((a, b) => a + b, 0) / v.length : NaN; };
const median = (arr) => {
  const v = arr.filter(Number.isFinite).sort((a, b) => a - b);
  if (!v.length) return NaN;
  const m = Math.floor(v.length / 2);
  return v.length % 2 ? v[m] : (v[m - 1] + v[m]) / 2;
};

function summarize(label, rows) {
  const ok = rows.filter((r) => !r.error);
  const ssim = ok.map((r) => num(r.ssim));
  const pd = ok.map((r) => num(r.pixelDiffRatio));
  const rgb = ok.map((r) => num(r.meanRgbDelta));
  const warn = rows.map((r) => num(r.importerWarnings)).filter(Number.isFinite);
  return {
    label,
    cases: rows.length,
    ok: ok.length,
    errored: rows.length - ok.length,
    ssimMean: mean(ssim),
    ssimMedian: median(ssim),
    pdMean: mean(pd),
    rgbMean: mean(rgb),
    warnSum: warn.reduce((a, b) => a + b, 0),
    ssimLow: ssim.filter((v) => Number.isFinite(v) && v < 0.3).length,
    ssimMid: ssim.filter((v) => Number.isFinite(v) && v >= 0.3 && v < 0.7).length,
    ssimHigh: ssim.filter((v) => Number.isFinite(v) && v >= 0.7).length,
  };
}

const f = (v, d = 4) => (Number.isFinite(v) ? v.toFixed(d) : '-');
const pct = (v, d = 2) => (Number.isFinite(v) ? (v * 100).toFixed(d) + '%' : '-');

function printConsole(summaries, overall) {
  const cols = [
    ['corpus', 22, (s) => s.label],
    ['cases', 6, (s) => String(s.cases)],
    ['ok', 5, (s) => String(s.ok)],
    ['err', 4, (s) => String(s.errored)],
    ['ssim', 7, (s) => f(s.ssimMean)],
    ['pxDiff', 8, (s) => pct(s.pdMean)],
    ['rgbΔ', 7, (s) => f(s.rgbMean, 2)],
    ['<.3', 5, (s) => String(s.ssimLow)],
    ['.3-.7', 6, (s) => String(s.ssimMid)],
    ['≥.7', 5, (s) => String(s.ssimHigh)],
    ['warn', 6, (s) => String(s.warnSum)],
  ];
  const pad = (str, w) => {
    // account for wide chars (Δ, ≥) counting as width 1 visually
    const s = String(str);
    return s.length >= w ? s : s + ' '.repeat(w - s.length);
  };
  const line = (s) => cols.map(([, w, get]) => pad(get(s), w)).join('  ');
  const header = cols.map(([h, w]) => pad(h, w)).join('  ');
  console.log('');
  console.log('=== HTML eval summary (report-only, no pass/fail gate) ===');
  console.log(header);
  console.log('-'.repeat(header.length));
  for (const s of summaries) console.log(line(s));
  console.log('-'.repeat(header.length));
  console.log(line(overall));
  console.log('');
}

function renderHtml(summaries, overall, title, entries) {
  const esc = (s) => String(s).replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
  const rowHtml = (s, isOverall, href) => `
    <tr class="${isOverall ? 'overall' : ''}">
      <td class="name">${href ? `<a href="${esc(href)}">${esc(s.label)}</a>` : esc(s.label)}</td>
      <td>${s.cases}</td><td>${s.ok}</td><td>${s.errored}</td>
      <td><b>${f(s.ssimMean)}</b></td><td>${f(s.ssimMedian)}</td>
      <td>${pct(s.pdMean)}</td><td>${f(s.rgbMean, 2)}</td>
      <td>${s.ssimLow}</td><td>${s.ssimMid}</td><td>${s.ssimHigh}</td>
      <td>${s.warnSum}</td>
    </tr>`;
  const body = summaries.map((s, i) => rowHtml(s, false, entries[i] && entries[i].indexHref)).join('');
  return `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>${esc(title)}</title>
<style>
  :root { color-scheme: light dark; }
  body { font: 14px/1.5 -apple-system, Segoe UI, Roboto, sans-serif; margin: 24px; }
  h1 { font-size: 20px; }
  .meta { color: #888; margin-bottom: 16px; }
  table { border-collapse: collapse; width: 100%; }
  th, td { padding: 6px 10px; text-align: right; border-bottom: 1px solid rgba(128,128,128,.3); }
  th:first-child, td.name { text-align: left; }
  tr.overall td { border-top: 2px solid rgba(128,128,128,.6); font-weight: 600; }
  a { color: inherit; }
</style></head>
<body>
  <h1>${esc(title)}</h1>
  <div class="meta">${esc(new Date().toISOString())} &middot; ${summaries.length} corpora &middot; ${overall.cases} cases total. Report-only; inspect per-corpus viewers for details.</div>
  <table>
    <thead><tr>
      <th>corpus</th><th>cases</th><th>ok</th><th>err</th>
      <th>ssim mean</th><th>ssim median</th><th>pixelDiff mean</th><th>rgbΔ mean</th>
      <th>ssim&lt;0.3</th><th>0.3–0.7</th><th>≥0.7</th><th>warn</th>
    </tr></thead>
    <tbody>
      ${body}
      ${rowHtml(overall, true, null)}
    </tbody>
  </table>
</body></html>
`;
}

// --- corpus-level mean baseline gate ---------------------------------------
//
// Browser fidelity is a corpus-level mean metric, so the gate compares this
// run's per-corpus means against a committed baseline instead of doing any
// per-case pixel comparison (baselines are Chromium-rendered at run time and
// therefore not deterministic per case). A corpus regresses when its SSIM mean
// drops, or its pixel-diff / RGB-delta mean rises, beyond the tolerance.

// Default tolerances per corpus. websites/generated pull CDN CSS/fonts/images
// at eval time, so they are inherently noisier and get looser bounds.
const DEFAULT_TOLERANCE = { ssim: 0.02, pd: 0.02, rgb: 2.0 };
const LOOSE_TOLERANCE = { ssim: 0.05, pd: 0.05, rgb: 5.0 };
const CORPUS_TOLERANCE = {
  'html-cases': DEFAULT_TOLERANCE,
  'html-cli': DEFAULT_TOLERANCE,
  'html-websites': LOOSE_TOLERANCE,
  'html-generated': LOOSE_TOLERANCE,
};

function loadBaseline(file) {
  if (!file || !fs.existsSync(file)) return null;
  try {
    return JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch (e) {
    console.error(`summary: failed to parse baseline ${file}: ${e.message}`);
    return null;
  }
}

function toleranceFor(baseline, label) {
  const perCorpus = baseline && baseline.corpora && baseline.corpora[label];
  if (perCorpus && perCorpus.tolerance) return perCorpus.tolerance;
  return CORPUS_TOLERANCE[label] || DEFAULT_TOLERANCE;
}

// Returns { ok, checks: [...] } comparing a corpus summary against its baseline
// entry. A missing baseline entry is report-only (ok=true, checks=[]).
function gateCorpus(summary, baseline) {
  const entry = baseline && baseline.corpora && baseline.corpora[summary.label];
  if (!entry) return { ok: true, checks: [], skipped: true };
  const tol = toleranceFor(baseline, summary.label);
  const checks = [];
  const cmp = (metric, cur, base, dir, t) => {
    if (!Number.isFinite(base)) return;
    const limit = dir === 'higher' ? base - t : base + t;
    const pass = dir === 'higher' ? cur >= limit : cur <= limit;
    checks.push({ metric, cur, base, limit, dir, pass });
  };
  cmp('ssimMean', summary.ssimMean, num(entry.ssimMean), 'higher', tol.ssim);
  cmp('pdMean', summary.pdMean, num(entry.pdMean), 'lower', tol.pd);
  cmp('rgbMean', summary.rgbMean, num(entry.rgbMean), 'lower', tol.rgb);
  return { ok: checks.every((c) => c.pass), checks, skipped: false };
}

function printGate(results) {
  console.log('=== HTML eval baseline gate (corpus-level means) ===');
  let anyGated = false;
  for (const { summary, gate } of results) {
    if (gate.skipped) {
      console.log(`  ${summary.label}: (no baseline entry — report-only)`);
      continue;
    }
    anyGated = true;
    const status = gate.ok ? 'PASS' : 'FAIL';
    console.log(`  ${summary.label}: ${status}`);
    for (const c of gate.checks) {
      const arrow = c.dir === 'higher' ? '>=' : '<=';
      const mark = c.pass ? 'ok ' : 'BAD';
      console.log(
        `      [${mark}] ${c.metric} ${f(c.cur)} (need ${arrow} ${f(c.limit)}, base ${f(c.base)})`
      );
    }
  }
  if (!anyGated) console.log('  (no corpus had a baseline entry; nothing gated)');
  console.log('');
}

function writeBaseline(file, summaries) {
  const prev = loadBaseline(file) || {};
  const corpora = {};
  for (const s of summaries) {
    const prevEntry = (prev.corpora && prev.corpora[s.label]) || {};
    corpora[s.label] = {
      ssimMean: Number.isFinite(s.ssimMean) ? Number(s.ssimMean.toFixed(4)) : null,
      pdMean: Number.isFinite(s.pdMean) ? Number(s.pdMean.toFixed(4)) : null,
      rgbMean: Number.isFinite(s.rgbMean) ? Number(s.rgbMean.toFixed(2)) : null,
      cases: s.cases,
      // Preserve any hand-tuned per-corpus tolerance override.
      ...(prevEntry.tolerance ? { tolerance: prevEntry.tolerance } : {}),
    };
  }
  const out = {
    _comment:
      'Corpus-level mean baseline for HTMLTest. Update only via a trusted run: ' +
      'HTML_EVAL_UPDATE_BASELINE=1 test/run_html_eval.sh. Do not hand-edit means.',
    updatedAt: new Date().toISOString(),
    corpora,
  };
  fs.writeFileSync(file, JSON.stringify(out, null, 2) + '\n', 'utf8');
  console.log(`summary: wrote baseline ${file}`);
}

function main() {
  const opts = parseArgs(process.argv);
  if (!opts.labels.length) {
    console.error('summary: no labels given');
    process.exit(2);
  }
  const summaries = [];
  const entries = [];
  const allRows = [];
  for (const label of opts.labels) {
    const dir = path.join(opts.outDir, label);
    const csv = path.join(dir, 'report.csv');
    if (!fs.existsSync(csv)) {
      console.error(`summary: skipping '${label}', missing ${csv}`);
      continue;
    }
    const rows = readRows(csv);
    allRows.push(...rows);
    summaries.push(summarize(label, rows));
    // index.html link relative to summary.html (which sits in opts.outDir).
    entries.push({ label, indexHref: path.join(label, 'index.html') });
  }
  if (!summaries.length) {
    console.error('summary: no report.csv found for any label');
    process.exit(1);
  }
  const overall = summarize('ALL', allRows);
  printConsole(summaries, overall);

  const outHtml = path.join(opts.outDir, 'summary.html');
  fs.writeFileSync(outHtml, renderHtml(summaries, overall, opts.title, entries), 'utf8');
  console.log(`summary: wrote ${outHtml}`);
  console.log('');

  // Explicit baseline update: never happens as a side effect of a normal run.
  if (opts.updateBaseline) {
    if (!opts.baseline) {
      console.error('summary: --update-baseline requires --baseline <file>');
      process.exit(2);
    }
    writeBaseline(opts.baseline, summaries);
    return;
  }

  // No baseline path given: stay report-only (preserves prior behaviour).
  if (!opts.baseline) return;

  const baseline = loadBaseline(opts.baseline);
  if (!baseline) {
    console.error(
      `summary: baseline ${opts.baseline} missing or unreadable — run once with ` +
        'HTML_EVAL_UPDATE_BASELINE=1 to seed it. Skipping gate.'
    );
    return;
  }
  const results = summaries.map((s) => ({ summary: s, gate: gateCorpus(s, baseline) }));
  printGate(results);
  const failed = results.filter((r) => !r.gate.ok);
  if (failed.length) {
    console.error(
      `summary: baseline gate FAILED for: ${failed.map((r) => r.summary.label).join(', ')}`
    );
    console.error(
      'summary: if this change is an intended fidelity update, re-baseline via ' +
        'HTML_EVAL_UPDATE_BASELINE=1 test/run_html_eval.sh'
    );
    process.exit(1);
  }
  console.log('summary: baseline gate PASSED');
}

main();
