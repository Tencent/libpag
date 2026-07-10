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
  --download-images   Download the page's external images (snapshot.js) and
                      reference them by local file path instead of inlining them
                      as base64. Images land in a single shared, content-
                      addressed cache at out/<label>/images/ (identical images
                      stored once); pagx render reads the files directly, so no
                      per-case manifest is needed.
  --pagx-images <m>   How images are stored in each .pagx: 'external' (default;
                      write image files next to the case's .pagx, keeping their
                      relative path, producing a portable per-case folder) or
                      'embed' (base64 data URIs). Independent of
                      --download-images.
  --skip-existing     Reuse existing baseline / render frames if present
  --only <substr>     Only run cases whose relative path contains <substr>.
                      Repeat the flag or pass a comma-separated list to match any of them.
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
    // When true, snapshot.js downloads the page's external images into a shared,
    // content-addressed cache (out/<label>/images/) and rewrites each subset
    // HTML to reference them by their absolute file path instead of inlining
    // them as base64 data URIs.
    downloadImages: false,
    // How images are stored in the exported .pagx: 'external' (default; write
    // image files next to each case's .pagx, keeping their relative path) or
    // 'embed' (base64 data URIs). Independent of --download-images.
    pagxImages: 'external',
    skipExisting: false,
    only: [],
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
    else if (a === '--download-images') opts.downloadImages = true;
    else if (a === '--pagx-images') {
      const mode = argv[++i];
      if (mode !== 'embed' && mode !== 'external') {
        fail(`--pagx-images expects 'embed' or 'external', got '${mode}'`);
      }
      opts.pagxImages = mode;
    }
    else if (a === '--skip-existing') opts.skipExisting = true;
    else if (a === '--only') {
      const parts = String(argv[++i])
        .split(',')
        .map((s) => s.trim())
        .filter(Boolean);
      opts.only.push(...parts);
    }
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
      if (only.length && !only.some((s) => rel.includes(s))) continue;
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
    // Per-frame sample times (seconds) + the global timeline length, so the HTML report can play
    // the captured frames back as an animation on the real timeline clock (dynamic playback mode).
    times: [],
    globalDurationMs: 0,
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
  row.times = samples.map((s) => (typeof s.timeSec === 'number' ? s.timeSec : 0));
  row.globalDurationMs = typeof samplesMeta.globalDurationMs === 'number' ? samplesMeta.globalDurationMs : 0;
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
      // Image download writes the page's images into the shared cache and
      // rewrites the subset HTML to reference them by absolute path. The path is
      // baked into the .pagx by `pagx import`, so render reads the files
      // directly — no per-case manifest / `--fallback` wiring needed.
      downloadImages: opts.downloadImages,
      imageDir: opts.downloadImages ? opts.imageCacheDir : '',
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
      // Image resources (both <img> and inline-SVG <image>) only exist after
      // resolve, so image storage is applied here. The subset lives in the case
      // dir, but its relative on-disk images are addressed relative to the
      // original HTML — point relocation there.
      imageStorage: opts.pagxImages,
      imageBaseDir: path.dirname(htmlPath),
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

  // Per-case data the in-page player needs: where the frames live and the real sample times so the
  // flip-book plays on the same clock the baseline / pagx render were sampled on.
  const playerData = rows.map((r) => ({
    name: r.name,
    dir: encodeURIComponent(r.name),
    frames: r.error ? 0 : r.frames,
    times: Array.isArray(r.times) ? r.times : [],
    globalDurationMs: r.globalDurationMs || 0,
  }));

  const cards = rows.map((r, ci) => {
    const dir = encodeURIComponent(r.name);
    const cls = ssimClass(r.ssimMean);
    const status = r.error ? `<span class="err">${r.error}</span>` : '';
    let body = '';
    if (!r.error && r.frames > 0) {
      // Dynamic playback: one larger frame per channel that the JS swaps in time. The static strips
      // stay below it (collapsed by default) for frame-by-frame inspection.
      const player = `
  <div class="player" data-case="${ci}">
    <div class="stage">
      <figure><figcaption>baseline</figcaption><img class="play-img" data-sub="baseline" src="${dir}/baseline/frame-000.png"/></figure>
      <figure><figcaption>pagx</figcaption><img class="play-img" data-sub="render" src="${dir}/render/frame-000.png"/></figure>
      <figure><figcaption>diff</figcaption><img class="play-img" data-sub="diff" src="${dir}/diff/frame-000.png"/></figure>
    </div>
    <div class="controls">
      <button class="play-btn" type="button">▶ Play</button>
      <input class="scrub" type="range" min="0" max="${r.frames - 1}" value="0" step="1"/>
      <span class="time-label">frame 1/${r.frames}</span>
      <label class="speed">speed
        <select class="speed-sel">
          <option value="0.25">0.25×</option>
          <option value="0.5">0.5×</option>
          <option value="1" selected>1×</option>
          <option value="2">2×</option>
          <option value="4">4×</option>
        </select>
      </label>
      <label class="loop"><input type="checkbox" class="loop-chk" checked/> loop</label>
    </div>
  </div>`;
      // One column per frame: the frame number once (shared by all three channels), then the
      // baseline / pagx / diff thumbnails stacked. A single scroll container holds every column, so
      // there is exactly one scrollbar and one number row driving all three channels.
      const subs = [['baseline', 'baseline'], ['render', 'pagx'], ['diff', 'diff']];
      let cells = '';
      for (let i = 0; i < r.frames; i++) {
        const idx = String(i).padStart(3, '0');
        let imgs = '';
        for (const [sub] of subs) imgs += `<img src="${dir}/${sub}/frame-${idx}.png" loading="lazy"/>`;
        cells += `<div class="frame-cell" data-frame="${i}"><span class="frame-no">${i + 1}</span>${imgs}</div>`;
      }
      const labels = '<span class="lbl-spacer"></span>' + subs.map(([, l]) => `<span class="strip-label">${l}</span>`).join('');
      const strips = `<div class="strips-body"><div class="strip-labels">${labels}</div><div class="frames">${cells}</div></div>`;
      body = `${player}
  <details class="strips">
    <summary>frame strip</summary>
    ${strips}
  </details>`;
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
  ${body}
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
  .toolbar { display: flex; align-items: center; gap: 12px; margin: 12px 0 4px; }
  .toolbar button { font-size: 13px; padding: 6px 14px; border: 1px solid #cbd5e1; border-radius: 6px; background: #fff; cursor: pointer; }
  .toolbar button:hover { background: #f1f5f9; }
  .case { border-top: 1px solid #e5e7eb; padding-top: 18px; margin-top: 18px; }
  .case h2 { margin: 0 0 6px; font-size: 18px; display: flex; align-items: center; gap: 10px; }
  .ssim-tag { font-size: 12px; font-weight: 600; padding: 2px 8px; border-radius: 999px; }
  .ssim-tag.good { background: #dcfce7; color: #166534; }
  .ssim-tag.mid  { background: #fef9c3; color: #854d0e; }
  .ssim-tag.bad  { background: #fee2e2; color: #991b1b; }
  .ssim-tag.na   { background: #e5e7eb; color: #475569; }
  .err { color: #b00; font-weight: 600; }
  .muted { color: #94a3b8; font-size: 12px; }
  .player { margin: 8px 0; }
  .stage { display: flex; gap: 16px; flex-wrap: wrap; }
  .stage figure { margin: 0; }
  .stage figcaption { font-size: 12px; color: #64748b; margin-bottom: 4px; }
  .stage img { height: 260px; width: auto; max-width: 100%; background: #f1f5f9; border-radius: 6px; image-rendering: pixelated; display: block; }
  .controls { display: flex; align-items: center; gap: 12px; margin-top: 10px; flex-wrap: wrap; }
  .controls .play-btn { font-size: 13px; padding: 5px 14px; border: 1px solid #cbd5e1; border-radius: 6px; background: #fff; cursor: pointer; min-width: 84px; }
  .controls .play-btn:hover { background: #f1f5f9; }
  .controls .scrub { flex: 1 1 220px; min-width: 160px; }
  .controls .time-label { font-size: 12px; color: #475569; font-variant-numeric: tabular-nums; min-width: 110px; }
  .controls .speed, .controls .loop { font-size: 12px; color: #64748b; display: inline-flex; align-items: center; gap: 4px; }
  .strips { margin-top: 12px; }
  .strips summary { font-size: 12px; color: #64748b; cursor: pointer; }
  .strips-body { display: flex; align-items: flex-start; gap: 10px; margin: 8px 0; }
  .strip-labels { display: flex; flex-direction: column; gap: 6px; flex: none; }
  .strip-labels .lbl-spacer { height: 12px; }
  .strip-labels .strip-label { height: 140px; display: flex; align-items: center; font-size: 12px; color: #64748b; }
  .frames { display: flex; gap: 6px; overflow-x: auto; }
  .frame-cell { display: flex; flex-direction: column; align-items: center; gap: 6px; flex: none; cursor: pointer; }
  .frame-no { height: 12px; line-height: 12px; font-size: 10px; color: #94a3b8; font-variant-numeric: tabular-nums; }
  .frames img { height: 140px; width: auto; background: #f1f5f9; border-radius: 4px; image-rendering: pixelated; display: block; }
  .frame-cell.active .frame-no { color: #2563eb; font-weight: 700; }
  .frame-cell.active img { outline: 2px solid #2563eb; outline-offset: -2px; }
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
<div class="toolbar">
  <button id="play-all" type="button">▶ Play all</button>
  <span class="muted">dynamic playback — frames flip on their captured timeline clock</span>
</div>
${cards}
<script>
const CASES = ${JSON.stringify(playerData)};

// One flip-book player per case. Frames are sparse samples taken at CASES[i].times (seconds) on the
// captured timeline. Playback runs on a bounded wall-clock loop (this.playbackMs) and maps back onto
// that captured timeline to pick each frame, so cases with a degenerate captured clock (e.g. GSAP's
// ~1e10 s infinite-timeline sentinel) still flip through every frame at a watchable rate.
class Player {
  constructor(root, data) {
    this.root = root;
    this.data = data;
    this.imgs = Array.from(root.querySelectorAll('.play-img'));
    this.btn = root.querySelector('.play-btn');
    this.scrub = root.querySelector('.scrub');
    this.timeLabel = root.querySelector('.time-label');
    this.speedSel = root.querySelector('.speed-sel');
    this.loopChk = root.querySelector('.loop-chk');
    this.frame = 0;
    this.playing = false;
    this.elapsedMs = 0;
    this.lastTs = 0;
    this.raf = 0;

    const times = data.times && data.times.length ? data.times : [];
    this.times = times;
    // Duration of the loop in ms: prefer the captured global duration, else last sample time.
    this.durationMs = data.globalDurationMs > 0
      ? data.globalDurationMs
      : (times.length ? times[times.length - 1] * 1000 : 0);
    if (!(this.durationMs > 0)) this.durationMs = Math.max(1, data.frames) * 1000;
    // Show real seconds for normal clocks; fall back to progress % when the captured clock is
    // degenerate (e.g. GSAP's infinite-timeline sentinel reports ~1e10 s, so there is no
    // human-meaningful seconds value to display).
    this.showSeconds = this.durationMs > 0 && this.durationMs <= 600000;
    // Wall-clock loop length. Normal clocks play in real time (bounded so a slow loop stays
    // watchable). A degenerate clock has no usable real duration, so we synthesise one from a fixed
    // per-frame cadence — matching the ~50 ms/frame the real-clock cases run at — instead of letting
    // the giant duration peg the loop to the 8 s ceiling (which made GSAP cases play far slower than
    // the others). Samples are evenly spaced, so a constant cadence reproduces the motion faithfully.
    this.playbackMs = this.showSeconds
      ? Math.min(Math.max(this.durationMs, 600), 8000)
      : Math.min(Math.max(Math.max(2, data.frames) * 50, 600), 8000);

    this.btn.addEventListener('click', () => this.toggle());
    this.scrub.addEventListener('input', () => {
      this.pause();
      this.setFrame(parseInt(this.scrub.value, 10) || 0);
      this.elapsedMs = this.progressForFrame(this.frame) * this.playbackMs;
    });

    // Frame strip: one column per frame (number + baseline/pagx/diff stacked) in a single scroll
    // container. The player highlights the active frame's column and clicking a column seeks.
    const section = root.closest('.case') || document;
    this.stripBox = section.querySelector('.strips .frames');
    this.stripCells = this.stripBox ? Array.from(this.stripBox.querySelectorAll('.frame-cell')) : [];
    if (this.stripBox) {
      this.stripBox.addEventListener('click', (e) => {
        const cell = e.target.closest('.frame-cell');
        if (!cell || !this.stripBox.contains(cell)) return;
        this.pause();
        this.setFrame(parseInt(cell.getAttribute('data-frame'), 10) || 0);
        this.elapsedMs = this.progressForFrame(this.frame) * this.playbackMs;
        this.centerStrips();
      });
    }

    this.setFrame(0);
  }

  // Highlight the active frame's column.
  highlightStrips() {
    for (let i = 0; i < this.stripCells.length; i++) {
      this.stripCells[i].classList.toggle('active', i === this.frame);
    }
  }

  // Scroll the strip so the active frame's column is centred.
  centerStrips() {
    const box = this.stripBox;
    const cell = this.stripCells[this.frame];
    if (!box || !cell) return;
    const target = cell.offsetLeft - (box.clientWidth - cell.offsetWidth) / 2;
    box.scrollLeft = Math.max(0, target);
  }

  frameTimeMs(i) {
    if (this.times.length > i && typeof this.times[i] === 'number') return this.times[i] * 1000;
    if (this.data.frames > 1) return (i / (this.data.frames - 1)) * this.durationMs;
    return 0;
  }

  // Normalised position (0..1) of frame i on the captured timeline.
  progressForFrame(i) {
    if (this.durationMs > 0) return this.frameTimeMs(i) / this.durationMs;
    if (this.data.frames > 1) return i / (this.data.frames - 1);
    return 0;
  }

  frameForTimeMs(ms) {
    // Last frame whose sample time is <= ms.
    let f = 0;
    for (let i = 0; i < this.data.frames; i++) {
      if (this.frameTimeMs(i) <= ms + 0.001) f = i; else break;
    }
    return f;
  }

  setFrame(i) {
    const n = this.data.frames;
    if (n <= 0) return;
    this.frame = ((i % n) + n) % n;
    const idx = String(this.frame).padStart(3, '0');
    for (const img of this.imgs) {
      const sub = img.getAttribute('data-sub');
      img.src = this.data.dir + '/' + sub + '/frame-' + idx + '.png';
    }
    this.scrub.value = String(this.frame);
    let suffix;
    if (this.showSeconds) {
      suffix = '(' + (this.frameTimeMs(this.frame) / 1000).toFixed(2) + 's)';
    } else {
      suffix = '(' + Math.round(this.progressForFrame(this.frame) * 100) + '%)';
    }
    this.timeLabel.textContent = 'frame ' + (this.frame + 1) + '/' + n + '  ' + suffix;
    this.highlightStrips();
  }

  toggle() { this.playing ? this.pause() : this.play(); }

  play() {
    if (this.playing || this.data.frames <= 1) return;
    this.playing = true;
    this.btn.textContent = '❚❚ Pause';
    this.lastTs = performance.now();
    if (this.elapsedMs >= this.playbackMs) this.elapsedMs = 0;
    const tick = (ts) => {
      if (!this.playing) return;
      const speed = parseFloat(this.speedSel.value) || 1;
      this.elapsedMs += (ts - this.lastTs) * speed;
      this.lastTs = ts;
      if (this.elapsedMs >= this.playbackMs) {
        if (this.loopChk.checked) {
          this.elapsedMs = this.elapsedMs % this.playbackMs;
        } else {
          this.elapsedMs = this.playbackMs;
          this.setFrame(this.data.frames - 1);
          this.pause();
          return;
        }
      }
      // Map wall-clock elapsed back onto the captured timeline to pick the frame.
      const prev = this.frame;
      this.setFrame(this.frameForTimeMs((this.elapsedMs / this.playbackMs) * this.durationMs));
      if (this.frame !== prev) this.centerStrips();
      this.raf = requestAnimationFrame(tick);
    };
    this.raf = requestAnimationFrame(tick);
  }

  pause() {
    this.playing = false;
    this.btn.textContent = '▶ Play';
    if (this.raf) cancelAnimationFrame(this.raf);
    this.raf = 0;
  }
}

const players = [];
document.querySelectorAll('.player').forEach((el) => {
  const ci = parseInt(el.getAttribute('data-case'), 10);
  if (CASES[ci] && CASES[ci].frames > 0) players.push(new Player(el, CASES[ci]));
});

const playAll = document.getElementById('play-all');
let allPlaying = false;
playAll.addEventListener('click', () => {
  allPlaying = !allPlaying;
  playAll.textContent = allPlaying ? '❚❚ Pause all' : '▶ Play all';
  for (const p of players) allPlaying ? p.play() : p.pause();
});
</script>
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

  // Single shared, content-addressed image cache for the whole run. Every case
  // downloads into it; identical images (e.g. a shared logo/hero across a
  // corpus) are stored once. The subset HTML references images by absolute
  // path, so no per-case manifest is needed.
  opts.imageCacheDir = path.join(opts.outDir, 'images');
  if (opts.downloadImages) ensureDir(opts.imageCacheDir);

  if (!fs.existsSync(opts.pagxBin)) {
    fail(`pagx binary not found: ${opts.pagxBin}`, 1);
  }
  const cases = findCorpusFiles(opts.corpus, opts.only, opts.recursive);
  if (cases.length === 0) {
    const hint = opts.recursive ? '' : ' (try --recursive to descend into sub-directories)';
    fail(`no html cases found in ${opts.corpus}${hint}`, 1);
  }
  const concurrency = Math.max(1, Math.min(opts.concurrency, cases.length));
  console.log(`anim-run: ${cases.length} cases → ${opts.outDir}  (samples=${opts.samples}, concurrency=${concurrency}${opts.downloadImages ? ', download-images=on' : ''}, pagx-images=${opts.pagxImages})`);

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

if (require.main === module) {
  main().catch((err) => {
    console.error(err && err.stack ? err.stack : err);
    process.exit(1);
  });
}

module.exports = { writeIndexHtml, writeMarkdown, writeCsv, summarize };
