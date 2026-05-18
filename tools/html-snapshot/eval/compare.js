#!/usr/bin/env node
/**
 * compare.js — given a baseline PNG (Chromium-rendered original HTML) and a
 * subset PNG (`pagx render` of `pagx import` on the snapshot), produce a
 * per-case row of metrics:
 *
 *   - pixelDiffRatio: fraction of differing pixels via pixelmatch (threshold 0.1)
 *   - meanRgbDelta:   mean per-channel absolute difference, 0..255
 *   - ssim:           luma-only SSIM (single-window block average) for a coarse
 *                     structural signal that survives subpixel offsets
 *   - flexRetained / flexTotal: ratio of flex containers preserved in the
 *                     subset HTML compared with the rendered live DOM
 *   - importerWarnings: warning count from `pagx import` stderr (parsed)
 *
 * The PNGs may differ in size when the snapshot fails to capture the full
 * body. We resize-pad the smaller image with white pixels rather than
 * resampling so the structural signal doesn't get smoothed away.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { PNG } = require('pngjs');
const pixelmatch = require('pixelmatch');

function loadPng(filePath) {
  const data = fs.readFileSync(filePath);
  return PNG.sync.read(data);
}

// Pad to common size with white pixels. We never resample because that would
// smooth out the structural mismatches we want to measure.
function padToCommon(a, b) {
  const w = Math.max(a.width, b.width);
  const h = Math.max(a.height, b.height);
  const padOne = (img) => {
    if (img.width === w && img.height === h) return img;
    const out = new PNG({ width: w, height: h });
    out.data.fill(255);
    for (let y = 0; y < img.height; y++) {
      const srcStart = y * img.width * 4;
      const dstStart = y * w * 4;
      img.data.copy(out.data, dstStart, srcStart, srcStart + img.width * 4);
    }
    return out;
  };
  return [padOne(a), padOne(b), w, h];
}

function meanRgbDelta(aData, bData, w, h) {
  let total = 0;
  const n = w * h;
  for (let i = 0; i < n; i++) {
    const j = i * 4;
    const dr = Math.abs(aData[j] - bData[j]);
    const dg = Math.abs(aData[j + 1] - bData[j + 1]);
    const db = Math.abs(aData[j + 2] - bData[j + 2]);
    total += (dr + dg + db) / 3;
  }
  return total / n;
}

// Luma-only single-window SSIM. We deliberately avoid the standard 8x8 sliding
// window to keep this dependency-free and still get a usable structural metric;
// the SSIM contribution is dominated by global mean/variance, which the single-
// window form captures well enough to rank cases relative to one another.
function lumaSsim(aData, bData, w, h) {
  const n = w * h;
  let sumA = 0;
  let sumB = 0;
  for (let i = 0; i < n; i++) {
    const j = i * 4;
    const lA = 0.2126 * aData[j] + 0.7152 * aData[j + 1] + 0.0722 * aData[j + 2];
    const lB = 0.2126 * bData[j] + 0.7152 * bData[j + 1] + 0.0722 * bData[j + 2];
    sumA += lA;
    sumB += lB;
  }
  const muA = sumA / n;
  const muB = sumB / n;
  let varA = 0;
  let varB = 0;
  let covAB = 0;
  for (let i = 0; i < n; i++) {
    const j = i * 4;
    const lA = 0.2126 * aData[j] + 0.7152 * aData[j + 1] + 0.0722 * aData[j + 2];
    const lB = 0.2126 * bData[j] + 0.7152 * bData[j + 1] + 0.0722 * bData[j + 2];
    varA += (lA - muA) * (lA - muA);
    varB += (lB - muB) * (lB - muB);
    covAB += (lA - muA) * (lB - muB);
  }
  varA /= n;
  varB /= n;
  covAB /= n;
  const c1 = (0.01 * 255) * (0.01 * 255);
  const c2 = (0.03 * 255) * (0.03 * 255);
  const num = (2 * muA * muB + c1) * (2 * covAB + c2);
  const den = (muA * muA + muB * muB + c1) * (varA + varB + c2);
  if (den === 0) return 1;
  return num / den;
}

function pixelMetrics(baselinePath, subsetPath, diffPath) {
  const a = loadPng(baselinePath);
  const b = loadPng(subsetPath);
  const [pa, pb, w, h] = padToCommon(a, b);
  const diff = new PNG({ width: w, height: h });
  const diffPixels = pixelmatch(pa.data, pb.data, diff.data, w, h, {
    threshold: 0.1,
    includeAA: false,
  });
  if (diffPath) {
    fs.writeFileSync(diffPath, PNG.sync.write(diff));
  }
  return {
    width: w,
    height: h,
    pixelDiffRatio: diffPixels / (w * h),
    meanRgbDelta: meanRgbDelta(pa.data, pb.data, w, h),
    ssim: lumaSsim(pa.data, pb.data, w, h),
  };
}

// Count `display: flex` declarations in the subset HTML, and (for context) in
// the live original DOM measured at snapshot time. The ratio approximates how
// much of the original flex layout survived the import.
async function countFlexInLiveDom(puppeteer, htmlPath, viewportWidth, viewportHeight, waitMs) {
  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--font-render-hinting=none'],
  });
  try {
    const page = await browser.newPage();
    await page.setViewport({ width: viewportWidth, height: viewportHeight, deviceScaleFactor: 1 });
    await page.goto('file://' + htmlPath, { waitUntil: 'networkidle0', timeout: 30000 });
    try {
      await page.waitForFunction(
        'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true',
        { timeout: 10000 },
      );
    } catch (_) { /* not fatal */ }
    if (waitMs > 0) await new Promise((r) => setTimeout(r, waitMs));
    // Capture the count before closing the page; otherwise the value would be
    // returned via a still-open evaluation handle that gets cancelled when the
    // browser shuts down (TargetCloseError surfaces as an unhandled rejection).
    const total = await page.evaluate(() => {
      // Count every element whose computed display starts with `flex` (i.e.
      // `flex` and `inline-flex`). We exclude `display: contents` and the
      // synthetic <body> root.
      let n = 0;
      const all = document.body.querySelectorAll('*');
      for (const el of all) {
        const cs = getComputedStyle(el);
        const d = cs.display || '';
        if (d === 'flex' || d === 'inline-flex') n++;
      }
      return n;
    });
    await page.close();
    return total;
  } finally {
    await browser.close();
  }
}

function countFlexInSubsetHtml(htmlPath) {
  if (!fs.existsSync(htmlPath)) return 0;
  const text = fs.readFileSync(htmlPath, 'utf8');
  // Match `display: flex` (with any spacing) inside style="..." attributes.
  const re = /display\s*:\s*flex\b/gi;
  let count = 0;
  let m;
  while ((m = re.exec(text)) !== null) count++;
  return count;
}

function countImporterWarnings(stderrPath) {
  if (!fs.existsSync(stderrPath)) return { total: 0, flexInferred: 0, flexSkipped: 0 };
  const text = fs.readFileSync(stderrPath, 'utf8');
  const lines = text.split(/\r?\n/);
  let total = 0;
  let flexInferred = 0;
  let flexSkipped = 0;
  for (const line of lines) {
    if (!line.trim()) continue;
    if (/warning:/i.test(line) || /\[subset:/i.test(line)) total++;
    if (/subset:flex-inferred/.test(line)) flexInferred++;
    if (/subset:flex-inference-skipped/.test(line)) flexSkipped++;
  }
  return { total, flexInferred, flexSkipped };
}

module.exports = {
  pixelMetrics,
  countFlexInLiveDom,
  countFlexInSubsetHtml,
  countImporterWarnings,
};

// CLI mode: ad-hoc per-case comparison.
if (require.main === module) {
  const [, , baselinePath, subsetPath, diffPath] = process.argv;
  if (!baselinePath || !subsetPath) {
    console.error('Usage: compare.js <baseline.png> <subset.png> [diff.png]');
    process.exit(2);
  }
  const m = pixelMetrics(baselinePath, subsetPath, diffPath);
  console.log(JSON.stringify(m, null, 2));
}
