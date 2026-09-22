#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const finite = (value) => typeof value === 'number' && Number.isFinite(value);
const format = (value, digits = 4) => finite(value) ? value.toFixed(digits) : '-';
const percent = (value, digits = 2) => finite(value) ? `${(value * 100).toFixed(digits)}%` : '-';

function mean(values) {
  const usable = values.filter(finite);
  return usable.length ? usable.reduce((total, value) => total + value, 0) / usable.length : NaN;
}

function median(values) {
  const usable = values.filter(finite).sort((a, b) => a - b);
  if (!usable.length) return NaN;
  const middle = Math.floor(usable.length / 2);
  return usable.length % 2 ? usable[middle] : (usable[middle - 1] + usable[middle]) / 2;
}

function summarize(rows) {
  const measured = rows.filter((row) => !row.error && !row.skipped && finite(row.ssim));
  const cases = new Set(rows.map((row) => row.case));
  const erroredCases = new Set(rows.filter((row) => row.error).map((row) => row.case));
  const skippedCases = new Set(rows.filter((row) => row.skipped).map((row) => row.case));
  return {
    cases: cases.size,
    pages: rows.length,
    measuredPages: measured.length,
    erroredCases: erroredCases.size,
    skippedCases: skippedCases.size,
    sizeMismatches: measured.filter((row) => row.unexpectedSizeMismatch).length,
    ssimMean: mean(measured.map((row) => row.ssim)),
    ssimMedian: median(measured.map((row) => row.ssim)),
    pdMean: mean(measured.map((row) => row.pixelDiffRatio)),
    rgbMean: mean(measured.map((row) => row.meanRgbDelta)),
    roiSsimMean: mean(measured.map((row) => row.roiSsim)),
    roiRgbMean: mean(measured.map((row) => row.roiMeanRgbDelta)),
  };
}

function csvEscape(value) {
  const text = value === undefined || value === null ? '' : String(value);
  return /[",\r\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}

function writeCsv(rows, output) {
  const fields = [
    'corpus', 'case', 'page', 'source', 'reference', 'actual', 'diff',
    'referenceSize', 'actualSize', 'sizeMismatch', 'ssim', 'pixelDiffRatio',
    'expectedActualSize', 'unexpectedSizeMismatch', 'meanRgbDelta', 'roiSsim',
    'roiMeanRgbDelta', 'exportMs', 'rasterMs',
    'renderer', 'skipped', 'error',
  ];
  const lines = [fields.join(',')];
  for (const row of rows) {
    lines.push(fields.map((field) => csvEscape(row[field])).join(','));
  }
  fs.writeFileSync(output, `${lines.join('\n')}\n`, 'utf8');
}

function writeMarkdown(rows, output, corpus, environment) {
  const summary = summarize(rows);
  const lines = [
    `# PPT eval: ${corpus}`,
    '',
    `Renderer: \`${environment.renderer}\``,
    '',
    `Environment: \`${environment.key}\``,
    '',
    `Cases: ${summary.cases}; measured pages: ${summary.measuredPages}; ` +
      `errored cases: ${summary.erroredCases}; skipped cases: ${summary.skippedCases}.`,
    '',
    `- SSIM mean/median: ${format(summary.ssimMean)} / ${format(summary.ssimMedian)}`,
    `- Pixel diff mean: ${percent(summary.pdMean)}`,
    `- Mean RGB delta: ${format(summary.rgbMean, 2)}`,
    `- Content ROI SSIM mean: ${format(summary.roiSsimMean)}`,
    `- Content ROI RGB delta mean: ${format(summary.roiRgbMean, 2)}`,
    `- Size mismatches: ${summary.sizeMismatches}`,
    '',
    '| case | page | SSIM | diff | RGB Δ | ROI SSIM | ROI RGB Δ | size | status |',
    '| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |',
  ];
  for (const row of rows) {
    const status = row.error ? `ERROR: ${row.error}` : row.skipped ? `SKIP: ${row.skipped}`
      : row.unexpectedSizeMismatch ? `ERROR: expected PPT page ${row.expectedActualSize}` : 'OK';
    lines.push(
      `| ${row.case} | ${row.page || '-'} | ${format(row.ssim)} | ` +
      `${percent(row.pixelDiffRatio)} | ${format(row.meanRgbDelta, 2)} | ` +
      `${format(row.roiSsim)} | ${format(row.roiMeanRgbDelta, 2)} | ` +
      `${row.referenceSize || '-'} / ${row.actualSize || '-'} | ${status.replace(/\|/g, '\\|')} |`,
    );
  }
  fs.writeFileSync(output, `${lines.join('\n')}\n`, 'utf8');
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (char) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
  }[char]));
}

function metricClass(ssim) {
  if (!finite(ssim)) return 'na';
  if (ssim >= 0.95) return 'good';
  if (ssim >= 0.75) return 'mid';
  return 'bad';
}

function writeHtml(rows, output, corpus, environment) {
  const summary = summarize(rows);
  const cards = rows.map((row) => {
    const state = row.error ? `<span class="error">${escapeHtml(row.error)}</span>`
      : row.skipped ? `<span class="skip">${escapeHtml(row.skipped)}</span>`
      : row.unexpectedSizeMismatch
        ? `<span class="error">expected ${escapeHtml(row.expectedActualSize)}</span>` : '';
    const images = row.error || row.skipped ? '' : `
      <div class="images">
        <figure><figcaption>PAGX reference</figcaption><img loading="lazy" src="${escapeHtml(row.reference)}"></figure>
        <figure><figcaption>LibreOffice</figcaption><img loading="lazy" src="${escapeHtml(row.actual)}"></figure>
        <figure><figcaption>Diff</figcaption><img loading="lazy" src="${escapeHtml(row.diff)}"></figure>
      </div>`;
    return `<section class="case" data-name="${escapeHtml(row.case)}" data-ssim="${finite(row.ssim) ? row.ssim : -1}">
      <h2>${escapeHtml(row.case)} <small>page ${row.page || '-'}</small>
        <span class="metric ${metricClass(row.ssim)}">SSIM ${format(row.ssim)}</span></h2>
      <p>diff ${percent(row.pixelDiffRatio)} · RGB Δ ${format(row.meanRgbDelta, 2)} · ` +
      `ROI SSIM ${format(row.roiSsim)} · size ${escapeHtml(row.referenceSize || '-')} / ` +
      `${escapeHtml(row.actualSize || '-')} ${state}</p>${images}
    </section>`;
  }).join('\n');
  const html = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>PPT eval: ${escapeHtml(corpus)}</title>
<style>
  :root { color-scheme: dark; } * { box-sizing: border-box; }
  body { margin: 0; padding: 24px; font: 14px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; background:#181818; color:#ddd; }
  h1 { margin:0 0 4px; font-size:22px; } .meta { color:#999; margin-bottom:16px; }
  .summary,.case { background:#242424; border:1px solid #393939; border-radius:8px; padding:14px 16px; margin-bottom:16px; }
  .summary { display:flex; gap:22px; flex-wrap:wrap; } .summary b { color:#68d5bd; }
  .toolbar { display:flex; gap:12px; align-items:center; margin:16px 0; }
  input[type=text] { width:280px; padding:7px 10px; border:1px solid #555; border-radius:5px; background:#303030; color:#fff; }
  h2 { font-size:15px; color:#e7d995; margin:0 0 6px; display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
  h2 small { color:#999; font-weight:400; } p { color:#aaa; margin:0 0 10px; }
  .metric { border-radius:999px; padding:2px 8px; font-size:11px; } .good{background:#14532d}.mid{background:#713f12}.bad{background:#7f1d1d}.na{background:#444}
  .error{color:#ff8080}.skip{color:#f7c873}.hidden{display:none}
  .images { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:10px; }
  figure { margin:0; padding:8px; border:1px solid #383838; border-radius:6px; background:#1b1b1b; }
  figcaption { color:#999; text-align:center; font-size:12px; margin-bottom:6px; }
  img { display:block; width:100%; height:auto; background:repeating-conic-gradient(#333 0 25%,#292929 0 50%) 0/20px 20px; }
  @media(max-width:900px){.images{grid-template-columns:1fr}}
</style></head><body>
<h1>PPT visual eval: ${escapeHtml(corpus)}</h1>
<div class="meta">${escapeHtml(environment.renderer)} · ${escapeHtml(environment.key)}</div>
<div class="summary">
  <span>cases <b>${summary.cases}</b></span><span>pages <b>${summary.measuredPages}</b></span>
  <span>SSIM <b>${format(summary.ssimMean)}</b></span><span>diff <b>${percent(summary.pdMean)}</b></span>
  <span>ROI SSIM <b>${format(summary.roiSsimMean)}</b></span><span>errors <b>${summary.erroredCases}</b></span>
</div>
<div class="toolbar"><input id="filter" type="text" placeholder="Filter cases"><label><input id="sort" type="checkbox"> worst first</label></div>
<main id="cases">${cards}</main>
<script>
const root=document.getElementById('cases');const cards=[...root.querySelectorAll('.case')];
document.getElementById('filter').addEventListener('input',e=>{const q=e.target.value.toLowerCase();cards.forEach(c=>c.classList.toggle('hidden',q&&!c.dataset.name.toLowerCase().includes(q)))});
document.getElementById('sort').addEventListener('change',e=>{const list=e.target.checked?[...cards].sort((a,b)=>Number(a.dataset.ssim)-Number(b.dataset.ssim)):cards;list.forEach(c=>root.appendChild(c))});
</script></body></html>`;
  fs.writeFileSync(output, html, 'utf8');
}

module.exports = { summarize, writeCsv, writeHtml, writeMarkdown };
