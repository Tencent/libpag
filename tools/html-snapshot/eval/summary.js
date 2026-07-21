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
  const opts = { outDir: path.join(__dirname, 'out'), title: 'HTML eval summary', labels: [] };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--out') opts.outDir = argv[++i];
    else if (a === '--title') opts.title = argv[++i];
    else if (a === '-h' || a === '--help') {
      console.log('Usage: node summary.js [--out <dir>] [--title <t>] <label> [<label> ...]');
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
}

main();
