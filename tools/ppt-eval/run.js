#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { PNG } = require('pngjs');
const { comparePng } = require('./compare');
const {
  exportPptx,
  findPdfRasterizer,
  findSoffice,
  rasterizePptx,
  renderPagx,
  sha256File,
  toolVersion,
} = require('./pipeline');
const { summarize, writeCsv, writeHtml, writeMarkdown } = require('./report');

const SCRIPT_DIR = __dirname;
const REPO_ROOT = path.resolve(SCRIPT_DIR, '..', '..');

function usage() {
  console.log(`Usage: node run.js --corpus <name> [options]

Options:
  --manifest <file>          Corpus manifest (default: resources/ppt/corpora.json)
  --corpus <name>            Corpus name from the manifest
  --out <dir>                Output directory for this corpus
  --pagx-bin <file>          Built pagx CLI (default: $PAGX_BIN)
  --soffice <file>           LibreOffice executable (default: $SOFFICE_BIN/PATH)
  --pdf-rasterizer <file>    pdftocairo or pdftoppm (default: PATH)
  --allow-png-fallback       Permit LibreOffice's single-slide PNG fallback
  --only <substring>         Run matching case names only
  --concurrency, -j <n>      Concurrent cases (default: 2)
  --scale <n>                Native PAGX render scale (default: 1)
  --timeout-ms <n>           Per-process timeout (default: 120000)
  --skip-existing            Reuse artifacts only when their cache key matches
  -h, --help                 Show this help`);
}

function parseArgs(argv) {
  const options = {
    manifest: path.join(REPO_ROOT, 'resources/ppt/corpora.json'),
    corpus: '',
    outDir: '',
    pagxBin: process.env.PAGX_BIN || path.join(REPO_ROOT, 'cmake-build-debug/pagx'),
    soffice: process.env.SOFFICE_BIN || '',
    pdfRasterizer: process.env.PDF_RASTERIZER_BIN || '',
    allowPngFallback: process.env.PPT_EVAL_ALLOW_PNG_FALLBACK === '1',
    only: '',
    concurrency: 2,
    scale: 1,
    timeoutMs: 120000,
    skipExisting: false,
  };
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--manifest') options.manifest = path.resolve(argv[++i]);
    else if (arg === '--corpus') options.corpus = argv[++i];
    else if (arg === '--out') options.outDir = path.resolve(argv[++i]);
    else if (arg === '--pagx-bin') options.pagxBin = path.resolve(argv[++i]);
    else if (arg === '--soffice') options.soffice = path.resolve(argv[++i]);
    else if (arg === '--pdf-rasterizer') options.pdfRasterizer = path.resolve(argv[++i]);
    else if (arg === '--allow-png-fallback') options.allowPngFallback = true;
    else if (arg === '--only') options.only = argv[++i];
    else if (arg === '--concurrency' || arg === '-j') options.concurrency = Number(argv[++i]);
    else if (arg === '--scale') options.scale = Number(argv[++i]);
    else if (arg === '--timeout-ms') options.timeoutMs = Number(argv[++i]);
    else if (arg === '--skip-existing') options.skipExisting = true;
    else if (arg === '-h' || arg === '--help') { usage(); process.exit(0); }
    else throw new Error(`unknown option '${arg}'`);
  }
  if (!options.corpus) throw new Error('--corpus is required');
  if (!options.outDir) options.outDir = path.join(SCRIPT_DIR, 'out', `ppt-${options.corpus}`);
  if (!Number.isFinite(options.concurrency) || options.concurrency < 1) {
    throw new Error('--concurrency must be a positive integer');
  }
  if (!Number.isFinite(options.scale) || options.scale <= 0) {
    throw new Error('--scale must be positive');
  }
  if (!Number.isFinite(options.timeoutMs) || options.timeoutMs < 1000) {
    throw new Error('--timeout-ms must be at least 1000');
  }
  options.concurrency = Math.floor(options.concurrency);
  return options;
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function normalizeRelative(filePath) {
  return filePath.split(path.sep).join('/');
}

function safeName(name) {
  return name.replace(/[^a-zA-Z0-9._-]+/g, '__');
}

function caseDirectoryName(name) {
  return `${safeName(name)}--${crypto.createHash('sha256').update(name).digest('hex').slice(0, 10)}`;
}

function isExcluded(relativePath, exclusions) {
  return exclusions.some((entry) => relativePath === entry || relativePath.startsWith(`${entry}/`));
}

function walkPagx(root, recursive) {
  const files = [];
  const visit = (dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      if (entry.name.startsWith('.')) continue;
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        if (recursive) visit(fullPath);
      } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.pagx')) {
        files.push(fullPath);
      }
    }
  };
  visit(root);
  return files;
}

function discoverCases(manifest, corpusName, only) {
  const corpus = manifest.corpora && manifest.corpora[corpusName];
  if (!corpus) throw new Error(`unknown corpus '${corpusName}'`);
  const cases = [];
  const seen = new Set();
  for (const rootConfig of corpus.roots || []) {
    const config = typeof rootConfig === 'string' ? { path: rootConfig } : rootConfig;
    const absoluteRoot = path.resolve(REPO_ROOT, config.path);
    if (!fs.existsSync(absoluteRoot)) throw new Error(`corpus root missing: ${absoluteRoot}`);
    const exclusions = (config.exclude || []).map(normalizeRelative);
    for (const file of walkPagx(absoluteRoot, config.recursive !== false)) {
      const relativeToRoot = normalizeRelative(path.relative(absoluteRoot, file));
      if (isExcluded(relativeToRoot, exclusions)) continue;
      const repoRelative = normalizeRelative(path.relative(REPO_ROOT, file));
      if (seen.has(repoRelative)) continue;
      seen.add(repoRelative);
      cases.push({
        name: repoRelative.replace(/\.pagx$/i, ''),
        inputs: [file],
        sourcePaths: [repoRelative],
        exportArgs: corpus.exportArgs || [],
      });
    }
  }
  for (const deck of corpus.decks || []) {
    const inputs = deck.inputs.map((input) => path.resolve(REPO_ROOT, input));
    for (const input of inputs) {
      if (!fs.existsSync(input)) throw new Error(`deck input missing: ${input}`);
    }
    cases.push({
      name: deck.name,
      inputs,
      sourcePaths: deck.inputs.map(normalizeRelative),
      exportArgs: deck.exportArgs || corpus.exportArgs || [],
    });
  }
  const filtered = only ? cases.filter((entry) => entry.name.includes(only)) : cases;
  filtered.sort((a, b) => a.name.localeCompare(b.name));
  return { corpus, cases: filtered };
}

function pngSize(filePath) {
  const data = fs.readFileSync(filePath);
  const image = PNG.sync.read(data, { skipRescale: true });
  return { width: image.width, height: image.height };
}

function expectedSlideSize(width, height, renderScale) {
  width /= renderScale;
  height /= renderScale;
  const emuPerPixel = 9525;
  const min = 914400;
  const max = 51206400;
  let cx = Math.round(width * emuPerPixel);
  let cy = Math.round(height * emuPerPixel);
  if (cx <= 0) cx = min;
  if (cy <= 0) cy = min;
  if (cx > max || cy > max) {
    const scale = Math.min(max / cx, max / cy);
    cx = Math.round(cx * scale);
    cy = Math.round(cy * scale);
  }
  if (cx < min || cy < min) {
    const scale = Math.max(min / cx, min / cy);
    cx = Math.round(cx * scale);
    cy = Math.round(cy * scale);
  }
  return {
    width: Math.round(cx / emuPerPixel * renderScale),
    height: Math.round(cy / emuPerPixel * renderScale),
  };
}

function sizeNear(actual, expected) {
  return Math.abs(actual.width - expected.width) <= 1 && Math.abs(actual.height - expected.height) <= 1;
}

function relativeArtifact(outDir, filePath) {
  return normalizeRelative(path.relative(outDir, filePath));
}

function hashStrings(values) {
  const hash = crypto.createHash('sha256');
  for (const value of values) hash.update(String(value)).update('\0');
  return hash.digest('hex');
}

function caseCacheKey(entry, context) {
  const values = [
    context.pagxHash,
    context.evalHash,
    context.environment.key,
    context.scale,
    ...entry.exportArgs,
  ];
  for (const input of entry.inputs) values.push(normalizeRelative(path.relative(REPO_ROOT, input)), sha256File(input));
  for (const extra of context.corpusResources) values.push(extra.relative, extra.hash);
  return hashStrings(values);
}

function collectResourceHashes(corpus) {
  const resources = [];
  for (const resource of corpus.resources || []) {
    const absolute = path.resolve(REPO_ROOT, resource);
    if (!fs.existsSync(absolute)) throw new Error(`corpus resource missing: ${absolute}`);
    resources.push({ relative: normalizeRelative(resource), hash: sha256File(absolute) });
  }
  return resources;
}

function cachedRows(caseDir, cacheKey) {
  const metadata = path.join(caseDir, 'case.json');
  if (!fs.existsSync(metadata)) return null;
  try {
    const saved = readJson(metadata);
    if (saved.cacheKey !== cacheKey || !Array.isArray(saved.rows)) return null;
    for (const row of saved.rows) {
      if (row.error || row.skipped) return null;
      for (const field of ['reference', 'actual', 'diff']) {
        if (!row[field] || !fs.existsSync(path.resolve(caseDir, '..', row[field]))) return null;
      }
    }
    return saved.rows;
  } catch (_) {
    return null;
  }
}

function errorRows(entry, corpusName, message, environment, timings = {}) {
  return [{
    corpus: corpusName,
    case: entry.name,
    page: '',
    source: entry.sourcePaths.join(';'),
    reference: '', actual: '', diff: '',
    referenceSize: '', actualSize: '', expectedActualSize: '',
    sizeMismatch: false, unexpectedSizeMismatch: false,
    ssim: NaN, pixelDiffRatio: NaN, meanRgbDelta: NaN,
    roiSsim: NaN, roiMeanRgbDelta: NaN,
    exportMs: timings.exportMs || 0,
    rasterMs: timings.rasterMs || 0,
    renderer: environment.renderer,
    skipped: '',
    error: message,
  }];
}

async function processCase(entry, options, context) {
  const caseDir = path.join(options.outDir, caseDirectoryName(entry.name));
  fs.mkdirSync(caseDir, { recursive: true });
  const cacheKey = caseCacheKey(entry, context);
  if (options.skipExisting) {
    const rows = cachedRows(caseDir, cacheKey);
    if (rows) return rows;
  }

  const referencePaths = [];
  for (let index = 0; index < entry.inputs.length; index++) {
    const reference = path.join(caseDir, `reference-${index + 1}.png`);
    const result = await renderPagx({
      pagxBin: options.pagxBin,
      input: entry.inputs[index],
      output: reference,
      scale: options.scale,
      timeoutMs: options.timeoutMs,
    });
    if (result.code !== 0 || !fs.existsSync(reference)) {
      return errorRows(entry, options.corpus, `render failed: ${(result.stderr || result.stdout).trim()}`, context.environment);
    }
    referencePaths.push(reference);
  }

  const pptx = path.join(caseDir, 'export.pptx');
  const exportResult = await exportPptx({
    pagxBin: options.pagxBin,
    inputs: entry.inputs,
    output: pptx,
    exportArgs: entry.exportArgs,
    timeoutMs: options.timeoutMs,
  });
  if (exportResult.code !== 0 || !fs.existsSync(pptx)) {
    return errorRows(
      entry,
      options.corpus,
      `PPTX export failed: ${(exportResult.stderr || exportResult.stdout).trim()}`,
      context.environment,
      { exportMs: exportResult.durationMs },
    );
  }

  const rasterStarted = Date.now();
  let raster;
  try {
    raster = await rasterizePptx({
      soffice: context.soffice,
      pdfRasterizer: context.pdfRasterizer,
      pptx,
      outputDir: caseDir,
      timeoutMs: options.timeoutMs,
      scale: options.scale,
    });
  } catch (error) {
    return errorRows(entry, options.corpus, error.message, context.environment, {
      exportMs: exportResult.durationMs,
      rasterMs: Date.now() - rasterStarted,
    });
  }
  const rasterMs = Date.now() - rasterStarted;
  if (raster.pages.length !== referencePaths.length) {
    return errorRows(
      entry,
      options.corpus,
      `slide count mismatch: expected ${referencePaths.length}, got ${raster.pages.length}`,
      context.environment,
      { exportMs: exportResult.durationMs, rasterMs },
    );
  }

  const deckReferenceSize = pngSize(referencePaths[0]);
  const expectedActual = expectedSlideSize(
    deckReferenceSize.width,
    deckReferenceSize.height,
    options.scale,
  );
  const rows = [];
  for (let index = 0; index < referencePaths.length; index++) {
    const page = index + 1;
    const reference = referencePaths[index];
    const actual = raster.pages[index];
    const diff = path.join(caseDir, `diff-${page}.png`);
    try {
      const metrics = comparePng(reference, actual, diff);
      const actualSize = { width: metrics.actualWidth, height: metrics.actualHeight };
      rows.push({
        corpus: options.corpus,
        case: entry.name,
        page,
        source: entry.sourcePaths[index] || entry.sourcePaths[0],
        reference: relativeArtifact(options.outDir, reference),
        actual: relativeArtifact(options.outDir, actual),
        diff: relativeArtifact(options.outDir, diff),
        referenceSize: `${metrics.referenceWidth}x${metrics.referenceHeight}`,
        actualSize: `${metrics.actualWidth}x${metrics.actualHeight}`,
        expectedActualSize: `${expectedActual.width}x${expectedActual.height}`,
        sizeMismatch: metrics.sizeMismatch,
        unexpectedSizeMismatch: !sizeNear(actualSize, expectedActual),
        ssim: metrics.ssim,
        pixelDiffRatio: metrics.pixelDiffRatio,
        meanRgbDelta: metrics.meanRgbDelta,
        roiSsim: metrics.roiSsim,
        roiMeanRgbDelta: metrics.roiMeanRgbDelta,
        exportMs: exportResult.durationMs || 0,
        rasterMs,
        renderer: `${context.environment.renderer}; ${raster.mode}`,
        skipped: '',
        error: '',
      });
    } catch (error) {
      rows.push(...errorRows(entry, options.corpus, `compare failed: ${error.message}`, context.environment, {
        exportMs: exportResult.durationMs,
        rasterMs,
      }));
      break;
    }
  }
  fs.writeFileSync(path.join(caseDir, 'case.json'), `${JSON.stringify({ cacheKey, rows }, null, 2)}\n`);
  return rows;
}

async function main() {
  const options = parseArgs(process.argv);
  if (!fs.existsSync(options.manifest)) throw new Error(`manifest missing: ${options.manifest}`);
  if (!fs.existsSync(options.pagxBin)) throw new Error(`pagx binary missing: ${options.pagxBin}`);
  const soffice = findSoffice(options.soffice);
  if (!soffice) throw new Error('LibreOffice soffice is required but was not found');
  const pdfRasterizer = findPdfRasterizer(options.pdfRasterizer);
  if (!pdfRasterizer && !options.allowPngFallback) {
    throw new Error(
      'pdftocairo or pdftoppm is required for stable multi-page rasterization; ' +
      'install Poppler or set PPT_EVAL_ALLOW_PNG_FALLBACK=1 for a local single-slide fallback',
    );
  }
  const manifest = readJson(options.manifest);
  const { corpus, cases } = discoverCases(manifest, options.corpus, options.only);
  if (!cases.length) throw new Error(`corpus '${options.corpus}' has no matching cases`);
  if (!pdfRasterizer && cases.some((entry) => entry.inputs.length !== 1)) {
    throw new Error(
      'the LibreOffice PNG fallback cannot validate multi-slide decks; install pdftocairo/pdftoppm',
    );
  }
  fs.mkdirSync(options.outDir, { recursive: true });

  const [sofficeVersion, rasterizerVersion] = await Promise.all([
    toolVersion(soffice),
    pdfRasterizer ? toolVersion(pdfRasterizer, ['-v']) : Promise.resolve('LibreOffice PNG fallback'),
  ]);
  const renderer = sofficeVersion || 'LibreOffice (version unknown)';
  const rasterizer = rasterizerVersion || path.basename(pdfRasterizer || 'soffice-png');
  const environment = {
    renderer,
    rasterizer,
    platform: process.platform,
    arch: process.arch,
    key: safeName(`${process.platform}-${process.arch}__${renderer}__${rasterizer}`),
  };
  fs.writeFileSync(path.join(options.outDir, 'environment.json'), `${JSON.stringify(environment, null, 2)}\n`);
  const context = {
    soffice,
    pdfRasterizer,
    environment,
    pagxHash: sha256File(options.pagxBin),
    evalHash: hashStrings([
      sha256File(path.join(SCRIPT_DIR, 'run.js')),
      sha256File(path.join(SCRIPT_DIR, 'pipeline.js')),
      sha256File(path.join(SCRIPT_DIR, 'compare.js')),
      sha256File(path.join(SCRIPT_DIR, 'report.js')),
    ]),
    scale: options.scale,
    corpusResources: collectResourceHashes(corpus),
  };

  const rowsByCase = new Array(cases.length);
  let next = 0;
  let done = 0;
  const concurrency = Math.min(options.concurrency, cases.length);
  console.log(
    `ppt-eval: ${options.corpus}: ${cases.length} cases, concurrency=${concurrency}, ` +
    `renderer=${renderer}, rasterizer=${rasterizer}`,
  );
  const worker = async () => {
    while (true) {
      const index = next++;
      if (index >= cases.length) return;
      let rows;
      try {
        rows = await processCase(cases[index], options, context);
      } catch (error) {
        rows = errorRows(cases[index], options.corpus, `unhandled: ${error.message}`, environment);
      }
      rowsByCase[index] = rows;
      done++;
      const failed = rows.find((row) => row.error);
      const worst = Math.min(...rows.map((row) => row.ssim).filter(Number.isFinite));
      console.log(
        `[${done}/${cases.length}] ${cases[index].name}  ` +
        (failed ? `ERROR ${failed.error}` : `SSIM ${Number.isFinite(worst) ? worst.toFixed(4) : '-'}`),
      );
    }
  };
  await Promise.all(Array.from({ length: concurrency }, worker));
  const rows = rowsByCase.flat();
  const expectedCases = options.only ? null : cases.length;
  writeCsv(rows, path.join(options.outDir, 'report.csv'));
  writeMarkdown(rows, path.join(options.outDir, 'report.md'), options.corpus, environment);
  writeHtml(rows, path.join(options.outDir, 'index.html'), options.corpus, environment);
  fs.writeFileSync(path.join(options.outDir, 'report.json'), `${JSON.stringify({
    corpus: options.corpus,
    environment,
    expectedCases,
    summary: summarize(rows),
    rows,
  }, null, 2)}\n`);
  const summary = summarize(rows);
  console.log(`ppt-eval: reports written to ${options.outDir}`);
  if (summary.erroredCases || rows.some((row) => row.unexpectedSizeMismatch)) process.exitCode = 1;
}

main().catch((error) => {
  console.error(`ppt-eval: ${error.message}`);
  process.exit(1);
});
