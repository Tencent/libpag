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
const compare = require('./compare');
const { makeFail } = require('../lib/cli');
const { launchBrowser, resolveEngine } = require('../lib/browser-engine');

const fail = makeFail('run');

function parseArgs(argv) {
  const opts = {
    corpus: process.env.EVAL_CORPUS || '',
    outDir: '',
    pagxBin: process.env.PAGX_BIN || '',
    inferFlex: true,
    skipExisting: false,
    only: '',
    label: 'current',
    recursive: false,
    concurrency: 1,
    // When true, snapshot.js downloads the page's web fonts; run.js then
    // embeds them into the .pagx (`pagx font embed`) and registers them as
    // render fallbacks, mirroring what html2pagx does. Mostly relevant for
    // CJK corpora whose text would otherwise fall back to a host typeface.
    downloadFonts: false,
    // Headless browser driver — propagates to baseline.js / snapshot.js via
    // --browser-engine, plus to the flex-counter pages owned by this script.
    // Honours HTML_SNAPSHOT_BROWSER when no flag is passed.
    browserEngine: resolveEngine(undefined),
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
    else if (a === '--recursive' || a === '-r') opts.recursive = true;
    else if (a === '--download-fonts') opts.downloadFonts = true;
    else if (a === '--concurrency' || a === '-j') {
      const v = parseInt(argv[++i], 10);
      if (!Number.isFinite(v) || v < 1) fail(`--concurrency requires a positive integer, got '${argv[i]}'`);
      opts.concurrency = v;
    }
    else if (a === '--browser-engine') {
      try { opts.browserEngine = resolveEngine(argv[++i]); }
      catch (err) { fail(err && err.message ? err.message : String(err)); }
    }
    else if (a === '-h' || a === '--help') {
      console.log(USAGE);
      process.exit(0);
    } else {
      fail(`unknown option '${a}'`);
    }
  }
  return opts;
}

const USAGE = `Usage: node run.js [options]

  --corpus <dir>      Directory of original HTML files (required; or set $EVAL_CORPUS)
  --out <dir>         Output directory (default: tools/html-snapshot/eval/out/<label>)
  --pagx-bin <path>   pagx binary (default: \$PAGX_BIN or repo cmake-build-debug/pagx)
  --no-infer-flex     Disable C++ AbsoluteToFlexInferencePass
  --skip-existing     Reuse existing baseline.png / subset.png if present
  --only <substr>     Only run cases whose relative path contains <substr>
  --label <name>      Sub-directory name under out/ (default: current)
  --recursive, -r     Recurse into sub-directories of --corpus; case names are
                      derived from the relative path (e.g. tech-hy3/page-001)
  --download-fonts    Download the page's web fonts (snapshot.js), embed them
                      into the .pagx (pagx font embed) and pass them to
                      pagx render as fallbacks. Per case, fonts land in
                      out/<label>/<case>/fonts/. Mirrors html2pagx.
  --concurrency, -j N Process N cases in parallel (default: 1). Each case
                      spawns its own baseline/snapshot Chromium plus the pagx
                      binary, so memory and CPU scale roughly linearly with N.
  --browser-engine N  Headless browser driver: 'puppeteer' (default) or
                      'playwright'. Forwarded to baseline.js and snapshot.js,
                      and also used for this script's flex-counter pages.
                      Honours \$HTML_SNAPSHOT_BROWSER when no flag is given.`;

const SCRIPT_DIR = __dirname;
const TOOL_DIR = path.resolve(SCRIPT_DIR, '..');
const REPO_ROOT = path.resolve(TOOL_DIR, '..', '..');

function defaultPagxBin() {
  return path.join(REPO_ROOT, 'cmake-build-debug', 'pagx');
}

// Returns an array of { path, name } where `path` is the absolute html path
// and `name` is a filesystem-safe identifier derived from the relative path
// (subdirectories collapsed with '__' so that two `page-001.html` files in
// different sub-corpora don't overwrite each other in the output tree).
function findCorpusFiles(corpusDir, only, recursive) {
  const out = [];
  const visit = (dir, relPrefix) => {
    let entries;
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch (err) {
      fail(`cannot read directory '${dir}': ${err.message}`);
    }
    for (const ent of entries) {
      const name = ent.name;
      if (name.startsWith('.')) continue;
      const full = path.join(dir, name);
      if (ent.isDirectory()) {
        if (recursive) visit(full, relPrefix ? `${relPrefix}/${name}` : name);
        continue;
      }
      if (!ent.isFile()) continue;
      if (!name.toLowerCase().endsWith('.html')) continue;
      // We deliberately exclude `.subset.` outputs that earlier runs left in
      // the corpus directory.
      if (name.includes('.subset.')) continue;
      const rel = relPrefix ? `${relPrefix}/${name}` : name;
      if (only && !rel.includes(only)) continue;
      const base = name.slice(0, -'.html'.length);
      const caseName = relPrefix
        ? `${relPrefix.replace(/[\\/]/g, '__')}__${base}`
        : base;
      out.push({ path: full, name: caseName });
    }
  };
  visit(corpusDir, '');
  out.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
  return out;
}

function ensureDir(p) {
  fs.mkdirSync(p, { recursive: true });
}

// Collect the SFNT files snapshot.js wrote into `dir` so they can be embedded
// into the .pagx and registered as render fallbacks. Returns a sorted absolute
// path list; missing directory → empty (the case simply had no web fonts).
function collectFontFiles(dir) {
  let entries;
  try {
    entries = fs.readdirSync(dir);
  } catch (_err) {
    return [];
  }
  return entries
    .filter((n) => /\.(ttf|otf|ttc)$/i.test(n))
    .sort()
    .map((n) => path.join(dir, n));
}

function timeNow() {
  return Date.now();
}

function fmt(n, digits = 4) {
  if (typeof n !== 'number' || !isFinite(n)) return '-';
  return n.toFixed(digits);
}

// Async spawn wrapper. Returns a Promise so processCase calls can run in
// parallel under --concurrency without serializing on the Node event loop
// (spawnSync would have blocked it).
function runProc(cmd, args, stderrPath) {
  return new Promise((resolve) => {
    const startedAt = timeNow();
    let child;
    try {
      child = child_process.spawn(cmd, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    } catch (err) {
      const stderr = `\n${err.message}`;
      if (stderrPath) {
        try { fs.writeFileSync(stderrPath, stderr, 'utf8'); } catch (_e) { /* ignore */ }
      }
      resolve({ code: null, durationMs: timeNow() - startedAt, stdout: '', stderr });
      return;
    }
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (d) => { stdout += d.toString(); });
    child.stderr.on('data', (d) => { stderr += d.toString(); });
    let killedByTimeout = false;
    const timer = setTimeout(() => {
      killedByTimeout = true;
      try { child.kill('SIGKILL'); } catch (_e) { /* ignore */ }
    }, 180000);
    child.on('error', (err) => { stderr += `\n${err.message}`; });
    child.on('close', (code) => {
      clearTimeout(timer);
      if (killedByTimeout) stderr += '\n[runProc] killed by timeout (180s)';
      if (stderrPath) {
        try { fs.writeFileSync(stderrPath, stderr, 'utf8'); } catch (_e) { /* ignore */ }
      }
      resolve({ code, durationMs: timeNow() - startedAt, stdout, stderr });
    });
  });
}

async function processCase(entry, outDir, opts, browser) {
  const htmlPath = entry.path;
  const base = entry.name;
  const caseDir = path.join(outDir, base);
  ensureDir(caseDir);

  const baselinePng = path.join(caseDir, 'baseline.png');
  const subsetHtml = path.join(caseDir, 'subset.html');
  const subsetPagx = path.join(caseDir, 'subset.pagx');
  const subsetPng = path.join(caseDir, 'subset.png');
  const diffPng = path.join(caseDir, 'diff.png');
  const fontDir = path.join(caseDir, 'fonts');
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
    flexDelta: 0,
    flexRetention: NaN,
    flexInflation: NaN,
    importerWarnings: 0,
    flexInferred: 0,
    flexSkipped: 0,
    snapshotMs: 0,
    importMs: 0,
    renderMs: 0,
    baselineMs: 0,
    error: '',
  };

  // The forwarded browser-engine flag keeps every subprocess on the same
  // headless driver as the parent run. Without this, baseline.js and
  // snapshot.js would fall back to their own default (puppeteer) and produce
  // inconsistent results when the parent was launched with --browser-engine
  // playwright (e.g. baseline PNG from puppeteer-Chromium, subset PNG from a
  // playwright-Chromium font cache).
  const engineArgs = ['--browser-engine', opts.browserEngine];

  // 1. baseline
  if (!opts.skipExisting || !fs.existsSync(baselinePng)) {
    const r = await runProc(
      'node',
      [path.join(SCRIPT_DIR, 'baseline.js'), htmlPath, '-o', baselinePng, ...engineArgs],
      baselineStderr,
    );
    row.baselineMs = r.durationMs;
    if (r.code !== 0) {
      row.error = 'baseline-failed';
      return row;
    }
  }

  // 2. snapshot
  if (!opts.skipExisting || !fs.existsSync(subsetHtml)) {
    const snapshotArgs = [path.join(TOOL_DIR, 'snapshot.js'), htmlPath, '-o', subsetHtml, ...engineArgs];
    if (opts.downloadFonts) snapshotArgs.push('--download-fonts', '--font-dir', fontDir);
    const r = await runProc('node', snapshotArgs, snapshotStderr);
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
    const r = await runProc(opts.pagxBin, args, importStderr);
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

  // Downloaded fonts (if any) become `--fallback` args for both `pagx font
  // embed` (so the .pagx is self-contained) and `pagx render` (so any
  // un-embedded face still resolves against the real typeface instead of a
  // host system fallback). Mirrors html2pagx's font handling.
  const fontFallbackArgs = [];
  if (opts.downloadFonts) {
    for (const f of collectFontFiles(fontDir)) fontFallbackArgs.push('--fallback', f);
  }

  // 4. resolve + (font embed) + render
  if (!opts.skipExisting || !fs.existsSync(subsetPng)) {
    await runProc(opts.pagxBin, ['resolve', subsetPagx], path.join(caseDir, 'resolve.stderr.txt'));
    if (fontFallbackArgs.length > 0) {
      await runProc(opts.pagxBin, ['font', 'embed', subsetPagx, ...fontFallbackArgs],
        path.join(caseDir, 'font-embed.stderr.txt'));
    }
    const r = await runProc(opts.pagxBin,
      ['render', subsetPagx, '--output', subsetPng, '--scale', '1', ...fontFallbackArgs],
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

  // 6. flex retention: render both pages with the same method (computed style)
  // so the live and subset counts are directly comparable.
  try {
    row.flexLive = await compare.countFlexInRenderedHtml(browser, htmlPath, {
      viewportWidth: 1400,
      viewportHeight: 900,
      waitMs: 800,
      waitForRoot: true,
    });
    row.flexSubset = await compare.countFlexInRenderedHtml(browser, subsetHtml, {
      viewportWidth: 1400,
      viewportHeight: 900,
      // Subset HTML is static (no JS, no async resources beyond inline assets)
      // so a short settle is enough.
      waitMs: 100,
      waitForRoot: false,
    });
    row.flexDelta = row.flexSubset - row.flexLive;
    if (row.flexLive > 0) {
      row.flexRetention = Math.min(row.flexSubset, row.flexLive) / row.flexLive;
      row.flexInflation = row.flexSubset / row.flexLive;
    } else {
      row.flexRetention = NaN;
      row.flexInflation = NaN;
    }
  } catch (err) {
    row.error = (row.error ? row.error + '; ' : '') + `flex-count-failed: ${err.message}`;
  }

  return row;
}

function writeCsv(rows, outPath) {
  const headers = [
    'name', 'width', 'height',
    'ssim', 'pixelDiffRatio', 'meanRgbDelta',
    'flexLive', 'flexSubset', 'flexDelta', 'flexRetention', 'flexInflation',
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

function summarize(rows) {
  const ok = rows.filter((r) => !r.error);
  const mean = (arr, get) => {
    const vals = arr.map(get).filter((v) => isFinite(v));
    return vals.length ? vals.reduce((a, v) => a + v, 0) / vals.length : NaN;
  };
  const median = (arr, get) => {
    const vals = arr.map(get).filter((v) => isFinite(v)).sort((a, b) => a - b);
    if (!vals.length) return NaN;
    const m = Math.floor(vals.length / 2);
    return vals.length % 2 ? vals[m] : (vals[m - 1] + vals[m]) / 2;
  };
  return {
    cases: rows.length,
    ok: ok.length,
    errored: rows.length - ok.length,
    ssimMean: mean(ok, (r) => r.ssim),
    ssimMedian: median(ok, (r) => r.ssim),
    pdMean: mean(ok, (r) => r.pixelDiffRatio),
    pdMedian: median(ok, (r) => r.pixelDiffRatio),
    retentionMean: mean(ok, (r) => r.flexRetention),
    inflationMean: mean(ok, (r) => r.flexInflation),
    ssimLow: ok.filter((r) => isFinite(r.ssim) && r.ssim < 0.3).length,
    ssimMid: ok.filter((r) => isFinite(r.ssim) && r.ssim >= 0.3 && r.ssim < 0.7).length,
    ssimHigh: ok.filter((r) => isFinite(r.ssim) && r.ssim >= 0.7).length,
    flexInflated: ok.filter((r) => isFinite(r.flexInflation) && r.flexInflation > 1.05).length,
    flexLiveZero: ok.filter((r) => r.flexLive === 0 && r.flexSubset > 0).length,
  };
}

function writeMarkdown(rows, outPath, label) {
  const headers = ['name', 'ssim', 'pixelDiff%', 'rgbΔ', 'flexLive', 'flexSubset', 'flexΔ', 'kept%', 'infl×', 'warn', 'flexInf', 'flexSkip', 'error'];
  const lines = [];
  lines.push(`# Eval report — ${label}`);
  lines.push('');
  const s = summarize(rows);
  lines.push(`Cases: ${s.cases} (ok ${s.ok}, errored ${s.errored})`);
  lines.push('');
  lines.push(`- SSIM — mean ${fmt(s.ssimMean)}, median ${fmt(s.ssimMedian)}`);
  lines.push(`- pixel diff — mean ${fmt(s.pdMean)}, median ${fmt(s.pdMedian)}`);
  lines.push(`- flex retention (min(s,l)/l, capped at 1.0): mean ${fmt(s.retentionMean)}`);
  lines.push(`- flex inflation (subset/live): mean ${fmt(s.inflationMean, 2)}× (cases with >1.05× inflation: ${s.flexInflated})`);
  lines.push(`- SSIM buckets: <0.3 → ${s.ssimLow}, 0.3–0.7 → ${s.ssimMid}, ≥0.7 → ${s.ssimHigh}`);
  if (s.flexLiveZero > 0) {
    lines.push(`- ⚠ ${s.flexLiveZero} cases have flexLive=0 but flexSubset>0 (snapshot invented flex where the live DOM had none)`);
  }
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
      (r.flexDelta > 0 ? '+' : '') + String(r.flexDelta),
      isFinite(r.flexRetention) ? fmt(r.flexRetention * 100, 1) : '-',
      isFinite(r.flexInflation) ? fmt(r.flexInflation, 2) : '-',
      String(r.importerWarnings),
      String(r.flexInferred),
      String(r.flexSkipped),
      r.error || '',
    ];
    lines.push('| ' + cells.join(' | ') + ' |');
  }
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');
}

function ssimClass(s) {
  if (!isFinite(s)) return 'na';
  if (s < 0.3) return 'bad';
  if (s < 0.7) return 'mid';
  return 'good';
}

function flexFlag(r) {
  // Flag cases the eval should draw attention to.
  if (r.flexLive === 0 && r.flexSubset > 0) {
    return { kind: 'invented', label: `live=0, subset=${r.flexSubset}` };
  }
  if (isFinite(r.flexInflation) && r.flexInflation > 1.5) {
    return { kind: 'inflated', label: `${fmt(r.flexInflation, 2)}× inflated` };
  }
  if (isFinite(r.flexRetention) && r.flexRetention < 0.8) {
    return { kind: 'dropped', label: `${fmt(r.flexRetention * 100, 0)}% kept` };
  }
  return null;
}

function writeIndexHtml(rows, outPath, label) {
  const s = summarize(rows);
  const cards = rows.map((r) => {
    const dir = encodeURIComponent(r.name);
    const ssim = fmt(r.ssim, 4);
    const pd = isFinite(r.pixelDiffRatio) ? fmt(r.pixelDiffRatio * 100, 2) + '%' : '-';
    const cls = ssimClass(r.ssim);
    const flag = flexFlag(r);
    const deltaSign = r.flexDelta > 0 ? '+' : '';
    const flagHtml = flag ? ` <span class="flag ${flag.kind}">${flag.label}</span>` : '';
    const status = r.error ? `<span class="err">${r.error}</span>` : '';
    return `
<section class="case">
  <h2>${r.name} <span class="ssim-tag ${cls}">SSIM ${ssim}</span></h2>
  <p>
    pixel diff <b>${pd}</b> &middot; rgbΔ <b>${fmt(r.meanRgbDelta, 2)}</b>
    &middot; flex live/subset <b>${r.flexLive}/${r.flexSubset}</b>
    &middot; Δ <b>${deltaSign}${r.flexDelta}</b>${flagHtml}
    &middot; warnings <b>${r.importerWarnings}</b>
    <span class="muted">(inferred ${r.flexInferred}, skipped ${r.flexSkipped})</span>
    ${status}
  </p>
  <div class="row">
    <figure><figcaption>baseline</figcaption><img src="${dir}/baseline.png" loading="lazy"/></figure>
    <figure><figcaption>subset</figcaption><img src="${dir}/subset.png" loading="lazy"/></figure>
    <figure><figcaption>diff</figcaption><img src="${dir}/diff.png" loading="lazy"/></figure>
  </div>
</section>`;
  }).join('\n');

  const inventedNote = s.flexLiveZero > 0
    ? `<li class="warn">${s.flexLiveZero} case(s) have flexLive=0 but flexSubset&gt;0 (snapshot invented flex containers).</li>`
    : '';
  const inflatedNote = s.flexInflated > 0
    ? `<li class="warn">${s.flexInflated} case(s) emit &gt;1.05× as many flex containers as the live DOM.</li>`
    : '';

  const html = `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<title>html-snapshot eval — ${label}</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 24px; color: #1e293b; }
  h1 { margin-top: 0; }
  .summary { background: #f8fafc; border: 1px solid #e5e7eb; border-radius: 8px; padding: 14px 18px; margin: 12px 0 24px; }
  .summary h3 { margin: 0 0 8px; font-size: 14px; color: #0f172a; }
  .summary ul { margin: 0; padding-left: 18px; font-size: 13px; }
  .summary li { margin: 2px 0; }
  .summary li.warn { color: #b45309; }
  .case { border-top: 1px solid #e5e7eb; padding-top: 18px; margin-top: 18px; }
  .case h2 { margin: 0 0 6px; font-size: 18px; display: flex; align-items: center; gap: 10px; }
  .ssim-tag { font-size: 12px; font-weight: 600; padding: 2px 8px; border-radius: 999px; }
  .ssim-tag.good { background: #dcfce7; color: #166534; }
  .ssim-tag.mid  { background: #fef9c3; color: #854d0e; }
  .ssim-tag.bad  { background: #fee2e2; color: #991b1b; }
  .ssim-tag.na   { background: #e5e7eb; color: #475569; }
  .flag { font-size: 11px; padding: 1px 6px; border-radius: 4px; margin-left: 4px; }
  .flag.invented { background: #fee2e2; color: #991b1b; }
  .flag.inflated { background: #fef3c7; color: #92400e; }
  .flag.dropped  { background: #e0e7ff; color: #3730a3; }
  .err { color: #b00; font-weight: 600; }
  .muted { color: #94a3b8; font-size: 12px; }
  .row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
  .row figure { margin: 0; background: #f1f5f9; padding: 6px; border-radius: 6px; }
  .row figcaption { font-size: 12px; color: #64748b; margin-bottom: 4px; }
  .row img { width: 100%; height: auto; display: block; image-rendering: pixelated; }
  p { color: #475569; font-size: 13px; }
</style>
</head>
<body>
<h1>html-snapshot eval — ${label}</h1>
<p>${rows.length} cases (${s.ok} ok, ${s.errored} errored). Generated ${new Date().toISOString()}.</p>
<div class="summary">
  <h3>Corpus summary</h3>
  <ul>
    <li>SSIM — mean <b>${fmt(s.ssimMean, 4)}</b>, median <b>${fmt(s.ssimMedian, 4)}</b> &middot; buckets: <b>${s.ssimLow}</b> &lt;0.3, <b>${s.ssimMid}</b> 0.3–0.7, <b>${s.ssimHigh}</b> ≥0.7</li>
    <li>pixel diff — mean <b>${isFinite(s.pdMean) ? fmt(s.pdMean * 100, 2) + '%' : '-'}</b>, median <b>${isFinite(s.pdMedian) ? fmt(s.pdMedian * 100, 2) + '%' : '-'}</b></li>
    <li>flex retention (min(subset,live)/live, capped at 1.0) — mean <b>${isFinite(s.retentionMean) ? fmt(s.retentionMean * 100, 1) + '%' : '-'}</b></li>
    <li>flex inflation (subset/live) — mean <b>${fmt(s.inflationMean, 2)}×</b></li>
    ${inflatedNote}
    ${inventedNote}
  </ul>
</div>
${cards}
</body>
</html>`;
  fs.writeFileSync(outPath, html, 'utf8');
}

async function main() {
  const opts = parseArgs(process.argv);
  if (!opts.corpus) {
    fail('--corpus <dir> is required (or set EVAL_CORPUS)');
  }
  if (!opts.pagxBin) opts.pagxBin = defaultPagxBin();
  if (!opts.outDir) opts.outDir = path.join(SCRIPT_DIR, 'out', opts.label);
  ensureDir(opts.outDir);

  if (!fs.existsSync(opts.pagxBin)) {
    fail(`pagx binary not found: ${opts.pagxBin}`, 1);
  }
  const cases = findCorpusFiles(opts.corpus, opts.only, opts.recursive);
  if (cases.length === 0) {
    const hint = opts.recursive ? '' : ' (try --recursive to descend into sub-directories)';
    fail(`no html cases found in ${opts.corpus}${hint}`, 1);
  }
  const concurrency = Math.max(1, Math.min(opts.concurrency, cases.length));
  console.log(`run: ${cases.length} cases → ${opts.outDir}  (concurrency=${concurrency}${opts.downloadFonts ? ', download-fonts=on' : ''})`);

  // A single browser instance is reused for both the live and subset flex
  // counts across every case. Launching once shaves ~1s per case. Opening
  // multiple pages on it concurrently is safe. The wrapper carries the
  // engine name so compare.countFlexInRenderedHtml can apply the right
  // viewport / waitUntil semantics for whichever driver was selected.
  const browser = await launchBrowser({ engine: opts.browserEngine });

  // Results are stored at the input index so the report keeps deterministic
  // order regardless of completion timing. Workers pull from a shared cursor.
  const rows = new Array(cases.length);
  let nextIdx = 0;
  let doneCount = 0;
  const total = cases.length;

  const worker = async () => {
    while (true) {
      const i = nextIdx++;
      if (i >= total) return;
      const c = cases[i];
      const t0 = timeNow();
      let row;
      try {
        row = await processCase(c, opts.outDir, opts, browser);
      } catch (err) {
        row = { name: c.name, htmlPath: c.path, error: `unhandled: ${err && err.message ? err.message : err}` };
      }
      const dur = timeNow() - t0;
      doneCount += 1;
      if (row.error) {
        console.log(`[${doneCount}/${total}] ${c.name}  ERROR ${row.error} (${dur} ms)`);
      } else {
        const delta = row.flexDelta > 0 ? `+${row.flexDelta}` : String(row.flexDelta);
        console.log(`[${doneCount}/${total}] ${c.name}  SSIM ${fmt(row.ssim, 4)}  diff ${fmt(row.pixelDiffRatio * 100, 2)}%  flex ${row.flexLive}/${row.flexSubset} (Δ${delta})  warn ${row.importerWarnings}  (${dur} ms)`);
      }
      rows[i] = row;
    }
  };

  try {
    await Promise.all(Array.from({ length: concurrency }, () => worker()));
  } finally {
    await browser.browser.close();
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
