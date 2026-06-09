#!/usr/bin/env node
/**
 * run.js — drive the *animation* eval over every original HTML in a corpus directory. It is the
 * moving-picture sibling of `eval/run.js`: where that one diffs a single static frame, this one
 * diffs a short timeline so it measures how faithfully the
 * `snapshot capture → pagx import → pagx render --time` chain reproduces motion.
 *
 *   for each case:
 *     1. baseline-frames.js  → baseline/frame-*.png + baseline/samples.json (the ground-truth
 *                              timeline: the original page seeked to N playback times)
 *     2. snapshot.js         → subset.html  (animations normalised to @keyframes + `animation`)
 *     3. pagx import         → subset.pagx  (HTMLAnimationBuilder maps them to PAGX animations)
 *     4. pagx resolve        → subset.pagx  (inline svg / imports)
 *     5. pagx render --time  → render/frame-*.png at the SAME sample times as step 1
 *     6. compare             → per-frame SSIM / pixel-diff, averaged into one row per case
 *
 * The sample times come from baseline/samples.json so the baseline and the PAGX render share one
 * clock (see baseline-frames.js). Steps 2–4 reuse the shared pipeline helpers in
 * tools/html-snapshot/lib/pipeline.ts; the per-frame render and metrics are this script's own.
 *
 * Outputs land in eval-animation/out/<label>/:
 *   report.csv  — one row per case (means across frames)
 *   report.md   — markdown table with a corpus summary on top
 *   index.html  — per-case baseline / render / diff frame strips; open in a browser
 *   <case>/baseline/*.png, <case>/render/*.png, <case>/diff/*.png, subset.html, subset.pagx, …
 */
'use strict';

const fs = require('fs');
const path = require('path');
const compare = require('../eval/compare');
const { makeFail } = require('../dist/lib/common');
const {
  spawnCapture,
  forkSnapshotCli,
  runPagxImportToFile,
  runPagxResolve,
} = require('../dist/lib/pipeline');

const fail = makeFail('anim-run');

const SCRIPT_DIR = __dirname;
const TOOL_DIR = path.resolve(SCRIPT_DIR, '..');
const REPO_ROOT = path.resolve(TOOL_DIR, '..', '..');

const USAGE = `Usage: node run.js [options]

  --corpus <dir>      Directory of original HTML files (required; or set $EVAL_CORPUS)
  --out <dir>         Output directory (default: eval-animation/out/<label>)
  --pagx-bin <path>   pagx binary (default: $PAGX_BIN or repo cmake-build-debug/pagx)
  --samples <n>       Frames sampled across the timeline (default: 5)
  --scale <float>     pagx render scale (default: 1.0)
  --no-infer-flex     Disable the C++ AbsoluteToFlexInferencePass during import
  --skip-existing     Reuse existing baseline / render frames if present
  --only <substr>     Only run cases whose relative path contains <substr>
  --label <name>      Sub-directory name under out/ (default: current)
  --recursive, -r     Recurse into sub-directories of --corpus
  --concurrency, -j N Process N cases in parallel (default: 1)
  --browser-engine N  Headless driver forwarded to baseline-frames.js / snapshot.js`;

function parseArgs(argv) {
  const opts = {
    corpus: process.env.EVAL_CORPUS || '',
    outDir: '',
    pagxBin: process.env.PAGX_BIN || '',
    samples: 5,
    scale: 1,
    inferFlex: true,
    skipExisting: false,
    only: '',
    label: 'current',
    recursive: false,
    concurrency: 1,
    browserEngine: '',
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--corpus') opts.corpus = argv[++i];
    else if (a === '--out') opts.outDir = argv[++i];
    else if (a === '--pagx-bin') opts.pagxBin = argv[++i];
    else if (a === '--samples') {
      const v = parseInt(argv[++i], 10);
      if (!Number.isFinite(v) || v < 1) fail(`--samples requires a positive integer, got '${argv[i]}'`);
      opts.samples = v;
    } else if (a === '--scale') {
      const v = parseFloat(argv[++i]);
      if (!Number.isFinite(v) || v <= 0) fail(`--scale requires a positive number, got '${argv[i]}'`);
      opts.scale = v;
    } else if (a === '--no-infer-flex') opts.inferFlex = false;
    else if (a === '--skip-existing') opts.skipExisting = true;
    else if (a === '--only') opts.only = argv[++i];
    else if (a === '--label') opts.label = argv[++i];
    else if (a === '--recursive' || a === '-r') opts.recursive = true;
    else if (a === '--browser-engine') opts.browserEngine = argv[++i];
    else if (a === '--concurrency' || a === '-j') {
      const v = parseInt(argv[++i], 10);
      if (!Number.isFinite(v) || v < 1) fail(`--concurrency requires a positive integer, got '${argv[i]}'`);
      opts.concurrency = v;
    } else if (a === '-h' || a === '--help') {
      console.log(USAGE);
      process.exit(0);
    } else {
      fail(`unknown option '${a}'`);
    }
  }
  return opts;
}

function defaultPagxBin() {
  return path.join(REPO_ROOT, 'cmake-build-debug', 'pagx');
}

function ensureDir(p) {
  fs.mkdirSync(p, { recursive: true });
}

function fmt(n, digits = 4) {
  if (typeof n !== 'number' || !isFinite(n)) return '-';
  return n.toFixed(digits);
}

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

// Parse `subset:animation-*` importer diagnostics plus the "captured N animation(s)" line the
// snapshot tool logs, so the report shows how much animation actually flowed through each stage.
function countAnimationsCaptured(stderrPath) {
  if (!fs.existsSync(stderrPath)) return 0;
  const text = fs.readFileSync(stderrPath, 'utf8');
  const m = text.match(/captured (\d+) animation\(s\)/);
  return m ? parseInt(m[1], 10) : 0;
}

function countAnimationWarnings(stderrPath) {
  if (!fs.existsSync(stderrPath)) return { animWarnings: 0, animDropped: 0 };
  const text = fs.readFileSync(stderrPath, 'utf8');
  let animWarnings = 0;
  let animDropped = 0;
  for (const line of text.split(/\r?\n/)) {
    if (/subset:animation-/.test(line)) animWarnings++;
    if (/subset:animation-unknown-keyframes/.test(line)) animDropped++;
  }
  return { animWarnings, animDropped };
}

// Mean of an array, ignoring non-finite entries.
function mean(arr) {
  const vals = arr.filter((v) => isFinite(v));
  return vals.length ? vals.reduce((a, v) => a + v, 0) / vals.length : NaN;
}

// "Motion" the baseline timeline actually contains: 1 - mean SSIM between consecutive baseline
// frames. ~0 means the frames are identical (no visible animation captured); higher means more
// movement. Lets a reader tell "PAGX matched a still page" from "PAGX matched a moving one".
function baselineMotion(baselineFrames) {
  if (baselineFrames.length < 2) return 0;
  const deltas = [];
  for (let i = 1; i < baselineFrames.length; i++) {
    try {
      const m = compare.pixelMetrics(baselineFrames[i - 1], baselineFrames[i], null);
      deltas.push(1 - m.ssim);
    } catch (_) {
      /* ignore */
    }
  }
  const mm = mean(deltas);
  return isFinite(mm) ? mm : 0;
}

async function runPagxRenderAtTime(opts) {
  const args = [
    'render', opts.pagxFile,
    '--output', opts.output,
    '--scale', String(opts.scale),
    '--time', String(opts.timeSec),
  ];
  return spawnCapture(opts.pagxBin, args, { stderrPath: opts.stderrPath });
}

async function processCase(entry, outDir, opts) {
  const htmlPath = entry.path;
  const base = entry.name;
  const caseDir = path.join(outDir, base);
  const baselineDir = path.join(caseDir, 'baseline');
  const renderDir = path.join(caseDir, 'render');
  const diffDir = path.join(caseDir, 'diff');
  ensureDir(baselineDir);
  ensureDir(renderDir);
  ensureDir(diffDir);

  const subsetHtml = path.join(caseDir, 'subset.html');
  const subsetPagx = path.join(caseDir, 'subset.pagx');
  const samplesJson = path.join(baselineDir, 'samples.json');
  const importStderr = path.join(caseDir, 'import.stderr.txt');
  const snapshotStderr = path.join(caseDir, 'snapshot.stderr.txt');
  const baselineStderr = path.join(caseDir, 'baseline.stderr.txt');

  const row = {
    name: base,
    frames: 0,
    ssimMean: NaN,
    pixelDiffMean: NaN,
    motion: NaN,
    animCaptured: 0,
    animWarnings: 0,
    animDropped: 0,
    width: 0,
    height: 0,
    error: '',
  };

  const engineArgs = opts.browserEngine ? ['--browser-engine', opts.browserEngine] : [];

  // 1. baseline frames (ground-truth timeline)
  if (!opts.skipExisting || !fs.existsSync(samplesJson)) {
    const r = await spawnCapture(
      process.execPath,
      [
        path.join(SCRIPT_DIR, 'baseline-frames.js'), htmlPath,
        '-o', baselineDir,
        '--samples', String(opts.samples),
        ...engineArgs,
      ],
      { stderrPath: baselineStderr },
    );
    if (r.code !== 0 || !fs.existsSync(samplesJson)) {
      row.error = 'baseline-failed';
      return row;
    }
  }

  let samplesMeta;
  try {
    samplesMeta = JSON.parse(fs.readFileSync(samplesJson, 'utf8'));
  } catch (err) {
    row.error = `samples-read-failed: ${err.message}`;
    return row;
  }
  const samples = Array.isArray(samplesMeta.samples) ? samplesMeta.samples : [];
  row.width = samplesMeta.width || 0;
  row.height = samplesMeta.height || 0;
  if (samples.length === 0) {
    row.error = 'no-samples';
    return row;
  }

  // 2. snapshot (captures + normalises the animation into the subset HTML)
  if (!opts.skipExisting || !fs.existsSync(subsetHtml)) {
    const r = await forkSnapshotCli({
      input: htmlPath,
      output: subsetHtml,
      scriptDir: TOOL_DIR,
      browserEngine: opts.browserEngine || undefined,
      stderrPath: snapshotStderr,
    });
    if (r.code !== 0) {
      row.error = 'snapshot-failed';
      return row;
    }
  }
  row.animCaptured = countAnimationsCaptured(snapshotStderr);

  // 3. import
  if (!opts.skipExisting || !fs.existsSync(subsetPagx)) {
    const r = await runPagxImportToFile({
      pagxBin: opts.pagxBin,
      subsetHtml,
      pagxFile: subsetPagx,
      inferFlex: opts.inferFlex,
      stderrPath: importStderr,
    });
    if (r.code !== 0 || !fs.existsSync(subsetPagx)) {
      row.error = 'import-failed';
      return row;
    }
  }
  const w = countAnimationWarnings(importStderr);
  row.animWarnings = w.animWarnings;
  row.animDropped = w.animDropped;

  // 4. resolve
  {
    const r = await runPagxResolve({
      pagxBin: opts.pagxBin,
      pagxFile: subsetPagx,
      stderrPath: path.join(caseDir, 'resolve.stderr.txt'),
    });
    if (r.code !== 0) {
      row.error = 'resolve-failed';
      return row;
    }
  }

  // 5. render each sample time + 6. per-frame metrics
  const baselineFrames = [];
  const ssims = [];
  const pdiffs = [];
  for (const s of samples) {
    const idx = String(s.index).padStart(3, '0');
    const baselinePng = path.join(baselineDir, `frame-${idx}.png`);
    const renderPng = path.join(renderDir, `frame-${idx}.png`);
    const diffPng = path.join(diffDir, `frame-${idx}.png`);
    if (fs.existsSync(baselinePng)) baselineFrames.push(baselinePng);

    if (!opts.skipExisting || !fs.existsSync(renderPng)) {
      const r = await runPagxRenderAtTime({
        pagxBin: opts.pagxBin,
        pagxFile: subsetPagx,
        output: renderPng,
        scale: opts.scale,
        timeSec: s.timeSec,
        stderrPath: path.join(caseDir, `render.${idx}.stderr.txt`),
      });
      if (r.code !== 0 || !fs.existsSync(renderPng)) {
        row.error = `render-failed@${idx}`;
        return row;
      }
    }

    try {
      const m = compare.pixelMetrics(baselinePng, renderPng, diffPng);
      ssims.push(m.ssim);
      pdiffs.push(m.pixelDiffRatio);
    } catch (err) {
      row.error = `compare-failed@${idx}: ${err.message}`;
      return row;
    }
  }

  row.frames = samples.length;
  row.ssimMean = mean(ssims);
  row.pixelDiffMean = mean(pdiffs);
  row.motion = baselineMotion(baselineFrames);
  return row;
}

function writeCsv(rows, outPath) {
  const headers = [
    'name', 'frames', 'ssimMean', 'pixelDiffMean', 'motion',
    'animCaptured', 'animWarnings', 'animDropped', 'width', 'height', 'error',
  ];
  const lines = [headers.join(',')];
  for (const r of rows) {
    const cells = headers.map((h) => {
      const v = r[h];
      if (typeof v === 'number') {
        if (!isFinite(v)) return '';
        return Number.isInteger(v) ? String(v) : v.toFixed(6);
      }
      if (v === undefined || v === null) return '';
      const s = String(v);
      return s.includes(',') || s.includes('"') ? `"${s.replace(/"/g, '""')}"` : s;
    });
    lines.push(cells.join(','));
  }
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');
}

function summarize(rows) {
  const ok = rows.filter((r) => !r.error);
  const moving = ok.filter((r) => isFinite(r.motion) && r.motion > 0.01);
  return {
    cases: rows.length,
    ok: ok.length,
    errored: rows.length - ok.length,
    moving: moving.length,
    ssimMean: mean(ok.map((r) => r.ssimMean)),
    ssimMovingMean: mean(moving.map((r) => r.ssimMean)),
    pdMean: mean(ok.map((r) => r.pixelDiffMean)),
    animCaptured: ok.reduce((a, r) => a + (r.animCaptured || 0), 0),
    animDropped: ok.reduce((a, r) => a + (r.animDropped || 0), 0),
  };
}

function writeMarkdown(rows, outPath, label) {
  const s = summarize(rows);
  const headers = ['name', 'frames', 'ssim', 'pixelDiff%', 'motion', 'animCap', 'animWarn', 'animDrop', 'error'];
  const lines = [];
  lines.push(`# Animation eval report — ${label}`);
  lines.push('');
  lines.push(`Cases: ${s.cases} (ok ${s.ok}, errored ${s.errored}, with motion ${s.moving})`);
  lines.push('');
  lines.push(`- SSIM (per-frame mean) — all cases ${fmt(s.ssimMean)}, moving cases only ${fmt(s.ssimMovingMean)}`);
  lines.push(`- pixel diff (per-frame mean) — ${fmt(s.pdMean)}`);
  lines.push(`- animations captured: ${s.animCaptured} (dropped at import: ${s.animDropped})`);
  lines.push('');
  lines.push('| ' + headers.join(' | ') + ' |');
  lines.push('| ' + headers.map(() => '---').join(' | ') + ' |');
  for (const r of rows) {
    const cells = [
      r.name,
      String(r.frames),
      fmt(r.ssimMean, 4),
      isFinite(r.pixelDiffMean) ? fmt(r.pixelDiffMean * 100, 2) : '-',
      fmt(r.motion, 3),
      String(r.animCaptured),
      String(r.animWarnings),
      String(r.animDropped),
      r.error || '',
    ];
    lines.push('| ' + cells.join(' | ') + ' |');
  }
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');
}

function ssimClass(v) {
  if (!isFinite(v)) return 'na';
  if (v < 0.3) return 'bad';
  if (v < 0.7) return 'mid';
  return 'good';
}

function writeIndexHtml(rows, outDir, label) {
  const s = summarize(rows);
  const cards = rows.map((r) => {
    const dir = encodeURIComponent(r.name);
    const cls = ssimClass(r.ssimMean);
    const status = r.error ? `<span class="err">${r.error}</span>` : '';
    let strips = '';
    if (!r.error && r.frames > 0) {
      const rowFor = (sub, label2) => {
        let cells = '';
        for (let i = 0; i < r.frames; i++) {
          const idx = String(i).padStart(3, '0');
          cells += `<img src="${dir}/${sub}/frame-${idx}.png" loading="lazy"/>`;
        }
        return `<div class="strip"><span class="strip-label">${label2}</span><div class="frames">${cells}</div></div>`;
      };
      strips = rowFor('baseline', 'baseline') + rowFor('render', 'pagx') + rowFor('diff', 'diff');
    }
    return `
<section class="case">
  <h2>${r.name} <span class="ssim-tag ${cls}">SSIM ${fmt(r.ssimMean, 4)}</span></h2>
  <p>
    frames <b>${r.frames}</b> &middot; pixel diff <b>${isFinite(r.pixelDiffMean) ? fmt(r.pixelDiffMean * 100, 2) + '%' : '-'}</b>
    &middot; motion <b>${fmt(r.motion, 3)}</b>
    &middot; animations captured <b>${r.animCaptured}</b>
    <span class="muted">(import warnings ${r.animWarnings}, dropped ${r.animDropped})</span>
    ${status}
  </p>
  ${strips}
</section>`;
  }).join('\n');

  const html = `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<title>html-snapshot animation eval — ${label}</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 24px; color: #1e293b; }
  h1 { margin-top: 0; }
  .summary { background: #f8fafc; border: 1px solid #e5e7eb; border-radius: 8px; padding: 14px 18px; margin: 12px 0 24px; }
  .summary ul { margin: 0; padding-left: 18px; font-size: 13px; }
  .case { border-top: 1px solid #e5e7eb; padding-top: 18px; margin-top: 18px; }
  .case h2 { margin: 0 0 6px; font-size: 18px; display: flex; align-items: center; gap: 10px; }
  .ssim-tag { font-size: 12px; font-weight: 600; padding: 2px 8px; border-radius: 999px; }
  .ssim-tag.good { background: #dcfce7; color: #166534; }
  .ssim-tag.mid  { background: #fef9c3; color: #854d0e; }
  .ssim-tag.bad  { background: #fee2e2; color: #991b1b; }
  .ssim-tag.na   { background: #e5e7eb; color: #475569; }
  .err { color: #b00; font-weight: 600; }
  .muted { color: #94a3b8; font-size: 12px; }
  .strip { display: flex; align-items: center; gap: 10px; margin: 6px 0; }
  .strip-label { width: 64px; font-size: 12px; color: #64748b; flex: none; }
  .frames { display: flex; gap: 6px; overflow-x: auto; }
  .frames img { height: 140px; width: auto; background: #f1f5f9; border-radius: 4px; image-rendering: pixelated; }
  p { color: #475569; font-size: 13px; }
</style>
</head>
<body>
<h1>html-snapshot animation eval — ${label}</h1>
<p>${rows.length} cases (${s.ok} ok, ${s.errored} errored, ${s.moving} with motion). Generated ${new Date().toISOString()}.</p>
<div class="summary">
  <ul>
    <li>SSIM (per-frame mean) — all <b>${fmt(s.ssimMean, 4)}</b>, moving cases only <b>${fmt(s.ssimMovingMean, 4)}</b></li>
    <li>pixel diff (per-frame mean) — <b>${isFinite(s.pdMean) ? fmt(s.pdMean * 100, 2) + '%' : '-'}</b></li>
    <li>animations captured — <b>${s.animCaptured}</b> (dropped at import: <b>${s.animDropped}</b>)</li>
  </ul>
</div>
${cards}
</body>
</html>`;
  fs.writeFileSync(path.join(outDir, 'index.html'), html, 'utf8');
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
  console.log(`anim-run: ${cases.length} cases → ${opts.outDir}  (samples=${opts.samples}, concurrency=${concurrency})`);

  const rows = new Array(cases.length);
  let nextIdx = 0;
  let doneCount = 0;
  const total = cases.length;

  const worker = async () => {
    while (true) {
      const i = nextIdx++;
      if (i >= total) return;
      const c = cases[i];
      const t0 = Date.now();
      let row;
      try {
        row = await processCase(c, opts.outDir, opts);
      } catch (err) {
        row = { name: c.name, frames: 0, error: `unhandled: ${err && err.message ? err.message : err}` };
      }
      const dur = Date.now() - t0;
      doneCount += 1;
      if (row.error) {
        console.log(`[${doneCount}/${total}] ${c.name}  ERROR ${row.error} (${dur} ms)`);
      } else {
        console.log(`[${doneCount}/${total}] ${c.name}  SSIM ${fmt(row.ssimMean, 4)}  diff ${fmt(row.pixelDiffMean * 100, 2)}%  motion ${fmt(row.motion, 3)}  anim ${row.animCaptured}  (${dur} ms)`);
      }
      rows[i] = row;
    }
  };

  await Promise.all(Array.from({ length: concurrency }, () => worker()));

  writeCsv(rows, path.join(opts.outDir, 'report.csv'));
  writeMarkdown(rows, path.join(opts.outDir, 'report.md'), opts.label);
  writeIndexHtml(rows, opts.outDir, opts.label);
  console.log(`anim-run: wrote ${path.join(opts.outDir, 'report.csv')}`);
  console.log(`anim-run: wrote ${path.join(opts.outDir, 'report.md')}`);
  console.log(`anim-run: wrote ${path.join(opts.outDir, 'index.html')}`);
}

main().catch((err) => {
  console.error(err && err.stack ? err.stack : err);
  process.exit(1);
});
