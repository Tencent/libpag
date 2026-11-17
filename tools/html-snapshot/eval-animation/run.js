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
 *
 * `--source pagx` evaluates the reverse direction over a corpus of hand-authored .pagx files
 * (resources/pagx_to_html/animations): the reference timeline is `pagx render --time` of the source
 * PAGX (written to <case>/baseline/) and the candidate is `pagx export` HTML seeked in the browser at
 * the same times (written to <case>/render/). Sample times are derived from the PAGX animations
 * themselves, so PAGX-only semantics (evaluationOffset, keys not spanning the duration) are covered.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const compare = require('../eval/compare');
const { freezeToState } = require('./freeze');
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

  --corpus <dir>      Directory of original HTML files, or .pagx files with --source pagx
                      (required; or set $EVAL_CORPUS)
  --source <kind>     'html' (default): HTML -> PAGX import eval. 'pagx': PAGX -> HTML export
                      eval, comparing pagx render --time (reference) with the exported HTML in
                      the browser. --roundtrip / --download-images / --pagx-images /
                      --no-infer-flex do not apply to 'pagx', and --scale must stay 1.
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
  --browser-engine N  Headless driver forwarded to baseline-frames.js / snapshot.js
  --roundtrip         Also evaluate PAGX -> HTML -> browser & roundtrip PAGX`;

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
    roundtrip: false,
    source: 'html',
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--corpus') opts.corpus = argv[++i];
    else if (a === '--source') {
      const kind = argv[++i];
      if (kind !== 'html' && kind !== 'pagx') {
        fail(`--source expects 'html' or 'pagx', got '${kind}'`);
      }
      opts.source = kind;
    }
    else if (a === '--out') opts.outDir = argv[++i];
    else if (a === '--pagx-bin') opts.pagxBin = argv[++i];
    else if (a === '--samples') {
      const v = parseInt(argv[++i], 10);
      if (!Number.isFinite(v) || v < 1) fail(`--samples requires a positive integer, got '${argv[i]}'`);
      opts.samples = v;
      opts.samplesSet = true;
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
    else if (a === '--roundtrip') opts.roundtrip = true;
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

function findCorpusFiles(corpusDir, only, recursive, ext = '.html') {
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
      if (!name.toLowerCase().endsWith(ext)) continue;
      if (name.includes('.subset.')) continue;
      const rel = relPrefix ? `${relPrefix}/${name}` : name;
      if (only.length && !only.some((s) => rel.includes(s))) continue;
      const base = name.slice(0, -ext.length);
      const caseName = relPrefix
        ? `${relPrefix.replace(/[\\/]/g, '__')}__${base}`
        : base;
      out.push({ path: full, name: caseName, rel });
    }
  };
  visit(corpusDir, '');
  out.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
  return out;
}

// Parse `subset:animation-*` importer diagnostics plus the "captured N animation(s)" line the
// snapshot tool logs, so the report shows how much animation actually flowed through each stage.

// Identity of a cached case run: the source file as it currently sits on disk plus the sampling that
// produced the frames. Reusing frames across a changed source or a different sample grid would score
// an old run against expectations it was never rendered for.
function buildCaseCacheKey(sourcePath, opts) {
  let source = { size: -1, mtimeMs: -1 };
  try {
    const stat = fs.statSync(sourcePath);
    source = { size: stat.size, mtimeMs: stat.mtimeMs };
  } catch (err) {
    source = { size: -1, mtimeMs: -1 };
  }
  return { source, samples: opts.samples, roundtrip: !!opts.roundtrip };
}

function cacheKeyMatches(cacheKeyPath, expected) {
  try {
    const actual = JSON.parse(fs.readFileSync(cacheKeyPath, 'utf8'));
    return (
      actual.source &&
      actual.source.size === expected.source.size &&
      actual.source.mtimeMs === expected.source.mtimeMs &&
      actual.samples === expected.samples &&
      !!actual.roundtrip === expected.roundtrip
    );
  } catch (err) {
    return false;
  }
}

function writeCaseCacheKey(cacheKeyPath, key) {
  try {
    fs.writeFileSync(cacheKeyPath, JSON.stringify(key));
  } catch (err) {
    // The cache key only shortens re-runs; failing to persist it must not fail the case.
  }
}

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

async function runPagxExportHtml(opts) {
  const args = [
    'export',
    '--input', opts.pagxFile,
    '--output', opts.htmlFile,
    '--format', 'html',
  ];
  return spawnCapture(opts.pagxBin, args, { stderrPath: opts.stderrPath });
}

async function processCase(entry, outDir, opts) {
  const htmlPath = entry.path;
  const base = entry.name;
  const caseDir = path.join(outDir, base);
  // Cached frames are only reusable for the same source and the same sample grid.
  const cacheKeyPath = path.join(caseDir, '.cache-key.json');
  const skipExisting = opts.skipExisting &&
    cacheKeyMatches(cacheKeyPath, buildCaseCacheKey(entry.path, opts));
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
    exportBrowserSsimMean: NaN,
    exportBrowserVsPagxSsimMean: NaN,
    roundtripPagxSsimMean: NaN,
    roundtripVsPagxSsimMean: NaN,
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
    roundtrip: !!opts.roundtrip,
    error: '',
  };

  const engineArgs = opts.browserEngine ? ['--browser-engine', opts.browserEngine] : [];

  // 1. baseline frames (ground-truth timeline)
  if (!skipExisting || !fs.existsSync(samplesJson)) {
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
  if (!skipExisting || !fs.existsSync(subsetHtml)) {
    const r = await forkSnapshotCli({
      input: htmlPath,
      output: subsetHtml,
      scriptDir: TOOL_DIR,
      browserEngine: opts.browserEngine || undefined,
      // This is the *animation* eval: capture the page's motion into the subset
      // so the timeline diff has something to compare. Snapshot capture is off
      // by default (static frame), so opt in explicitly here.
      captureAnimations: true,
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
  if (!skipExisting || !fs.existsSync(subsetPagx)) {
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

    if (!skipExisting || !fs.existsSync(renderPng)) {
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

  // 7. Optional round-trip evaluation:
  //    subset.pagx -> exported.html -> (A) browser frames via baseline-frames.js
  //                                 -> (B) roundtrip.pagx -> render frames
  if (opts.roundtrip) {
    const exportedHtml = path.join(caseDir, 'exported.html');
    const roundtripPagx = path.join(caseDir, 'roundtrip.pagx');
    const exportedBrowserDir = path.join(caseDir, 'exported-browser');
    const roundtripRenderDir = path.join(caseDir, 'roundtrip-render');
    ensureDir(exportedBrowserDir);
    ensureDir(roundtripRenderDir);

    if (!skipExisting || !fs.existsSync(exportedHtml)) {
      const r = await runPagxExportHtml({
        pagxBin: opts.pagxBin,
        pagxFile: subsetPagx,
        htmlFile: exportedHtml,
        stderrPath: path.join(caseDir, 'export.stderr.txt'),
      });
      if (r.code !== 0 || !fs.existsSync(exportedHtml)) {
        row.error = 'export-html-failed';
        return row;
      }
    }

    // 7A. Render exported.html in headless Chromium at the exact same sample timestamps
    const exportedSamplesJson = path.join(exportedBrowserDir, 'samples.json');
    if (!skipExisting || !fs.existsSync(exportedSamplesJson)) {
      const r = await spawnCapture(
        process.execPath,
        [
          path.join(SCRIPT_DIR, 'baseline-frames.js'), exportedHtml,
          '-o', exportedBrowserDir,
          '--samples', String(opts.samples),
          '--times-json', samplesJson,
          ...engineArgs,
        ],
        { stderrPath: path.join(caseDir, 'exported-browser.stderr.txt') },
      );
      if (r.code !== 0 || !fs.existsSync(exportedSamplesJson)) {
        row.error = 'exported-browser-failed';
        return row;
      }
    }

    // 7B. Snapshot exported.html -> roundtrip.subset.html -> roundtrip.pagx -> resolve -> render frames
    const roundtripSubsetHtml = path.join(caseDir, 'roundtrip.subset.html');
    if (!skipExisting || !fs.existsSync(roundtripPagx)) {
      const rSnap = await forkSnapshotCli({
        input: exportedHtml,
        output: roundtripSubsetHtml,
        scriptDir: TOOL_DIR,
        browserEngine: opts.browserEngine || undefined,
        captureAnimations: true,
        downloadImages: opts.downloadImages,
        imageDir: opts.downloadImages ? opts.imageCacheDir : '',
        stderrPath: path.join(caseDir, 'roundtrip-snapshot.stderr.txt'),
      });
      if (rSnap.code !== 0 || !fs.existsSync(roundtripSubsetHtml)) {
        row.error = 'roundtrip-snapshot-failed';
        return row;
      }
      const rImp = await runPagxImportToFile({
        pagxBin: opts.pagxBin,
        subsetHtml: roundtripSubsetHtml,
        pagxFile: roundtripPagx,
        inferFlex: opts.inferFlex,
        stderrPath: path.join(caseDir, 'roundtrip-import.stderr.txt'),
      });
      if (rImp.code !== 0 || !fs.existsSync(roundtripPagx)) {
        row.error = 'roundtrip-import-failed';
        return row;
      }
      const rRes = await runPagxResolve({
        pagxBin: opts.pagxBin,
        pagxFile: roundtripPagx,
        imageStorage: opts.pagxImages,
        imageBaseDir: caseDir,
        stderrPath: path.join(caseDir, 'roundtrip-resolve.stderr.txt'),
      });
      if (rRes.code !== 0) {
        row.error = 'roundtrip-resolve-failed';
        return row;
      }
    }

    const expBrowserVsBaseSsims = [];
    const expBrowserVsPagxSsims = [];
    const rtPagxVsBaseSsims = [];
    const rtPagxVsPagxSsims = [];

    for (const s of samples) {
      const idx = String(s.index).padStart(3, '0');
      const baselinePng = path.join(baselineDir, `frame-${idx}.png`);
      const renderPng = path.join(renderDir, `frame-${idx}.png`);
      const expBrowserPng = path.join(exportedBrowserDir, `frame-${idx}.png`);
      const rtRenderPng = path.join(roundtripRenderDir, `frame-${idx}.png`);

      if (!skipExisting || !fs.existsSync(rtRenderPng)) {
        const r = await runPagxRenderAtTime({
          pagxBin: opts.pagxBin,
          pagxFile: roundtripPagx,
          output: rtRenderPng,
          scale: opts.scale,
          timeSec: s.timeSec,
          stderrPath: path.join(caseDir, `roundtrip-render.${idx}.stderr.txt`),
        });
        if (r.code !== 0 || !fs.existsSync(rtRenderPng)) {
          row.error = `roundtrip-render-failed@${idx}`;
          return row;
        }
      }

      try {
        expBrowserVsBaseSsims.push(compare.pixelMetrics(baselinePng, expBrowserPng, null).ssim);
        expBrowserVsPagxSsims.push(compare.pixelMetrics(renderPng, expBrowserPng, null).ssim);
        rtPagxVsBaseSsims.push(compare.pixelMetrics(baselinePng, rtRenderPng, null).ssim);
        rtPagxVsPagxSsims.push(compare.pixelMetrics(renderPng, rtRenderPng, null).ssim);
      } catch (err) {
        row.error = `roundtrip-compare-failed@${idx}: ${err.message}`;
        return row;
      }
    }

    row.exportBrowserSsimMean = mean(expBrowserVsBaseSsims);
    row.exportBrowserVsPagxSsimMean = mean(expBrowserVsPagxSsims);
    row.roundtripPagxSsimMean = mean(rtPagxVsBaseSsims);
    row.roundtripVsPagxSsimMean = mean(rtPagxVsPagxSsims);
  }

  return row;
}

// ---------------------------------------------------------------------------------------------
// --source pagx: PAGX -> HTML export eval
// ---------------------------------------------------------------------------------------------

const PAGX_DEFAULT_SAMPLES = 8;
// Sample windows longer than this are clamped so a long loop does not stretch the sample grid.
const PAGX_MAX_WINDOW_SEC = 10;
// Mean per-channel RGB delta (0-255 levels) from the first frame below which a timeline counts as
// still. RGB rather than SSIM because the shared SSIM is luma-only and a hue-only animation (e.g.
// blue to orange) barely moves luma.
const PAGX_STATIC_RGB_EPSILON = 0.01;

function readXmlAttr(tag, name) {
  const m = tag.match(new RegExp(`\\s${name}="([^"]*)"`));
  return m ? m[1] : null;
}

// Parses the structured header comment every corpus file carries (see the corpus prompt):
//   Tests: ...  Target: <NodeType>  Channel: <name>  Type: <type>  Expect: <supported|gap>  Ref: ...
function parsePagxCaseHeader(xml) {
  const comment = (xml.match(/<!--([\s\S]*?)-->/) || [])[1] || '';
  const pick = (re) => {
    const m = comment.match(re);
    return m ? m[1].trim() : '';
  };
  return {
    target: pick(/Target:\s*(\S+)/),
    channel: pick(/Channel:\s*(\S+)/),
    expect: pick(/Expect:\s*(\w+)/) || 'unknown',
    ref: pick(/Ref:\s*(\S+)/),
  };
}

// Derives the playback window from the PAGX animations: each definition contributes its period
// (doubled for pingPong) shifted by the largest positive evaluationOffset any timeline mounts it
// with. Top-level animations without a <Timelines> mount have offset 0.
function parsePagxTiming(xml) {
  const defs = [];
  const offsets = new Map();
  const tagRe = /<Animation\b[^>]*>/g;
  let m;
  while ((m = tagRe.exec(xml)) !== null) {
    const tag = m[0];
    const ref = readXmlAttr(tag, 'ref');
    if (ref !== null) {
      const id = ref.replace(/^@/, '');
      const offset = parseFloat(readXmlAttr(tag, 'evaluationOffset') || '0') || 0;
      offsets.set(id, Math.max(offsets.has(id) ? offsets.get(id) : 0, offset));
      continue;
    }
    const id = readXmlAttr(tag, 'id');
    if (id === null) continue;
    defs.push({
      id,
      duration: parseFloat(readXmlAttr(tag, 'duration') || '0') || 0,
      frameRate: parseFloat(readXmlAttr(tag, 'frameRate') || '60') || 60,
      loop: readXmlAttr(tag, 'loop') || 'once',
    });
  }
  let windowSec = 0;
  for (const d of defs) {
    const periodFrames = d.duration * (d.loop === 'pingPong' ? 2 : 1);
    const offsetFrames = Math.max(0, offsets.get(d.id) || 0);
    windowSec = Math.max(windowSec, (offsetFrames + periodFrames) / d.frameRate);
  }
  return {
    frameRates: Array.from(new Set(defs.map((d) => d.frameRate))),
    windowSec: Math.min(windowSec, PAGX_MAX_WINDOW_SEC),
  };
}

// Evenly spaced sample times strictly inside (0, window). A time that lands within a quarter frame
// of an integer frame is nudged by half a frame: keys sit on integer frames, and the browser and the
// PAGX runtime may round a sample sitting exactly on a key boundary to different sides.
function pagxSampleTimes(timing, count) {
  const times = [];
  if (!(timing.windowSec > 0)) return times;
  for (let i = 0; i < count; i++) {
    let t = (timing.windowSec * (i + 0.5)) / count;
    for (const fr of timing.frameRates) {
      const f = t * fr;
      if (Math.abs(f - Math.round(f)) < 0.25) t += 0.5 / fr;
    }
    times.push(Math.round(t * 1e6) / 1e6);
  }
  return times;
}

// Largest mean RGB delta between the first frame and any later frame, i.e. how far the timeline
// travels away from its starting picture. 0 means every frame matches the first one.
function pagxTimelineSpan(pngPaths) {
  let span = 0;
  for (let i = 1; i < pngPaths.length; i++) {
    span = Math.max(span, compare.pixelMetrics(pngPaths[0], pngPaths[i], null).meanRgbDelta);
  }
  return span;
}

// Classifies a case whose timelines did not both move, so a broken reference render is not
// mistaken for an export regression:
//   ref-static    reference (pagx render) never changes but the exported HTML does
//   export-static reference moves but the exported HTML never changes (animation lost on export)
//   both-static   neither moves (the case or the channel does not animate at all)
function classifyPagxMotion(refStatic, exportStatic) {
  if (refStatic && exportStatic) return 'both-static';
  if (refStatic) return 'ref-static';
  if (exportStatic) return 'export-static';
  return '';
}

async function processPagxCase(entry, outDir, opts) {
  const pagxPath = entry.path;
  const caseDir = path.join(outDir, entry.name);
  // Cached frames are only reusable for the same source and the same sample grid.
  const cacheKeyPath = path.join(caseDir, '.cache-key.json');
  const skipExisting = opts.skipExisting &&
    cacheKeyMatches(cacheKeyPath, buildCaseCacheKey(entry.path, opts));
  // Directory layout mirrors the HTML eval so the shared report player works unchanged:
  // baseline/ = reference (pagx render --time), render/ = candidate (exported HTML in the browser).
  const baselineDir = path.join(caseDir, 'baseline');
  const renderDir = path.join(caseDir, 'render');
  const diffDir = path.join(caseDir, 'diff');
  ensureDir(baselineDir);
  ensureDir(renderDir);
  ensureDir(diffDir);

  const relDir = path.dirname(entry.rel);
  const row = {
    source: 'pagx',
    name: entry.name,
    group: relDir === '.' ? '(root)' : relDir.split(/[\\/]/)[0],
    target: '',
    channel: '',
    expect: 'unknown',
    ref: '',
    frames: 0,
    ssimMean: NaN,
    ssimMin: NaN,
    pixelDiffMean: NaN,
    rgbDeltaMean: NaN,
    rgbDeltaMax: NaN,
    motion: NaN,
    exportMotion: NaN,
    // Static baseline (see measureStaticBaseline) plus the fidelity grade it feeds.
    staticSSIM0: NaN,
    staticSSIMworst: NaN,
    animSSIM0: NaN,
    animSSIMworst: NaN,
    freezeSSIM0: NaN,
    freezeSSIMworst: NaN,
    stateSSIM0: NaN,
    stateSSIMworst: NaN,
    staticAvailable: 'no',
    staticNote: '',
    // Per-instant animation loss (staticSSIM_i - animSSIM_i, signed) and the clamped worst of the
    // two, which is what the grade uses.
    animLoss0: NaN,
    animLossWorst: NaN,
    animLoss: NaN,
    animGradeMean: '',
    animGradeMin: '',
    grade: '',
    attribution: '',
    flag: '',
    animCaptured: 0,
    animWarnings: 0,
    animDropped: 0,
    width: 0,
    height: 0,
    times: [],
    globalDurationMs: 0,
    roundtrip: false,
    error: '',
  };

  let xml;
  try {
    xml = fs.readFileSync(pagxPath, 'utf8');
  } catch (err) {
    row.error = `read-failed: ${err.message}`;
    return row;
  }
  Object.assign(row, parsePagxCaseHeader(xml));
  const rootTag = (xml.match(/<pagx\b[^>]*>/) || [''])[0];
  row.width = parseInt(readXmlAttr(rootTag, 'width') || '0', 10) || 0;
  row.height = parseInt(readXmlAttr(rootTag, 'height') || '0', 10) || 0;

  const timing = parsePagxTiming(xml);
  const times = pagxSampleTimes(timing, opts.samples);
  if (times.length === 0) {
    row.error = 'no-animation-window';
    return row;
  }
  row.times = times;
  row.globalDurationMs = timing.windowSec * 1000;
  const samples = times.map((timeSec, index) => ({ index, timeSec }));
  const samplesJson = path.join(baselineDir, 'samples.json');
  fs.writeFileSync(
    samplesJson,
    JSON.stringify(
      { width: row.width, height: row.height, globalDurationMs: row.globalDurationMs, samples },
      null,
      2,
    ),
    'utf8',
  );

  // 1. reference frames: the PAGX runtime at each sample time
  const refFrames = [];
  for (const s of samples) {
    const idx = String(s.index).padStart(3, '0');
    const refPng = path.join(baselineDir, `frame-${idx}.png`);
    if (!skipExisting || !fs.existsSync(refPng)) {
      const r = await runPagxRenderAtTime({
        pagxBin: opts.pagxBin,
        pagxFile: pagxPath,
        output: refPng,
        scale: 1,
        timeSec: s.timeSec,
        stderrPath: path.join(caseDir, `reference.${idx}.stderr.txt`),
      });
      if (r.code !== 0 || !fs.existsSync(refPng)) {
        row.error = `reference-render-failed@${idx}`;
        return row;
      }
    }
    refFrames.push(refPng);
  }

  // 2. export
  const exportedHtml = path.join(caseDir, 'exported.html');
  if (!skipExisting || !fs.existsSync(exportedHtml)) {
    const r = await runPagxExportHtml({
      pagxBin: opts.pagxBin,
      pagxFile: pagxPath,
      htmlFile: exportedHtml,
      stderrPath: path.join(caseDir, 'export.stderr.txt'),
    });
    if (r.code !== 0 || !fs.existsSync(exportedHtml)) {
      row.error = 'export-html-failed';
      return row;
    }
  }

  // 3. candidate frames: the exported HTML seeked to the same times in the browser
  const engineArgs = opts.browserEngine ? ['--browser-engine', opts.browserEngine] : [];
  const browserSamplesJson = path.join(renderDir, 'samples.json');
  if (!skipExisting || !fs.existsSync(browserSamplesJson)) {
    const r = await spawnCapture(
      process.execPath,
      [
        path.join(SCRIPT_DIR, 'baseline-frames.js'), exportedHtml,
        '-o', renderDir,
        '--samples', String(samples.length),
        '--times-json', samplesJson,
        ...engineArgs,
      ],
      { stderrPath: path.join(caseDir, 'browser.stderr.txt') },
    );
    if (r.code !== 0 || !fs.existsSync(browserSamplesJson)) {
      row.error = 'browser-frames-failed';
      return row;
    }
  }

  // 4. per-frame metrics
  const exportFrames = [];
  const ssims = [];
  const pdiffs = [];
  const rgbDeltas = [];
  for (const s of samples) {
    const idx = String(s.index).padStart(3, '0');
    const refPng = path.join(baselineDir, `frame-${idx}.png`);
    const candPng = path.join(renderDir, `frame-${idx}.png`);
    exportFrames.push(candPng);
    try {
      const m = compare.pixelMetrics(refPng, candPng, path.join(diffDir, `frame-${idx}.png`));
      ssims.push(m.ssim);
      pdiffs.push(m.pixelDiffRatio);
      rgbDeltas.push(m.meanRgbDelta);
    } catch (err) {
      row.error = `compare-failed@${idx}: ${err.message}`;
      return row;
    }
  }

  row.frames = samples.length;
  row.ssimMean = mean(ssims);
  row.ssimMin = Math.min(...ssims);
  row.pixelDiffMean = mean(pdiffs);
  row.rgbDeltaMean = mean(rgbDeltas);
  row.rgbDeltaMax = Math.max(...rgbDeltas);
  row.motion = pagxTimelineSpan(refFrames);
  row.exportMotion = pagxTimelineSpan(exportFrames);
  row.flag = classifyPagxMotion(
    row.motion < PAGX_STATIC_RGB_EPSILON,
    row.exportMotion < PAGX_STATIC_RGB_EPSILON,
  );

  // 5. static baseline at t=0 and at the worst animSSIM frame, then the fidelity grade.
  const staticRow = await measureStaticBaseline({
    pagxPath,
    xml,
    caseDir,
    pagxBin: opts.pagxBin,
    browserEngine: opts.browserEngine,
    exportedHtml,
    times,
    ssims,
    refFrames,
    renderFrames: exportFrames,
    skipExisting,
  });
  Object.assign(row, staticRow);
  row.hasText = caseDrawsText(xml);
  gradePagxRow(row);
  return row;
}

// ---------------------------------------------------------------------------------------------
// Static baseline: freeze the PAGX at one instant, then compare the two renderers on that state.
//
// `animSSIM` (the case's ssimMean) diffs an animated PAGX render against the exported HTML seeked to
// the same time, so it charges the exporter for *both* its static fidelity and its animation wiring.
// Freezing separates them: the frozen document carries the channel values of that instant as plain
// attributes and no <Animations> at all, so `pagx render` of it and the browser rendering its static
// HTML export are two readings of one identical, motionless state. Whatever gap remains is the
// static ceiling; the gap between that and animSSIM is what the animation writer contributes.
//
// Two instants only (t=0 and the case's worst animSSIM frame): the freeze costs two renders plus the
// static exports, and every extra instant buys nothing for attribution.
// ---------------------------------------------------------------------------------------------

const STATIC_DIR = 'static';
// The frozen document is rendered through the PAGX *static* path while the reference animSSIM frame
// comes from its animated path, and those two paths can differ slightly on their own (measured:
// up to ~0.004 SSIM on a text-in-layer case). The structural `exact` flag, not this floor, is what
// decides availability; the floor only catches a baking bug the static analysis missed.
const STATIC_FREEZE_FLOOR = 0.99;

// spawnCapture resolves only after the child's pipes close, so a browser that outlives its parent
// (or a Playwright launch that never settles) leaves the promise pending even after its own timeout
// fired. Every static-baseline step therefore runs under a deadline of its own: a stuck capture
// costs one instant's evidence, not the whole run.
function withDeadline(promise, ms, label) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`${label}-timeout`)), ms);
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (err) => {
        clearTimeout(timer);
        reject(err);
      },
    );
  });
}

const STATIC_BROWSER_TIMEOUT_MS = 120000;
const STATIC_TOOL_TIMEOUT_MS = 60000;

function indexOfMin(values) {
  let best = -1;
  for (let i = 0; i < values.length; i++) {
    if (!isFinite(values[i])) continue;
    if (best < 0 || values[i] < values[best]) best = i;
  }
  return best < 0 ? 0 : best;
}

async function measureStaticBaseline(params) {
  const dir = path.join(params.caseDir, STATIC_DIR);
  ensureDir(dir);
  const row = {
    staticSSIM0: NaN,
    staticSSIMworst: NaN,
    animSSIM0: NaN,
    animSSIMworst: NaN,
    freezeSSIM0: NaN,
    freezeSSIMworst: NaN,
    stateSSIM0: NaN,
    stateSSIMworst: NaN,
    staticAvailable: 'no',
    staticNote: '',
  };
  const notes = [];
  let allExact = true;
  const engineArgs = params.browserEngine ? ['--browser-engine', params.browserEngine] : [];
  const worstIdx = indexOfMin(params.ssims);
  const instants = [
    // t=0 is outside the sample grid (samples sit strictly inside the window), so both of its
    // pictures — the reference render and the exported-HTML screenshot — have to be taken here.
    { tag: '0', timeSec: 0, animatedPng: null, animatedBrowserPng: null, field: '0' },
    {
      tag: 'worst',
      timeSec: params.times[worstIdx],
      animatedPng: params.refFrames[worstIdx],
      animatedBrowserPng: params.renderFrames[worstIdx],
      field: 'worst',
    },
  ];

  for (const instant of instants) {
    const stem = path.join(dir, instant.tag);
    const frozenPath = `${stem}.frozen.pagx`;
    const refPng = `${stem}.ref.png`;
    const htmlPath = `${stem}.html`;
    const browserDir = `${stem}.browser`;
    const browserPng = path.join(browserDir, 'frame-000.png');
    const timesJson = `${stem}.times.json`;
    const animatedPng = instant.animatedPng || `${stem}.animated.png`;
    // The exported HTML seeked to the same instant: animSSIM of *this* moment, needed because the
    // loss has to be a same-instant difference (staticSSIM_i - animSSIM_i), not a mean minus a
    // minimum. For the worst instant the case's own render frame already is that picture.
    const animatedBrowserDir = `${stem}.animated-browser`;
    const animatedBrowserPng = instant.animatedBrowserPng || path.join(animatedBrowserDir, 'frame-000.png');

    const frozen = freezeToState(params.xml, instant.timeSec);
    if (!frozen.exact) allExact = false;
    // Notes are kept even when the freeze is exact: `skipped:` records a channel the runtime
    // ignores, which is a registered difference worth carrying into the report/attribution.
    if (frozen.notes.length) notes.push(`${instant.tag}:${frozen.notes.join('+')}`);

    // Artifacts of an instant are reusable only if they were produced for this exact time: the
    // frozen document bakes that time into its attributes, so reusing one across a changed sample
    // grid would compare two different states.
    const metaPath = `${stem}.meta.json`;
    let reusable = false;
    try {
      reusable = JSON.parse(fs.readFileSync(metaPath, 'utf8')).timeSec === instant.timeSec;
    } catch (err) {
      reusable = false;
    }

    try {
      if (!reusable || !fs.existsSync(frozenPath)) {
        fs.writeFileSync(frozenPath, frozen.xml, 'utf8');
      }
      if (!reusable || !fs.existsSync(refPng)) {
        const r = await withDeadline(
          runPagxRenderAtTime({
            pagxBin: params.pagxBin,
            pagxFile: frozenPath,
            output: refPng,
            scale: 1,
            timeSec: 0,
            stderrPath: `${stem}.ref.stderr.txt`,
          }),
          STATIC_TOOL_TIMEOUT_MS,
          'ref-render',
        );
        if (r.code !== 0 || !fs.existsSync(refPng)) throw new Error('ref-render-failed');
      }
      // The same-time render of the *animated* source: the self-check that the frozen document
      // really reproduces the picture the runtime shows at this instant.
      if (!reusable || !fs.existsSync(animatedPng)) {
        const r = await withDeadline(
          runPagxRenderAtTime({
            pagxBin: params.pagxBin,
            pagxFile: params.pagxPath,
            output: animatedPng,
            scale: 1,
            timeSec: instant.timeSec,
            stderrPath: `${stem}.animated.stderr.txt`,
          }),
          STATIC_TOOL_TIMEOUT_MS,
          'animated-render',
        );
        if (r.code !== 0 || !fs.existsSync(animatedPng)) throw new Error('animated-render-failed');
      }
      if (!reusable || !fs.existsSync(htmlPath)) {
        const r = await withDeadline(
          runPagxExportHtml({
            pagxBin: params.pagxBin,
            pagxFile: frozenPath,
            htmlFile: htmlPath,
            stderrPath: `${stem}.export.stderr.txt`,
          }),
          STATIC_TOOL_TIMEOUT_MS,
          'static-export',
        );
        if (r.code !== 0 || !fs.existsSync(htmlPath)) throw new Error('static-export-failed');
      }
      // A document without <Animations> needs no seek: capture it at its own resting state. The
      // times file exists only because baseline-frames.js drives its capture loop from one.
      fs.writeFileSync(
        timesJson,
        JSON.stringify({ width: 0, height: 0, globalDurationMs: 0, samples: [{ index: 0, timeSec: 0 }] }),
        'utf8',
      );
      if (!reusable || !fs.existsSync(browserPng)) {
        const r = await withDeadline(
          spawnCapture(
            process.execPath,
            [
              path.join(SCRIPT_DIR, 'baseline-frames.js'), htmlPath,
              '-o', browserDir,
              '--samples', '1',
              '--times-json', timesJson,
              ...engineArgs,
            ],
            { stderrPath: `${stem}.browser.stderr.txt` },
          ),
          STATIC_BROWSER_TIMEOUT_MS,
          'static-browser',
        );
        if (r.code !== 0 || !fs.existsSync(browserPng)) throw new Error('static-browser-failed');
      }

      // animSSIM of this instant: the exported HTML seeked to it against the animated reference.
      if (!fs.existsSync(animatedBrowserPng)) {
        fs.writeFileSync(
          `${stem}.animated.times.json`,
          JSON.stringify({ width: 0, height: 0, globalDurationMs: 0, samples: [{ index: 0, timeSec: instant.timeSec }] }),
          'utf8',
        );
        const r = await withDeadline(
          spawnCapture(
            process.execPath,
            [
              path.join(SCRIPT_DIR, 'baseline-frames.js'), params.exportedHtml,
              '-o', animatedBrowserDir,
              '--samples', '1',
              '--times-json', `${stem}.animated.times.json`,
              ...engineArgs,
            ],
            { stderrPath: `${stem}.animated-browser.stderr.txt` },
          ),
          STATIC_BROWSER_TIMEOUT_MS,
          'animated-browser',
        );
        if (r.code !== 0 || !fs.existsSync(animatedBrowserPng)) throw new Error('animated-browser-failed');
      }

      // Publish the self-check pair under both instants, so `<tag>.ref.png` (the frozen document's
      // render) and `<tag>.animated.png` (the source at the same instant) sit side by side. For the
      // worst instant that picture already exists as the case's reference frame; copying it keeps
      // the artifact set symmetric without rendering the same frame twice.
      const animatedCopy = `${stem}.animated.png`;
      if (path.resolve(animatedCopy) !== path.resolve(animatedPng) && !fs.existsSync(animatedCopy)) {
        fs.copyFileSync(animatedPng, animatedCopy);
      }
      if (path.resolve(refPng) === path.resolve(animatedPng) || path.resolve(refPng) === path.resolve(animatedCopy)) {
        throw new Error('freeze-pair-same-file');
      }

      const staticSSIM = compare.pixelMetrics(refPng, browserPng, `${stem}.diff.png`).ssim;
      const freezeSSIM = compare.pixelMetrics(animatedPng, refPng, null).ssim;
      const animSSIM = compare.pixelMetrics(animatedPng, animatedBrowserPng, null).ssim;
      // Did the two renderers agree that the frozen state is the animated one? Two independent
      // readings: the PAGX renders (`freezeSSIM`, freeze correctness) and the browser renderings of
      // both exports (`stateSSIM`, whether the static export route reproduces that same state at
      // all). When the static route diverges, staticSSIM measures the route gap, not fidelity.
      const stateSSIM = compare.pixelMetrics(browserPng, animatedBrowserPng, null).ssim;
      const suffix = instant.tag === '0' ? '0' : 'worst';
      row[`staticSSIM${suffix}`] = staticSSIM;
      row[`freezeSSIM${suffix}`] = freezeSSIM;
      row[`animSSIM${suffix}`] = animSSIM;
      row[`stateSSIM${suffix}`] = stateSSIM;
      if (!(freezeSSIM >= STATIC_FREEZE_FLOOR)) {
        allExact = false;
        notes.push(`${instant.tag}:freeze-drift-${freezeSSIM.toFixed(4)}`);
      }
      // stateSSIM is recorded, not gated: a small value means the exporter's two routes disagree on
      // this state, which is exactly what animLoss is meant to quantify (composite/18 scores 0.82
      // here). Gating on it would discard the static evidence for every case whose animation path
      // is imperfect — i.e. the cases the metric exists for.
      if (!(stateSSIM >= STATIC_FREEZE_FLOOR)) {
        notes.push(`${instant.tag}:state-drift-${stateSSIM.toFixed(4)}`);
      }
      fs.writeFileSync(
        metaPath,
        JSON.stringify({
          timeSec: instant.timeSec,
          exact: frozen.exact,
          freezeSSIM,
          refPng: path.basename(refPng),
          animatedPng: path.basename(animatedPng),
        }),
        'utf8',
      );
    } catch (err) {
      notes.push(`${instant.tag}:${err && err.message ? err.message : String(err)}`);
      allExact = false;
    }
  }

  row.staticAvailable = allExact ? 'yes' : 'no';
  row.staticNote = notes.join(' ');
  return row;
}

function pagxScoredRows(rows) {
  return rows.filter((r) => !r.error && r.flag !== 'ref-static' && r.flag !== 'both-static');
}

// ---------------------------------------------------------------------------------------------
// Fidelity grade (doc: resources/pagx_to_html/animations/README.md, restore-fidelity section).
//   >= 0.999  complete    — pixel-identical to the reference
//   >= 0.99   basic       — anti-aliasing / quantization level
//   else      distorted   — needs a per-case attribution
//
// A case that draws text is graded on a looser curve - complete at 0.99, basic at 0.97 - because the
// two renderers shape glyphs differently: PAGX fills a hinted outline and, for fauxBold, unions a
// stroked one, while the browser rasterises its own face and (for fauxBold) strokes with
// `-webkit-text-stroke`. Those outlines differ by a pixel or two at the edges whatever the animation
// does, which caps a text case around 0.95-0.99; grading it on the shape-agnostic 0.999 curve would
// report a rendering-strategy difference as a distortion of the export.
// A case scores the stricter of animSSIM and staticSSIM. When the static ceiling sits below the
// animSSIM tier, the residual error is the *static* path (browser font shape, layout) rather than
// the animation writer, so the case is reported as static-limited and animLoss
// (staticSSIM - animSSIM at the same instant) says how much the animation writer itself costs.
// ---------------------------------------------------------------------------------------------

const GRADE_COMPLETE = '完全还原';
const GRADE_BASIC = '基本还原';
const GRADE_STATIC_LIMITED = '静态受限';
const GRADE_DISTORTED = '失真';

// A static route that scores this much worse than the animated one on the same state is not the
// font-shape ceiling the grade is about; it is a defect of the static path itself.
const STATIC_ROUTE_DEFECT_GAP = 0.05;

const TEXT_COMPLETE_SSIM = 0.99;
const TEXT_BASIC_SSIM = 0.97;

function gradeTier(ssim, hasText = false) {
  if (!isFinite(ssim)) return 0;
  if (ssim >= (hasText ? TEXT_COMPLETE_SSIM : 0.999)) return 3;
  if (ssim >= (hasText ? TEXT_BASIC_SSIM : 0.99)) return 2;
  return 1;
}

// A case draws text when its document declares a Text or TextBox node, wherever it sits in the tree.
function caseDrawsText(xml) {
  return /<Text(Box)?[\s/>]/.test(xml);
}

function gradeTierName(tier) {
  if (tier >= 3) return GRADE_COMPLETE;
  if (tier === 2) return GRADE_BASIC;
  return GRADE_DISTORTED;
}

// The static ceiling of a case: the stricter of its two instants.
function staticCeiling(row) {
  const values = [row.staticSSIM0, row.staticSSIMworst].filter((v) => isFinite(v));
  return values.length ? Math.min(...values) : NaN;
}

function gradePagxRow(row) {
  // The animated metric is graded on two readings: the per-frame mean (overall fidelity) and the
  // worst frame (a single badly reproduced frame is a defect a mean would hide). The case takes the
  // stricter of the two, so both are reported separately.
  const hasText = row.hasText === true;
  const animTierMean = gradeTier(row.ssimMean, hasText);
  const animTierMin = gradeTier(row.ssimMin, hasText);
  const animTier = Math.min(animTierMean, animTierMin);
  row.animGradeMean = gradeTierName(animTierMean);
  row.animGradeMin = gradeTierName(animTierMin);
  const staticValue = row.staticAvailable === 'yes' ? staticCeiling(row) : NaN;
  const staticTier = gradeTier(staticValue, hasText);
  if (staticTier > 0) {
    // Same instant against same instant: subtracting a mean animSSIM from the worst staticSSIM (or
    // the other way round) mixes two different states and flips conclusions on small-loss cases.
    row.animLoss0 = row.staticSSIM0 - row.animSSIM0;
    row.animLossWorst = row.staticSSIMworst - row.animSSIMworst;
    const losses = [row.animLoss0, row.animLossWorst].filter((v) => isFinite(v));
    // A negative loss means this state exports *better* statically than the animation does, so the
    // animation writer is not the limiter: clamp it away rather than crediting the case.
    row.animLoss = losses.length ? Math.max(0, Math.max(...losses)) : NaN;
  }
  if (staticTier === 0) {
    row.grade = gradeTierName(animTier);
    row.attribution = row.grade === GRADE_DISTORTED ? 'static-unavailable' : '';
    return;
  }
  const hasSkippedChannel = /skipped:/.test(row.staticNote || '');
  if (staticTier < animTier) {
    // The static state already scores below the animated one, so the animation writer is not what
    // is missing: grade the animation loss itself.
    row.grade = GRADE_STATIC_LIMITED;
    if (hasSkippedChannel) row.attribution = 'known-limit';
    else if (row.animLoss <= 0.001) row.attribution = 'animation-complete';
    else if (row.animLoss <= 0.01) row.attribution = 'animation-basic';
    else row.attribution = 'static';
    return;
  }
  row.grade = gradeTierName(Math.min(animTier, staticTier));
  if (row.grade !== GRADE_DISTORTED) {
    row.attribution = '';
    return;
  }
  if (hasText && row.staticAvailable === 'yes' && isFinite(row.animLoss) &&
      row.animLoss <= 0.01) {
    // The animated route is as faithful as the static route on the very states it plays, so what is
    // left is the glyph-outline difference the text curve already accounts for - a ceiling of the
    // renderers, not a defect of the export (composite/09, text/06).
    row.grade = GRADE_STATIC_LIMITED;
    row.attribution = 'static';
    return;
  }
  if (row.expect === 'gap') {
    // The corpus header already declares this channel unsupported by the exporter.
    row.attribution = 'known-limit';
    return;
  }
  if (hasSkippedChannel) {
    // A channel the runtime ignores but the exporter honors (or vice versa): the two routes can
    // never agree on that state, so it is a registered difference, not a fidelity defect.
    row.attribution = 'known-limit';
    return;
  }
  if (animTierMin < animTierMean) {
    // The mean is fine but at least one frame is not: the animation wiring for that frame is the
    // error, whatever the static ceiling is.
    row.attribution = 'exporter';
    return;
  }
  if (row.animLoss > 0.001) {
    // Same state, exported worse through the animated route: the keyframe writing is the error.
    row.attribution = 'exporter';
    return;
  }
  // The animation writer costs nothing here, so the ceiling is the limiter. A static route that
  // renders this state far *worse* than the animated one is a static-path defect, not the
  // font-shape difference the tier is meant to record.
  const worstNegative = Math.min(
    ...[row.animLoss0, row.animLossWorst].filter((v) => isFinite(v)),
  );
  row.attribution = worstNegative < -STATIC_ROUTE_DEFECT_GAP ? 'exporter' : 'static';
}

function summarizePagx(rows) {
  const scored = pagxScoredRows(rows);
  const flags = {};
  for (const r of rows) {
    if (r.flag) flags[r.flag] = (flags[r.flag] || 0) + 1;
  }
  const byExpect = {};
  for (const e of ['supported', 'gap', 'unknown']) {
    const sub = scored.filter((r) => r.expect === e);
    byExpect[e] = {
      count: sub.length,
      ssimMean: mean(sub.map((r) => r.ssimMean)),
      rgbDeltaMean: mean(sub.map((r) => r.rgbDeltaMean)),
    };
  }
  const groups = [];
  for (const g of Array.from(new Set(rows.map((r) => r.group)))) {
    const all = rows.filter((r) => r.group === g);
    const sup = pagxScoredRows(all).filter((r) => r.expect === 'supported');
    groups.push({
      group: g,
      cases: all.length,
      supportedScored: sup.length,
      ssimMean: mean(sup.map((r) => r.ssimMean)),
      ssimMin: sup.length ? Math.min(...sup.map((r) => r.ssimMin)) : NaN,
      rgbDeltaMean: mean(sup.map((r) => r.rgbDeltaMean)),
      flagged: all.filter((r) => r.flag).length,
      errored: all.filter((r) => r.error).length,
    });
  }
  // Grade buckets over the rows that produced a picture to grade. `static-unavailable` rows still
  // land in a grade (from animSSIM alone) but are counted separately so the summary never implies
  // they had static evidence.
  const grades = {};
  for (const name of [GRADE_COMPLETE, GRADE_BASIC, GRADE_STATIC_LIMITED, GRADE_DISTORTED]) {
    grades[name] = rows.filter((r) => !r.error && r.grade === name).length;
  }
  const staticStats = {
    available: rows.filter((r) => r.staticAvailable === 'yes').length,
    unavailable: rows.filter((r) => r.staticAvailable !== 'yes').length,
    ssimMean:
      mean(rows.filter((r) => r.staticAvailable === 'yes').map((r) => staticCeiling(r))),
    animLossMean: mean(rows.filter((r) => r.staticAvailable === 'yes').map((r) => r.animLoss)),
    freezeMean: mean(rows.map((r) => Math.min(r.freezeSSIM0, r.freezeSSIMworst))),
  };
  // The two families are graded on different curves (see gradeTier), so the summary keeps them
  // apart: a text case at 0.98 is complete for its family and would read as "basic" in a combined
  // bucket.
  const summarizeFamily = (list) => ({
    cases: list.length,
    ssimMean: mean(list.map((r) => r.ssimMean)),
    ssimMin: mean(list.map((r) => r.ssimMin)),
    grades: Object.fromEntries(
      [GRADE_COMPLETE, GRADE_BASIC, GRADE_STATIC_LIMITED, GRADE_DISTORTED].map((name) => [
        name,
        list.filter((r) => r.grade === name).length,
      ])
    ),
  });
  const gradedRows = rows.filter((r) => !r.error);
  const textFamily = summarizeFamily(gradedRows.filter((r) => r.hasText === true));
  const plainFamily = summarizeFamily(gradedRows.filter((r) => r.hasText !== true));

  const distorted = rows
    .filter((r) => !r.error && r.grade === GRADE_DISTORTED)
    .sort((a, b) => a.ssimMean - b.ssimMean);
  return {
    text: textFamily,
    nonText: plainFamily,
    cases: rows.length,
    errored: rows.filter((r) => r.error).length,
    scored: scored.length,
    flags,
    byExpect,
    groups,
    grades,
    staticStats,
    distorted,
  };
}

function writePagxCsv(rows, outPath) {
  const headers = [
    'name', 'group', 'target', 'channel', 'expect', 'ref', 'frames', 'ssimMean', 'ssimMin',
    'pixelDiffMean', 'rgbDeltaMean', 'rgbDeltaMax', 'motion', 'exportMotion', 'flag', 'width',
    'height', 'error',
    // Static baseline + grade. ssimMean / ssimMin above are the animated metric (animSSIM) and keep
    // their meaning; eval_gate.py reads them by name and ignores everything after.
    'staticSSIM0', 'staticSSIMworst', 'animSSIM0', 'animSSIMworst', 'freezeSSIM0',
    'freezeSSIMworst', 'stateSSIM0', 'stateSSIMworst', 'staticAvailable', 'staticNote', 'animLoss0',
    'animLossWorst', 'animLoss',
    'hasText', 'animGradeMean', 'animGradeMin', 'grade', 'attribution',
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

function writePagxMarkdown(rows, outPath, label) {
  const s = summarizePagx(rows);
  const lines = [];
  lines.push(`# PAGX -> HTML animation export eval — ${label}`);
  lines.push('');
  lines.push('Reference: `pagx render --time` of the source PAGX. Candidate: `pagx export` HTML seeked in the browser at the same times.');
  lines.push('');
  lines.push(`Cases: ${s.cases} (errored ${s.errored}, scored ${s.scored})`);
  lines.push('');
  lines.push(`- supported — SSIM ${fmt(s.byExpect.supported.ssimMean)}, rgbΔ ${fmt(s.byExpect.supported.rgbDeltaMean, 2)} over ${s.byExpect.supported.count} case(s)`);
  lines.push(`- gap — SSIM ${fmt(s.byExpect.gap.ssimMean)}, rgbΔ ${fmt(s.byExpect.gap.rgbDeltaMean, 2)} over ${s.byExpect.gap.count} case(s) (not supported by the exporter; informational)`);
  if (s.byExpect.unknown.count) {
    lines.push(`- missing Expect header — ${s.byExpect.unknown.count} case(s)`);
  }
  const flagText = Object.keys(s.flags).map((k) => `${k} ${s.flags[k]}`).join(', ');
  lines.push(`- motion flags — ${flagText || 'none'} (ref-static / both-static are excluded from SSIM)`);
  lines.push('');
  lines.push('## Restore fidelity');
  lines.push('');
  lines.push('Grades: `>= 0.999` complete / `>= 0.99` basic / below that distorted, applied to both the per-frame mean and the worst frame of animSSIM (`grade(mean)` / `grade(min)`) and taking the stricter one, so a case with one badly reproduced frame cannot pass on a good mean. `animSSIM` (the `ssim mean` column) diffs an animated PAGX render against the exported HTML seeked to the same time. `staticSSIM` diffs the PAGX *frozen* to one instant against the browser rendering the static HTML export of that same frozen document, so it excludes the animation writer and shows the static ceiling. A case takes the stricter of the two. `animLoss` is per instant `staticSSIM − animSSIM` (worst of the two instants, clamped at 0): it is how much the animation writer costs *at that same state*; attribution `static` means the loss is ~0 and the ceiling itself (font shape / static layout) is what keeps the case down, `exporter` means either the animation path adds the error or the static route renders the state more than 0.05 worse than the animated one, `known-limit` means the corpus header already declares the channel unsupported.');
  lines.push('');
  lines.push('| grade | cases |');
  lines.push('| --- | --- |');
  for (const name of [GRADE_COMPLETE, GRADE_BASIC, GRADE_STATIC_LIMITED, GRADE_DISTORTED]) {
    lines.push(`| ${name} | ${s.grades[name]} |`);
  }
  lines.push('');
  lines.push(`- static baseline available — ${s.staticStats.available} case(s), unavailable ${s.staticStats.unavailable} (freeze cannot represent the animated state: multi-phase mounts, matrix / matrix3D, contour-path point channels)`);
  lines.push(`- staticSSIM mean over available cases — ${fmt(s.staticStats.ssimMean)}; animLoss mean — ${fmt(s.staticStats.animLossMean)} (per instant staticSSIM − animSSIM, clamped at 0, worst of t=0 and the worst frame)`);
  lines.push(`- freeze self-check (min over t=0 / worst) mean — ${fmt(s.staticStats.freezeMean)} (frozen render vs the animated render at the same instant; 1.0 means the frozen document reproduces the runtime picture)`);
  lines.push('');
  if (s.distorted.length) {
    lines.push('### Distorted cases (attribution required)');
    lines.push('');
    lines.push('| case | animSSIM | staticSSIM | animLoss (t=0) | animLoss (worst) | animLoss | attribution | note |');
    lines.push('| --- | --- | --- | --- | --- | --- | --- | --- |');
    for (const r of s.distorted) {
      const ceiling = r.staticAvailable === 'yes' ? staticCeiling(r) : NaN;
      lines.push(`| ${r.name} | ${fmt(r.ssimMean)} | ${fmt(ceiling)} | ${fmt(r.animLoss0)} | ${fmt(r.animLossWorst)} | ${fmt(r.animLoss)} | ${r.attribution || '-'} | ${r.staticNote || ''} |`);
    }
    lines.push('');
  }
  lines.push('## By group (supported cases only)');
  lines.push('');
  lines.push('| group | cases | scored | ssim mean | ssim min | rgbΔ mean | flagged | errored |');
  lines.push('| --- | --- | --- | --- | --- | --- | --- | --- |');
  for (const g of s.groups) {
    lines.push(`| ${g.group} | ${g.cases} | ${g.supportedScored} | ${fmt(g.ssimMean)} | ${fmt(g.ssimMin)} | ${fmt(g.rgbDeltaMean, 2)} | ${g.flagged} | ${g.errored} |`);
  }
  lines.push('');
  lines.push('## Cases');
  lines.push('');
  lines.push('SSIM is luma-only, so a wrong hue can still score ~1; `rgbΔ` (mean per-channel RGB delta, 0-255) catches color errors. `ref span` / `export span` are the largest RGB delta from the first frame on each timeline (0 = never changes). `static0` / `staticWorst` are the two static instants; `freeze` is the PAGX-side self-check (frozen render vs the animated render at the same instant) and `state` the browser-side one (the frozen export vs the animated export at that same instant). A case is only attributed statically when both agree with the animated state (>= 0.99).');
  lines.push('');
  lines.push('| case | expect | target.channel | frames | ssim mean | ssim min | static0 | staticWorst | animLoss0 | animLossWorst | animLoss | freeze | state | grade(mean) | grade(min) | grade | attribution | rgbΔ mean | rgbΔ max | pixelDiff% | ref span | export span | flag | error |');
  lines.push('| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |');
  for (const r of rows) {
    const tc = r.target || r.channel ? `${r.target}.${r.channel}` : '-';
    lines.push('| ' + [
      r.name,
      r.expect,
      tc,
      String(r.frames),
      fmt(r.ssimMean),
      fmt(r.ssimMin),
      r.staticAvailable === 'yes' ? fmt(r.staticSSIM0) : '-',
      r.staticAvailable === 'yes' ? fmt(r.staticSSIMworst) : '-',
      fmt(r.animLoss0),
      fmt(r.animLossWorst),
      fmt(r.animLoss),
      fmt(Math.min(r.freezeSSIM0, r.freezeSSIMworst)),
      fmt(Math.min(r.stateSSIM0, r.stateSSIMworst)),
      r.animGradeMean || '',
      r.animGradeMin || '',
      r.grade || '',
      r.attribution || '',
      fmt(r.rgbDeltaMean, 2),
      fmt(r.rgbDeltaMax, 2),
      isFinite(r.pixelDiffMean) ? fmt(r.pixelDiffMean * 100, 2) : '-',
      fmt(r.motion, 2),
      fmt(r.exportMotion, 2),
      r.flag || '',
      r.error || '',
    ].join(' | ') + ' |');
  }
  lines.push('');
  if (s.staticStats.unavailable) {
    lines.push('Unavailable static baselines (animSSIM is still reported; no static attribution):');
    lines.push('');
    for (const r of rows.filter((x) => !x.error && x.staticAvailable !== 'yes')) {
      lines.push(`- \`${r.name}\` — ${r.staticNote || 'no note'}`);
    }
  }
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');
}

function writeCsv(rows, outPath) {
  const hasRt = rows.some((r) => r && r.roundtrip);
  const headers = [
    'name', 'frames', 'ssimMean', 'pixelDiffMean',
    ...(hasRt ? [
      'exportBrowserSsimMean',
      'exportBrowserVsPagxSsimMean',
      'roundtripPagxSsimMean',
      'roundtripVsPagxSsimMean',
    ] : []),
    'motion', 'animCaptured', 'animWarnings', 'animDropped', 'width', 'height', 'error',
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
    exportBrowserSsimMean: mean(ok.map((r) => r.exportBrowserSsimMean)),
    exportBrowserVsPagxSsimMean: mean(ok.map((r) => r.exportBrowserVsPagxSsimMean)),
    roundtripPagxSsimMean: mean(ok.map((r) => r.roundtripPagxSsimMean)),
    roundtripVsPagxSsimMean: mean(ok.map((r) => r.roundtripVsPagxSsimMean)),
    animCaptured: ok.reduce((a, r) => a + (r.animCaptured || 0), 0),
    animDropped: ok.reduce((a, r) => a + (r.animDropped || 0), 0),
  };
}

function writeMarkdown(rows, outPath, label) {
  const s = summarize(rows);
  const hasRt = rows.some((r) => r && r.roundtrip);
  const headers = hasRt
    ? ['name', 'frames', 'ssim(pagx/base)', 'expHtml/base', 'expHtml/pagx', 'rtPagx/base', 'rtPagx/pagx', 'motion', 'animCap', 'error']
    : ['name', 'frames', 'ssim', 'pixelDiff%', 'motion', 'animCap', 'animWarn', 'animDrop', 'error'];
  const lines = [];
  lines.push(`# Animation eval report — ${label}`);
  lines.push('');
  lines.push(`Cases: ${s.cases} (ok ${s.ok}, errored ${s.errored}, with motion ${s.moving})`);
  lines.push('');
  lines.push(`- SSIM (pagx vs baseline) — all cases ${fmt(s.ssimMean)}, moving cases only ${fmt(s.ssimMovingMean)}`);
  lines.push(`- pixel diff (per-frame mean) — ${fmt(s.pdMean)}`);
  if (hasRt) {
    lines.push(`- exported.html (browser) vs baseline SSIM — ${fmt(s.exportBrowserSsimMean)}`);
    lines.push(`- exported.html (browser) vs pagx SSIM — ${fmt(s.exportBrowserVsPagxSsimMean)}`);
    lines.push(`- roundtrip.pagx vs baseline SSIM — ${fmt(s.roundtripPagxSsimMean)}`);
    lines.push(`- roundtrip.pagx vs pagx SSIM — ${fmt(s.roundtripVsPagxSsimMean)}`);
  }
  lines.push(`- animations captured: ${s.animCaptured} (dropped at import: ${s.animDropped})`);
  lines.push('');
  lines.push('| ' + headers.join(' | ') + ' |');
  lines.push('| ' + headers.map(() => '---').join(' | ') + ' |');
  for (const r of rows) {
    const cells = hasRt
      ? [
          r.name,
          String(r.frames),
          fmt(r.ssimMean, 4),
          fmt(r.exportBrowserSsimMean, 4),
          fmt(r.exportBrowserVsPagxSsimMean, 4),
          fmt(r.roundtripPagxSsimMean, 4),
          fmt(r.roundtripVsPagxSsimMean, 4),
          fmt(r.motion, 3),
          String(r.animCaptured),
          r.error || '',
        ]
      : [
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

// Grade -> tag colour. "Static limited" is informational (the exporter is not at fault), the two
// hard grades earn green / red.
function gradeClass(grade) {
  if (grade === GRADE_COMPLETE) return 'good';
  if (grade === GRADE_BASIC) return 'mid';
  if (grade === GRADE_STATIC_LIMITED) return 'na';
  return 'bad';
}

function writeIndexHtml(rows, outDir, label) {
  const s = summarize(rows);
  const pagxSource = rows.some((r) => r && r.source === 'pagx');
  // PAGX reports are shared as a single file. Keep every sampled PNG once in the page and
  // resolve both the player and its frame strips against this map rather than sibling folders.
  const frameSources = {};
  if (pagxSource) {
    for (const row of rows) {
      if (row.error || row.frames <= 0) continue;
      for (const sub of ['baseline', 'render', 'diff']) {
        for (let i = 0; i < row.frames; i++) {
          const frame = `frame-${String(i).padStart(3, '0')}.png`;
          const key = `${encodeURIComponent(row.name)}/${sub}/${frame}`;
          frameSources[key] = `data:image/png;base64,${fs.readFileSync(path.join(outDir, row.name, sub, frame)).toString('base64')}`;
        }
      }
      // The static baseline pair (frozen reference / frozen export / their diff), when present.
      for (const sub of ['static/0.ref.png', 'static/0.diff.png', 'static/0.browser/frame-000.png']) {
        const full = path.join(outDir, row.name, sub);
        if (!fs.existsSync(full)) continue;
        frameSources[`${encodeURIComponent(row.name)}/${sub}`] =
          `data:image/png;base64,${fs.readFileSync(full).toString('base64')}`;
      }
    }
  }
  const frameUrl = (key) => pagxSource ? frameSources[key] : key;
  const baselineCaption = pagxSource ? 'pagx render (reference)' : 'baseline';
  const renderCaption = pagxSource ? 'exported.html (browser)' : 'pagx';

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
      const rtFigures = r.roundtrip ? `
      <figure><figcaption>exported.html (browser)</figcaption><img class="play-img" data-sub="exported-browser" src="${frameUrl(`${dir}/exported-browser/frame-000.png`)}"/></figure>
      <figure><figcaption>roundtrip.pagx</figcaption><img class="play-img" data-sub="roundtrip-render" src="${frameUrl(`${dir}/roundtrip-render/frame-000.png`)}"/></figure>` : '';
      const player = `
  <div class="player" data-case="${ci}">
    <div class="stage">
      <figure><figcaption>${baselineCaption}</figcaption><img class="play-img" data-sub="baseline" src="${frameUrl(`${dir}/baseline/frame-000.png`)}"/></figure>
      <figure><figcaption>${renderCaption}</figcaption><img class="play-img" data-sub="render" src="${frameUrl(`${dir}/render/frame-000.png`)}"/></figure>${rtFigures}
      <figure><figcaption>diff</figcaption><img class="play-img" data-sub="diff" src="${frameUrl(`${dir}/diff/frame-000.png`)}"/></figure>
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
      const subs = r.roundtrip
        ? [
            ['baseline', 'baseline'],
            ['render', 'pagx'],
            ['exported-browser', 'exp.html'],
            ['roundtrip-render', 'rt.pagx'],
            ['diff', 'diff'],
          ]
        : [['baseline', pagxSource ? 'reference' : 'baseline'], ['render', pagxSource ? 'exp.html' : 'pagx'], ['diff', 'diff']];
      let cells = '';
      for (let i = 0; i < r.frames; i++) {
        const idx = String(i).padStart(3, '0');
        let imgs = '';
        for (const [sub] of subs) {
          const key = `${dir}/${sub}/frame-${idx}.png`;
          imgs += pagxSource
            ? `<img data-frame-key="${key}" loading="lazy"/>`
            : `<img src="${key}" loading="lazy"/>`;
        }
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
    const rtTags = r.roundtrip
      ? ` <span class="ssim-tag ${ssimClass(r.exportBrowserSsimMean)}">expHtml/base ${fmt(r.exportBrowserSsimMean, 4)}</span>` +
        ` <span class="ssim-tag ${ssimClass(r.roundtripVsPagxSsimMean)}">rtPagx/pagx ${fmt(r.roundtripVsPagxSsimMean, 4)}</span>`
      : '';
    const pagxTags = r.source === 'pagx'
      ? ` <span class="ssim-tag na">${r.expect} · ${r.target}.${r.channel}</span>` +
        (r.flag ? ` <span class="ssim-tag bad">${r.flag}</span>` : '')
      : '';
    // Static ceiling and the grade it feeds: the animated SSIM tag alone cannot tell an exporter
    // bug apart from a static-path (browser font) difference.
    const staticTags = r.source === 'pagx'
      ? (r.staticAvailable === 'yes'
        ? ` <span class="ssim-tag ${ssimClass(staticCeiling(r))}">static ${fmt(staticCeiling(r), 4)}</span>`
        : ' <span class="ssim-tag na">static n/a</span>') +
        (isFinite(r.animLoss) ? ` <span class="ssim-tag ${ssimClass(1 - Math.abs(r.animLoss))}">animLoss ${fmt(r.animLoss, 4)}</span>` : '') +
        (r.grade ? ` <span class="ssim-tag ${gradeClass(r.grade)}">${r.grade}${r.attribution ? ` · ${r.attribution}` : ''}</span>` : '')
      : '';
    const staticFigures = r.source === 'pagx' && r.staticAvailable === 'yes'
      ? `
      <figure><figcaption>static ref (frozen pagx, t=0)</figcaption><img src="${frameUrl(`${dir}/static/0.ref.png`)}"/></figure>
      <figure><figcaption>static candidate (frozen export)</figcaption><img src="${frameUrl(`${dir}/static/0.browser/frame-000.png`)}"/></figure>
      <figure><figcaption>static diff</figcaption><img src="${frameUrl(`${dir}/static/0.diff.png`)}"/></figure>`
      : '';
    return `
<section class="case">
  <h2>${r.name} <span class="ssim-tag ${cls}">SSIM ${fmt(r.ssimMean, 4)}</span>${staticTags}${rtTags}${pagxTags}</h2>
  <p>
    frames <b>${r.frames}</b> &middot; pixel diff <b>${isFinite(r.pixelDiffMean) ? fmt(r.pixelDiffMean * 100, 2) + '%' : '-'}</b>
    &middot; motion <b>${fmt(r.motion, 3)}</b>
    &middot; animations captured <b>${r.animCaptured}</b>
    <span class="muted">(import warnings ${r.animWarnings}, dropped ${r.animDropped})</span>
    ${status}
    ${r.staticAvailable === 'yes' ? '' : `<br/><span class="muted">static baseline unavailable — ${r.staticNote || 'no note'}</span>`}
  </p>
  ${staticFigures ? `<div class="stage">${staticFigures}</div>` : ''}
  ${body}
</section>`;
  }).join('\n');

  const hasRt = rows.some((r) => r && r.roundtrip);
  const rtSummaryItems = hasRt ? `
    <li>exported.html (browser) vs baseline SSIM — <b>${fmt(s.exportBrowserSsimMean, 4)}</b>, vs pagx — <b>${fmt(s.exportBrowserVsPagxSsimMean, 4)}</b></li>
    <li>roundtrip.pagx vs baseline SSIM — <b>${fmt(s.roundtripPagxSsimMean, 4)}</b>, vs pagx — <b>${fmt(s.roundtripVsPagxSsimMean, 4)}</b></li>` : '';
  let gradeSummaryItems = '';
  if (pagxSource) {
    const p = summarizePagx(rows);
    gradeSummaryItems = `
    <li>grade — complete <b>${p.grades[GRADE_COMPLETE]}</b>, basic <b>${p.grades[GRADE_BASIC]}</b>, static-limited <b>${p.grades[GRADE_STATIC_LIMITED]}</b>, distorted <b>${p.grades[GRADE_DISTORTED]}</b></li>
    <li><b>without text</b> (complete 0.999 / basic 0.99) — ${p.nonText.cases} cases &middot; ssim mean <b>${fmt(p.nonText.ssimMean, 4)}</b> &middot; complete <b>${p.nonText.grades[GRADE_COMPLETE]}</b>, basic <b>${p.nonText.grades[GRADE_BASIC]}</b>, static-limited <b>${p.nonText.grades[GRADE_STATIC_LIMITED]}</b>, distorted <b>${p.nonText.grades[GRADE_DISTORTED]}</b></li>
    <li><b>with text</b> (complete 0.99 / basic 0.97; glyph outlines differ between the renderers) — ${p.text.cases} cases &middot; ssim mean <b>${fmt(p.text.ssimMean, 4)}</b> &middot; complete <b>${p.text.grades[GRADE_COMPLETE]}</b>, basic <b>${p.text.grades[GRADE_BASIC]}</b>, static-limited <b>${p.text.grades[GRADE_STATIC_LIMITED]}</b>, distorted <b>${p.text.grades[GRADE_DISTORTED]}</b></li>
    <li>static baseline — <b>${p.staticStats.available}</b> available / <b>${p.staticStats.unavailable}</b> unavailable &middot; staticSSIM mean <b>${fmt(p.staticStats.ssimMean, 4)}</b> &middot; animLoss mean <b>${fmt(p.staticStats.animLossMean, 4)}</b> &middot; freeze self-check mean <b>${fmt(p.staticStats.freezeMean, 4)}</b></li>`;
  }

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
    <li>pixel diff (per-frame mean) — <b>${isFinite(s.pdMean) ? fmt(s.pdMean * 100, 2) + '%' : '-'}</b></li>${gradeSummaryItems}${rtSummaryItems}
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
const FRAME_SOURCES = ${JSON.stringify(frameSources)};
function frameUrl(key) { return FRAME_SOURCES[key] || key; }
// Defer decoding the static frame strips until they are brought into view.
const frameObserver = 'IntersectionObserver' in window
  ? new IntersectionObserver((entries, observer) => {
      for (const entry of entries) {
        if (!entry.isIntersecting) continue;
        const image = entry.target;
        image.src = frameUrl(image.dataset.frameKey);
        observer.unobserve(image);
      }
    }, { rootMargin: '300px' })
  : null;
for (const image of document.querySelectorAll('img[data-frame-key]')) {
  if (frameObserver) frameObserver.observe(image);
  else image.src = frameUrl(image.dataset.frameKey);
}

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
      img.src = frameUrl(this.data.dir + '/' + sub + '/frame-' + idx + '.png');
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
  const pagxSource = opts.source === 'pagx';
  if (pagxSource) {
    if (opts.roundtrip) fail('--roundtrip does not apply to --source pagx');
    if (opts.downloadImages) fail('--download-images does not apply to --source pagx');
    // The browser captures the exported HTML at 1x, so the reference must be rendered at 1x too.
    if (opts.scale !== 1) fail('--scale must be 1 with --source pagx');
    if (!opts.samplesSet) opts.samples = PAGX_DEFAULT_SAMPLES;
  }
  const ext = pagxSource ? '.pagx' : '.html';
  const cases = findCorpusFiles(opts.corpus, opts.only, opts.recursive, ext);
  if (cases.length === 0) {
    const hint = opts.recursive ? '' : ' (try --recursive to descend into sub-directories)';
    fail(`no ${ext.slice(1)} cases found in ${opts.corpus}${hint}`, 1);
  }
  const concurrency = Math.max(1, Math.min(opts.concurrency, cases.length));
  console.log(`anim-run: ${cases.length} cases → ${opts.outDir}  (source=${opts.source}, samples=${opts.samples}, concurrency=${concurrency}${opts.downloadImages ? ', download-images=on' : ''}, pagx-images=${opts.pagxImages})`);

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
        row = pagxSource ? await processPagxCase(c, opts.outDir, opts) : await processCase(c, opts.outDir, opts);
      } catch (err) {
        row = { name: c.name, frames: 0, error: `unhandled: ${err && err.message ? err.message : err}` };
        if (pagxSource) Object.assign(row, { source: 'pagx', group: '(error)', expect: 'unknown', flag: '' });
      }
      const dur = Date.now() - t0;
      doneCount += 1;
      if (row.error) {
        console.log(`[${doneCount}/${total}] ${c.name}  ERROR ${row.error} (${dur} ms)`);
      } else if (pagxSource) {
        const staticText = row.staticAvailable === 'yes' ? fmt(staticCeiling(row), 4) : 'n/a';
        console.log(`[${doneCount}/${total}] ${c.name}  SSIM ${fmt(row.ssimMean, 4)}  min ${fmt(row.ssimMin, 4)}  static ${staticText}  ${row.grade}${row.attribution ? '/' + row.attribution : ''}  ${row.expect}${row.flag ? '  ' + row.flag : ''}  (${dur} ms)`);
      } else if (row.roundtrip) {
        console.log(`[${doneCount}/${total}] ${c.name}  SSIM ${fmt(row.ssimMean, 4)}  expHtml/pagx ${fmt(row.exportBrowserVsPagxSsimMean, 4)}  rtPagx/pagx ${fmt(row.roundtripVsPagxSsimMean, 4)}  anim ${row.animCaptured}  (${dur} ms)`);
      } else {
        console.log(`[${doneCount}/${total}] ${c.name}  SSIM ${fmt(row.ssimMean, 4)}  diff ${fmt(row.pixelDiffMean * 100, 2)}%  motion ${fmt(row.motion, 3)}  anim ${row.animCaptured}  (${dur} ms)`);
      }
      rows[i] = row;
      if (!row.error) {
        writeCaseCacheKey(path.join(opts.outDir, c.name, '.cache-key.json'),
          buildCaseCacheKey(c.path, opts));
      }
    }
  };

  await Promise.all(Array.from({ length: concurrency }, () => worker()));

  if (pagxSource) {
    writePagxCsv(rows, path.join(opts.outDir, 'report.csv'));
    writePagxMarkdown(rows, path.join(opts.outDir, 'report.md'), opts.label);
  } else {
    writeCsv(rows, path.join(opts.outDir, 'report.csv'));
    writeMarkdown(rows, path.join(opts.outDir, 'report.md'), opts.label);
  }
  writeIndexHtml(rows, opts.outDir, opts.label);
  console.log(`anim-run: wrote ${path.join(opts.outDir, 'report.csv')}`);
  console.log(`anim-run: wrote ${path.join(opts.outDir, 'report.md')}`);
  console.log(`anim-run: wrote ${path.join(opts.outDir, 'index.html')}`);

  // A run that produced reports is not a successful run: case failures used to leave the exit code
  // at 0, so CI could not tell a broken export from a clean one.
  const failed = rows.filter((row) => row && row.error).length;
  if (failed > 0) {
    console.error(`anim-run: ${failed} of ${rows.length} case(s) failed`);
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main().catch((err) => {
    console.error(err && err.stack ? err.stack : err);
    process.exit(1);
  });
}

module.exports = {
  buildCaseCacheKey,
  cacheKeyMatches,
  writeCaseCacheKey,
  writeIndexHtml,
  writeMarkdown,
  writeCsv,
  summarize,
  parsePagxTiming,
  pagxSampleTimes,
  parsePagxCaseHeader,
  classifyPagxMotion,
  summarizePagx,
};
