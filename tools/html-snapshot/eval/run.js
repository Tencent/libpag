#!/usr/bin/env node
/**
 * run.js — drive the eval pipeline over every original HTML in a corpus
 * directory:
 *
 *   for each case:
 *     1. baseline.js  → out/<case>.baseline.png
 *     2. html2pagx    → out/<case>.subset.html / .pagx / .png + stderr
 *     3. compare.js   → metrics row
 *
 * Outputs:
 *   out/report.csv         — one row per case
 *   out/report.md          — markdown table for human reading
 *   out/index.html         — side-by-side baseline / subset / diff viewer
 *
 * The script is idempotent: passing --skip-existing reuses an existing
 * baseline / subset PNG if present so you can re-run only the metrics step
 * after editing compare.js.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const child_process = require('child_process');
const puppeteer = require('puppeteer');
const compare = require('./compare');

function parseArgs(argv) {
  const opts = {
    corpus: '/Users/yucong/Desktop/tmp_pagx',
    outDir: '',
    pagxBin: process.env.PAGX_BIN || '',
    inferFlex: true,
    skipExisting: false,
    only: '',
    label: 'current',
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--corpus') opts.corpus = argv[++i];
    else if (a === '--out') opts.outDir = argv[++i];
    else if (a === '--pagx-bin') opts.pagxBin = argv[++i];
    else if (a === '--no-infer-flex') opts.inferFlex = false;
    else if (a === '--skip-existing') opts.skipExisting = true;
    else if (a === '--only') opts.only = argv[++i];
    else if (a === '--label') opts.label = argv[++i];
    else if (a === '-h' || a === '--help') {
      console.log(USAGE);
      process.exit(0);
    } else {
      console.error(`run: unknown option '${a}'`);
      process.exit(2);
    }
  }
  return opts;
}

const USAGE = `Usage: node run.js [options]

  --corpus <dir>      Directory of original HTML files (default: /Users/yucong/Desktop/tmp_pagx)
  --out <dir>         Output directory (default: tools/html-snapshot/eval/out/<label>)
  --pagx-bin <path>   pagx binary (default: \$PAGX_BIN or repo cmake-build-debug/pagx)
  --no-infer-flex     Disable C++ AbsoluteToFlexInferencePass
  --skip-existing     Reuse existing baseline.png / subset.png if present
  --only <substr>     Only run cases whose filename contains <substr>
  --label <name>      Sub-directory name under out/ (default: current)`;

const SCRIPT_DIR = __dirname;
const TOOL_DIR = path.resolve(SCRIPT_DIR, '..');
const REPO_ROOT = path.resolve(TOOL_DIR, '..', '..');

function defaultPagxBin() {
  return path.join(REPO_ROOT, 'cmake-build-debug', 'pagx');
}

function findCorpusFiles(corpusDir, only) {
  // We deliberately exclude `.subset.` outputs that earlier runs left in the
  // corpus directory. Only top-level *.html files are kept.
  const entries = fs.readdirSync(corpusDir);
  const files = [];
  for (const name of entries) {
    if (!name.toLowerCase().endsWith('.html')) continue;
    if (name.includes('.subset.')) continue;
    if (only && !name.includes(only)) continue;
    const full = path.join(corpusDir, name);
    const stat = fs.statSync(full);
    if (!stat.isFile()) continue;
    files.push(full);
  }
  files.sort();
  return files;
}

function ensureDir(p) {
  fs.mkdirSync(p, { recursive: true });
}

function timeNow() {
  return Date.now();
}

function fmt(n, digits = 4) {
  if (typeof n !== 'number' || !isFinite(n)) return '-';
  return n.toFixed(digits);
}

function runProc(cmd, args, stderrPath) {
  const startedAt = timeNow();
  const result = child_process.spawnSync(cmd, args, {
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
    timeout: 180000,
  });
  const stderr = (result.stderr || '') + (result.error ? `\n${result.error.message}` : '');
  if (stderrPath) fs.writeFileSync(stderrPath, stderr, 'utf8');
  return {
    code: result.status,
    durationMs: timeNow() - startedAt,
    stdout: result.stdout || '',
    stderr,
  };
}

async function processCase(htmlPath, outDir, opts) {
  const base = path.basename(htmlPath, '.html');
  const caseDir = path.join(outDir, base);
  ensureDir(caseDir);

  const baselinePng = path.join(caseDir, 'baseline.png');
  const subsetHtml = path.join(caseDir, 'subset.html');
  const subsetPagx = path.join(caseDir, 'subset.pagx');
  const subsetPng = path.join(caseDir, 'subset.png');
  const diffPng = path.join(caseDir, 'diff.png');
  const importStderr = path.join(caseDir, 'import.stderr.txt');
  const snapshotStderr = path.join(caseDir, 'snapshot.stderr.txt');
  const baselineStderr = path.join(caseDir, 'baseline.stderr.txt');

  const row = {
    name: base,
    htmlPath,
    width: 0,
    height: 0,
    pixelDiffRatio: NaN,
    meanRgbDelta: NaN,
    ssim: NaN,
    flexLive: 0,
    flexSubset: 0,
    flexRetention: NaN,
    importerWarnings: 0,
    flexInferred: 0,
    flexSkipped: 0,
    snapshotMs: 0,
    importMs: 0,
    renderMs: 0,
    baselineMs: 0,
    error: '',
  };

  // 1. baseline
  if (!opts.skipExisting || !fs.existsSync(baselinePng)) {
    const r = runProc('node', [path.join(SCRIPT_DIR, 'baseline.js'), htmlPath, '-o', baselinePng], baselineStderr);
    row.baselineMs = r.durationMs;
    if (r.code !== 0) {
      row.error = 'baseline-failed';
      return row;
    }
  }

  // 2. snapshot
  if (!opts.skipExisting || !fs.existsSync(subsetHtml)) {
    const r = runProc('node', [path.join(TOOL_DIR, 'snapshot.js'), htmlPath, '-o', subsetHtml], snapshotStderr);
    row.snapshotMs = r.durationMs;
    if (r.code !== 0) {
      row.error = 'snapshot-failed';
      return row;
    }
  }

  // 3. import
  if (!opts.skipExisting || !fs.existsSync(subsetPagx)) {
    const args = ['import', '--input', subsetHtml, '--output', subsetPagx, '--format', 'html'];
    if (opts.inferFlex) args.push('--html-infer-flex');
    const r = runProc(opts.pagxBin, args, importStderr);
    row.importMs = r.durationMs;
    if (r.code !== 0 || !fs.existsSync(subsetPagx)) {
      row.error = 'import-failed';
      return row;
    }
  }
  const w = compare.countImporterWarnings(importStderr);
  row.importerWarnings = w.total;
  row.flexInferred = w.flexInferred;
  row.flexSkipped = w.flexSkipped;

  // 4. resolve + render
  if (!opts.skipExisting || !fs.existsSync(subsetPng)) {
    runProc(opts.pagxBin, ['resolve', subsetPagx], path.join(caseDir, 'resolve.stderr.txt'));
    const r = runProc(opts.pagxBin, ['render', subsetPagx, '--output', subsetPng, '--scale', '1'],
      path.join(caseDir, 'render.stderr.txt'));
    row.renderMs = r.durationMs;
    if (r.code !== 0 || !fs.existsSync(subsetPng)) {
      row.error = 'render-failed';
      return row;
    }
  }

  // 5. metrics
  try {
    const m = compare.pixelMetrics(baselinePng, subsetPng, diffPng);
    row.width = m.width;
    row.height = m.height;
    row.pixelDiffRatio = m.pixelDiffRatio;
    row.meanRgbDelta = m.meanRgbDelta;
    row.ssim = m.ssim;
  } catch (err) {
    row.error = `compare-failed: ${err.message}`;
    return row;
  }

  // 6. flex retention
  try {
    row.flexLive = await compare.countFlexInLiveDom(puppeteer, htmlPath, 1400, 900, 800);
    row.flexSubset = compare.countFlexInSubsetHtml(subsetHtml);
    if (row.flexLive > 0) row.flexRetention = row.flexSubset / row.flexLive;
    else row.flexRetention = NaN;
  } catch (err) {
    row.error = (row.error ? row.error + '; ' : '') + `flex-count-failed: ${err.message}`;
  }

  return row;
}

function writeCsv(rows, outPath) {
  const headers = [
    'name', 'width', 'height',
    'ssim', 'pixelDiffRatio', 'meanRgbDelta',
    'flexLive', 'flexSubset', 'flexRetention',
    'importerWarnings', 'flexInferred', 'flexSkipped',
    'baselineMs', 'snapshotMs', 'importMs', 'renderMs',
    'error',
  ];
  const lines = [headers.join(',')];
  for (const r of rows) {
    const row = headers.map((h) => {
      const v = r[h];
      if (typeof v === 'number') {
        if (!isFinite(v)) return '';
        return Number.isInteger(v) ? String(v) : v.toFixed(6);
      }
      if (v === undefined || v === null) return '';
      const s = String(v);
      return s.includes(',') || s.includes('"') ? `"${s.replace(/"/g, '""')}"` : s;
    });
    lines.push(row.join(','));
  }
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');
}

function writeMarkdown(rows, outPath, label) {
  const headers = ['name', 'ssim', 'pixelDiff%', 'rgbΔ', 'flexLive', 'flexSubset', 'flexKept%', 'warn', 'flexInf', 'flexSkip', 'error'];
  const lines = [];
  lines.push(`# Eval report — ${label}`);
  lines.push('');
  lines.push(`Cases: ${rows.length}`);
  const ok = rows.filter((r) => !r.error);
  const ssimMean = ok.length ? ok.reduce((a, r) => a + (isFinite(r.ssim) ? r.ssim : 0), 0) / ok.length : NaN;
  const pdMean = ok.length ? ok.reduce((a, r) => a + (isFinite(r.pixelDiffRatio) ? r.pixelDiffRatio : 0), 0) / ok.length : NaN;
  const flexKept = ok.filter((r) => isFinite(r.flexRetention));
  const retMean = flexKept.length ? flexKept.reduce((a, r) => a + r.flexRetention, 0) / flexKept.length : NaN;
  lines.push('');
  lines.push(`- mean SSIM: ${fmt(ssimMean)}`);
  lines.push(`- mean pixel diff ratio: ${fmt(pdMean)}`);
  lines.push(`- mean flex retention: ${fmt(retMean)}`);
  lines.push('');
  lines.push('| ' + headers.join(' | ') + ' |');
  lines.push('| ' + headers.map(() => '---').join(' | ') + ' |');
  for (const r of rows) {
    const cells = [
      r.name,
      fmt(r.ssim, 4),
      isFinite(r.pixelDiffRatio) ? fmt(r.pixelDiffRatio * 100, 2) : '-',
      fmt(r.meanRgbDelta, 2),
      String(r.flexLive),
      String(r.flexSubset),
      isFinite(r.flexRetention) ? fmt(r.flexRetention * 100, 1) : '-',
      String(r.importerWarnings),
      String(r.flexInferred),
      String(r.flexSkipped),
      r.error || '',
    ];
    lines.push('| ' + cells.join(' | ') + ' |');
  }
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');
}

function writeIndexHtml(rows, outPath, label) {
  const cards = rows.map((r) => {
    const dir = encodeURIComponent(r.name);
    const ssim = fmt(r.ssim, 4);
    const pd = isFinite(r.pixelDiffRatio) ? fmt(r.pixelDiffRatio * 100, 2) + '%' : '-';
    const ret = isFinite(r.flexRetention) ? fmt(r.flexRetention * 100, 1) + '%' : '-';
    const status = r.error ? `<span style="color:#b00">${r.error}</span>` : '';
    return `
<section class="case">
  <h2>${r.name}</h2>
  <p>
    SSIM <b>${ssim}</b> &middot; pixel diff <b>${pd}</b> &middot; rgbΔ <b>${fmt(r.meanRgbDelta, 2)}</b>
    &middot; flex live/subset <b>${r.flexLive}/${r.flexSubset}</b>
    &middot; retained <b>${ret}</b>
    &middot; warnings <b>${r.importerWarnings}</b> (inferred ${r.flexInferred}, skipped ${r.flexSkipped})
    ${status}
  </p>
  <div class="row">
    <figure><figcaption>baseline</figcaption><img src="${dir}/baseline.png" loading="lazy"/></figure>
    <figure><figcaption>subset</figcaption><img src="${dir}/subset.png" loading="lazy"/></figure>
    <figure><figcaption>diff</figcaption><img src="${dir}/diff.png" loading="lazy"/></figure>
  </div>
</section>`;
  }).join('\n');
  const html = `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<title>html-snapshot eval — ${label}</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 24px; color: #1e293b; }
  h1 { margin-top: 0; }
  .case { border-top: 1px solid #e5e7eb; padding-top: 18px; margin-top: 18px; }
  .case h2 { margin: 0 0 6px; font-size: 18px; }
  .row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
  .row figure { margin: 0; background: #f1f5f9; padding: 6px; border-radius: 6px; }
  .row figcaption { font-size: 12px; color: #64748b; margin-bottom: 4px; }
  .row img { width: 100%; height: auto; display: block; image-rendering: pixelated; }
  p { color: #475569; font-size: 13px; }
</style>
</head>
<body>
<h1>html-snapshot eval — ${label}</h1>
<p>${rows.length} cases. Generated ${new Date().toISOString()}.</p>
${cards}
</body>
</html>`;
  fs.writeFileSync(outPath, html, 'utf8');
}

async function main() {
  const opts = parseArgs(process.argv);
  if (!opts.pagxBin) opts.pagxBin = defaultPagxBin();
  if (!opts.outDir) opts.outDir = path.join(SCRIPT_DIR, 'out', opts.label);
  ensureDir(opts.outDir);

  if (!fs.existsSync(opts.pagxBin)) {
    console.error(`run: pagx binary not found: ${opts.pagxBin}`);
    process.exit(1);
  }
  const cases = findCorpusFiles(opts.corpus, opts.only);
  if (cases.length === 0) {
    console.error(`run: no html cases found in ${opts.corpus}`);
    process.exit(1);
  }
  console.log(`run: ${cases.length} cases → ${opts.outDir}`);

  const rows = [];
  for (let i = 0; i < cases.length; i++) {
    const c = cases[i];
    const name = path.basename(c);
    process.stdout.write(`[${i + 1}/${cases.length}] ${name}  `);
    const t0 = timeNow();
    const row = await processCase(c, opts.outDir, opts);
    const dur = timeNow() - t0;
    if (row.error) {
      console.log(`ERROR ${row.error} (${dur} ms)`);
    } else {
      console.log(`SSIM ${fmt(row.ssim, 4)}  diff ${fmt(row.pixelDiffRatio * 100, 2)}%  flex ${row.flexSubset}/${row.flexLive}  warn ${row.importerWarnings}  (${dur} ms)`);
    }
    rows.push(row);
  }

  writeCsv(rows, path.join(opts.outDir, 'report.csv'));
  writeMarkdown(rows, path.join(opts.outDir, 'report.md'), opts.label);
  writeIndexHtml(rows, path.join(opts.outDir, 'index.html'), opts.label);
  console.log(`run: wrote ${path.join(opts.outDir, 'report.csv')}`);
  console.log(`run: wrote ${path.join(opts.outDir, 'report.md')}`);
  console.log(`run: wrote ${path.join(opts.outDir, 'index.html')}`);
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
