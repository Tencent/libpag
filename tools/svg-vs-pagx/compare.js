#!/usr/bin/env node
/**
 * svg-vs-pagx compare
 *
 * Compares per-frame PNGs produced by `svg-frames.js` (browser/SVG ground truth) and
 * `pagx render-frames` (PAGX renderer). Reuses the pixel metrics from html-snapshot/eval/compare.js
 * (pixelmatch + mean RGB delta + luma SSIM).
 *
 * Outputs:
 *   <output>/report.csv   per-frame pixelDiffRatio, meanRgbDelta, ssim
 *   <output>/diff/*.png   pixelmatch difference highlights (where frames differ)
 *   summary printed to stdout (mean SSIM, worst frame, mean pixel diff ratio)
 *
 * Usage:
 *   node compare.js --svg-dir svg_frames/ --pagx-dir pagx_frames/ --output out/
 */

'use strict';

const fs = require('fs');
const path = require('path');
const { pixelMetrics } = require('../html-snapshot/eval/compare.js');

function parseArgs(argv) {
  const opts = { svgDir: '', pagxDir: '', output: 'compare_out' };
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--svg-dir') opts.svgDir = argv[++i];
    else if (arg === '--pagx-dir') opts.pagxDir = argv[++i];
    else if (arg === '--output' || arg === '-o') opts.output = argv[++i];
    else if (arg === '--help' || arg === '-h') {
      console.log('Usage: node compare.js --svg-dir svg/ --pagx-dir pagx/ --output out/');
      process.exit(0);
    }
  }
  if (!opts.svgDir || !opts.pagxDir) {
    console.error('compare: missing --svg-dir or --pagx-dir');
    process.exit(1);
  }
  return opts;
}

function main() {
  const opts = parseArgs(process.argv);
  const svgDir = path.resolve(opts.svgDir);
  const pagxDir = path.resolve(opts.pagxDir);

  const svgFiles = fs.readdirSync(svgDir).filter((f) => f.endsWith('.png')).sort();
  if (svgFiles.length === 0) {
    console.error(`compare: no PNG frames in ${svgDir}`);
    process.exit(1);
  }

  const diffDir = path.join(opts.output, 'diff');
  fs.mkdirSync(diffDir, { recursive: true });

  const rows = [];
  let sumSsim = 0;
  let sumPixelDiff = 0;
  let compared = 0;
  let worstFrame = null;
  let worstSsim = 1;

  for (const name of svgFiles) {
    const svgPath = path.join(svgDir, name);
    const pagxPath = path.join(pagxDir, name);
    if (!fs.existsSync(pagxPath)) {
      console.error(`compare: missing PAGX frame ${pagxPath} (skipping ${name})`);
      continue;
    }
    const diffPath = path.join(diffDir, name);
    const m = pixelMetrics(svgPath, pagxPath, diffPath);
    rows.push({ frame: name, ...m });
    sumSsim += m.ssim;
    sumPixelDiff += m.pixelDiffRatio;
    compared++;
    if (m.ssim < worstSsim) {
      worstSsim = m.ssim;
      worstFrame = { frame: name, ...m };
    }
  }

  if (compared === 0) {
    console.error('compare: no comparable frames');
    process.exit(1);
  }

  // Write report.csv
  const csvLines = ['frame,pixelDiffRatio,meanRgbDelta,ssim'];
  for (const r of rows) {
    csvLines.push(`${r.frame},${r.pixelDiffRatio.toFixed(6)},${r.meanRgbDelta.toFixed(6)},${r.ssim.toFixed(6)}`);
  }
  fs.mkdirSync(opts.output, { recursive: true });
  fs.writeFileSync(path.join(opts.output, 'report.csv'), csvLines.join('\n') + '\n');

  const meanSsim = sumSsim / compared;
  const meanPixelDiff = sumPixelDiff / compared;
  console.log(`compare: ${compared} frames compared`);
  console.log(`  mean SSIM:        ${meanSsim.toFixed(4)} (1.0 = identical)`);
  console.log(`  mean pixel diff:  ${(meanPixelDiff * 100).toFixed(3)}%`);
  console.log(`  worst frame:      ${worstFrame.frame} (ssim=${worstFrame.ssim.toFixed(4)}, diff=${(worstFrame.pixelDiffRatio * 100).toFixed(3)}%)`);
  console.log(`  report:           ${path.join(opts.output, 'report.csv')}`);
  console.log(`  diff images:      ${diffDir}/`);
}

main();
