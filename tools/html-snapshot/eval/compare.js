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
 *   - flex live/subset counts: number of `display:flex|inline-flex` elements
 *                     in the rendered live DOM vs. the rendered subset DOM
 *                     (both measured via getComputedStyle, so the counts are
 *                     directly comparable)
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
const { unwrap, newPage, mapWaitUntil } = require('../dist/lib/browser-engine');
// pixelmatch v7+ is ESM-only (`export default`). Under Node's ESM-from-CJS
// `require()` interop the module resolves to a namespace object whose default
// export is the function, so `require('pixelmatch')` itself is not callable.
// Unwrap defensively so both shapes (legacy CJS and the v7+ namespace) work.
const pixelmatchMod = require('pixelmatch');
const pixelmatch = typeof pixelmatchMod === 'function' ? pixelmatchMod : pixelmatchMod.default;

// Composite every pixel over an opaque white background, in place. The baseline
// is a Chromium screenshot (opaque, white page background); `pagx render`
// preserves transparency, so any region the page left unpainted comes back as
// (0,0,0,0). Without this step the downstream metrics read those transparent
// pixels as pure *black* (alpha is ignored by imageMetrics and not blended by
// pixelmatch here), which turns a faithful render into a huge false diff —
// e.g. a static page with a transparent body reads as ~21% pixel diff / SSIM
// below zero. Flattening over white makes both images apples-to-apples.
function flattenOverWhite(img) {
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const a = d[i + 3];
    if (a === 255) continue;
    const inv = 255 - a;
    d[i] = Math.round((d[i] * a + 255 * inv) / 255);
    d[i + 1] = Math.round((d[i + 1] * a + 255 * inv) / 255);
    d[i + 2] = Math.round((d[i + 2] * a + 255 * inv) / 255);
    d[i + 3] = 255;
  }
  return img;
}

function loadPng(filePath) {
  const data = fs.readFileSync(filePath);
  return flattenOverWhite(PNG.sync.read(data));
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

// Single-pass image metrics: walks both buffers exactly once and emits the
// mean per-channel RGB delta together with luma-only SSIM. Caching luma into
// Float32Arrays during the first pass lets the variance step skip the second
// luma recomputation. Luma weights match BT.709 (matching previous behaviour).
//
// SSIM is computed over a single global window — the dependency-free,
// approximate form chosen for this codebase. It tracks structural mismatches
// well enough to rank cases relative to one another but does not match the
// canonical 8x8 sliding-window value.
function imageMetrics(aData, bData, w, h) {
  const n = w * h;
  const lumaA = new Float32Array(n);
  const lumaB = new Float32Array(n);
  let rgbDeltaTotal = 0;
  let sumA = 0;
  let sumB = 0;
  for (let i = 0; i < n; i++) {
    const j = i * 4;
    const ar = aData[j];
    const ag = aData[j + 1];
    const ab = aData[j + 2];
    const br = bData[j];
    const bg = bData[j + 1];
    const bb = bData[j + 2];
    rgbDeltaTotal += (Math.abs(ar - br) + Math.abs(ag - bg) + Math.abs(ab - bb)) / 3;
    const lA = 0.2126 * ar + 0.7152 * ag + 0.0722 * ab;
    const lB = 0.2126 * br + 0.7152 * bg + 0.0722 * bb;
    lumaA[i] = lA;
    lumaB[i] = lB;
    sumA += lA;
    sumB += lB;
  }
  const muA = sumA / n;
  const muB = sumB / n;
  let varA = 0;
  let varB = 0;
  let covAB = 0;
  for (let i = 0; i < n; i++) {
    const dA = lumaA[i] - muA;
    const dB = lumaB[i] - muB;
    varA += dA * dA;
    varB += dB * dB;
    covAB += dA * dB;
  }
  varA /= n;
  varB /= n;
  covAB /= n;
  const c1 = (0.01 * 255) * (0.01 * 255);
  const c2 = (0.03 * 255) * (0.03 * 255);
  const num = (2 * muA * muB + c1) * (2 * covAB + c2);
  const den = (muA * muA + muB * muB + c1) * (varA + varB + c2);
  return {
    meanRgbDelta: rgbDeltaTotal / n,
    ssim: den === 0 ? 1 : num / den,
  };
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
  const m = imageMetrics(pa.data, pb.data, w, h);
  return {
    width: w,
    height: h,
    pixelDiffRatio: diffPixels / (w * h),
    meanRgbDelta: m.meanRgbDelta,
    ssim: m.ssim,
  };
}

// Count flex containers in a rendered DOM. Both the live original HTML and the
// subset HTML emitted by snapshot.js are rendered the same way and walked with
// `getComputedStyle`, so the two counts are apples-to-apples. Counting the
// subset via a `display:\s*flex` regex on the source (the previous approach)
// inflates the count because each <div> wrapper emits its own style attribute,
// and the live count uses computed style — the two metrics weren't comparable.
//
// `browserOrWrapper` accepts either a raw Puppeteer/Playwright browser or the
// `{ browser, engine }` wrapper returned by `lib/browser-engine`. The wrapper
// shape is preferred because it threads the engine name through to the
// per-engine API differences (viewport setter, networkidle token).
async function countFlexInRenderedHtml(browserOrWrapper, htmlPath, opts = {}) {
  const {
    viewportWidth = 1400,
    viewportHeight = 900,
    waitMs = 0,
    waitForRoot = false,
  } = opts;
  const { engine } = unwrap(browserOrWrapper);
  const page = await newPage(browserOrWrapper, {
    viewport: { width: viewportWidth, height: viewportHeight, deviceScaleFactor: 1 },
  });
  try {
    await page.goto('file://' + htmlPath, {
      waitUntil: mapWaitUntil(engine, 'networkidle'),
      timeout: 30000,
    });
    if (waitForRoot) {
      try {
        await page.waitForFunction(
          'document.querySelector("#root") ? document.querySelector("#root").children.length > 0 : true',
          { timeout: 10000 },
        );
      } catch (_) { /* not fatal */ }
    }
    if (waitMs > 0) await new Promise((r) => setTimeout(r, waitMs));
    // Capture the count before closing the page; otherwise the value would be
    // returned via a still-open evaluation handle that gets cancelled when the
    // browser shuts down (TargetCloseError surfaces as an unhandled rejection).
    const total = await page.evaluate(() => {
      // Count every body descendant whose computed display is `flex` or
      // `inline-flex`. Excluding <html>/<body> avoids counting global style
      // rules the snapshot pipeline injects (e.g. `html { display:flex }`).
      let n = 0;
      const all = document.body.querySelectorAll('*');
      for (const el of all) {
        const cs = getComputedStyle(el);
        const d = cs.display || '';
        if (d === 'flex' || d === 'inline-flex') n++;
      }
      return n;
    });
    return total;
  } finally {
    await page.close();
  }
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
  countFlexInRenderedHtml,
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
