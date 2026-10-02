#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

function parseArgs(argv) {
  const options = {
    outDir: path.join(__dirname, 'out'),
    baseline: '',
    updateBaseline: false,
    requireBaseline: false,
    labels: [],
  };
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--out') options.outDir = path.resolve(argv[++i]);
    else if (arg === '--baseline') options.baseline = path.resolve(argv[++i]);
    else if (arg === '--update-baseline') options.updateBaseline = true;
    else if (arg === '--require-baseline') options.requireBaseline = true;
    else if (arg === '-h' || arg === '--help') {
      console.log('Usage: node summary.js --out <dir> --baseline <file> [--update-baseline] [--require-baseline] <label>...');
      process.exit(0);
    } else options.labels.push(arg);
  }
  if (!options.labels.length) throw new Error('no corpus labels given');
  return options;
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

const finite = (value) => typeof value === 'number' && Number.isFinite(value);
const format = (value, digits = 4) => finite(value) ? value.toFixed(digits) : '-';
const percent = (value) => finite(value) ? `${(value * 100).toFixed(2)}%` : '-';

function environmentCompatible(expected, actual) {
  if (!expected) return false;
  return expected.platform === actual.platform &&
    expected.arch === actual.arch &&
    expected.renderer === actual.renderer &&
    expected.rasterizer === actual.rasterizer;
}

function tolerance(corpusEntry, environmentEntry) {
  return environmentEntry.tolerance || corpusEntry.tolerance || {
    ssim: 0.02,
    pd: 0.02,
    rgb: 2,
    roiSsim: 0.03,
    roiRgb: 3,
  };
}

function pageKey(row) {
  return `${row.case}#${row.page}`;
}

function checkCorpus(report, baseline, requireBaseline) {
  const corpusEntry = baseline && baseline.corpora && baseline.corpora[report.corpus];
  if (!corpusEntry) return requireBaseline
    ? { status: 'FAIL', detail: 'no baseline entry', checks: [] }
    : { status: 'SKIP', detail: 'no baseline entry', checks: [] };
  // The environments map allows Linux CI and local macOS runs to keep separate
  // baselines without overwriting each other. Accept the original single-entry
  // shape as a compatibility path for early adopters of this tool.
  const entry = corpusEntry.environments
    ? corpusEntry.environments[report.environment.key]
    : corpusEntry;
  if (!entry || !environmentCompatible(entry.environment, report.environment)) {
    return requireBaseline
      ? { status: 'FAIL', detail: 'baseline belongs to a different renderer environment', checks: [] }
      : { status: 'SKIP', detail: 'baseline belongs to a different renderer environment', checks: [] };
  }
  if (report.expectedCases === null) {
    return requireBaseline
      ? { status: 'FAIL', detail: 'filtered run cannot satisfy strict baseline mode', checks: [] }
      : { status: 'SKIP', detail: 'filtered run', checks: [] };
  }
  const current = report.summary;
  const allowed = tolerance(corpusEntry, entry);
  const checks = [];
  const compare = (name, actual, reference, direction, delta) => {
    if (!finite(reference)) return;
    const limit = direction === 'higher' ? reference - delta : reference + delta;
    const pass = direction === 'higher' ? actual >= limit : actual <= limit;
    checks.push({ name, actual, reference, limit, direction, pass });
  };
  compare('ssimMean', current.ssimMean, entry.ssimMean, 'higher', allowed.ssim);
  compare('pdMean', current.pdMean, entry.pdMean, 'lower', allowed.pd);
  compare('rgbMean', current.rgbMean, entry.rgbMean, 'lower', allowed.rgb);
  compare('roiSsimMean', current.roiSsimMean, entry.roiSsimMean, 'higher', allowed.roiSsim);
  compare('roiRgbMean', current.roiRgbMean, entry.roiRgbMean, 'lower', allowed.roiRgb);
  if (current.cases !== entry.cases) {
    checks.push({ name: 'cases', actual: current.cases, reference: entry.cases, limit: entry.cases, direction: 'equal', pass: false });
  }
  if (entry.measuredPages !== undefined && current.measuredPages !== entry.measuredPages) {
    checks.push({ name: 'measuredPages', actual: current.measuredPages, reference: entry.measuredPages, limit: entry.measuredPages, direction: 'equal', pass: false });
  }

  const pageChecks = [];
  const pageTolerance = entry.pageTolerance || corpusEntry.pageTolerance || {
    ssim: 0.05,
    pd: 0.05,
    rgb: 5,
    roiSsim: 0.08,
    roiRgb: 8,
  };
  if (entry.pages) {
    const currentPages = new Map(
      report.rows.filter((row) => !row.error && !row.skipped && finite(row.ssim))
        .map((row) => [pageKey(row), row]),
    );
    for (const [key, pageBaseline] of Object.entries(entry.pages)) {
      const row = currentPages.get(key);
      if (!row) {
        pageChecks.push({ key, metric: 'present', pass: false, detail: 'missing page' });
        continue;
      }
      currentPages.delete(key);
      const comparePage = (metric, actual, reference, direction, delta) => {
        if (!finite(reference)) return;
        const limit = direction === 'higher' ? reference - delta : reference + delta;
        const pass = direction === 'higher' ? actual >= limit : actual <= limit;
        pageChecks.push({ key, metric, actual, reference, limit, direction, pass });
      };
      comparePage('ssim', row.ssim, pageBaseline.ssim, 'higher', pageTolerance.ssim);
      comparePage('pixelDiffRatio', row.pixelDiffRatio, pageBaseline.pixelDiffRatio, 'lower', pageTolerance.pd);
      comparePage('meanRgbDelta', row.meanRgbDelta, pageBaseline.meanRgbDelta, 'lower', pageTolerance.rgb);
      comparePage('roiSsim', row.roiSsim, pageBaseline.roiSsim, 'higher', pageTolerance.roiSsim);
      comparePage('roiMeanRgbDelta', row.roiMeanRgbDelta, pageBaseline.roiMeanRgbDelta, 'lower', pageTolerance.roiRgb);
    }
    for (const key of currentPages.keys()) {
      pageChecks.push({ key, metric: 'present', pass: false, detail: 'new page not present in baseline' });
    }
  }
  return {
    status: checks.every((check) => check.pass) && pageChecks.every((check) => check.pass)
      ? 'PASS' : 'FAIL',
    checks,
    pageChecks,
  };
}

function writeBaseline(filePath, reports) {
  let previous = {};
  if (fs.existsSync(filePath)) previous = readJson(filePath);
  const corpora = { ...(previous.corpora || {}) };
  for (const report of reports) {
    const oldCorpus = previous.corpora && previous.corpora[report.corpus];
    const oldEnvironments = oldCorpus && oldCorpus.environments
      ? oldCorpus.environments
      : oldCorpus && oldCorpus.environment
        ? { [oldCorpus.environment.key]: oldCorpus }
        : {};
    const oldEnvironment = oldEnvironments[report.environment.key];
    const pages = {};
    for (const row of report.rows) {
      if (row.error || row.skipped || !finite(row.ssim)) continue;
      pages[pageKey(row)] = {
        ssim: Number(row.ssim.toFixed(6)),
        pixelDiffRatio: Number(row.pixelDiffRatio.toFixed(6)),
        meanRgbDelta: Number(row.meanRgbDelta.toFixed(4)),
        roiSsim: Number(row.roiSsim.toFixed(6)),
        roiMeanRgbDelta: Number(row.roiMeanRgbDelta.toFixed(4)),
      };
    }
    const environmentEntry = {
      environment: report.environment,
      cases: report.summary.cases,
      measuredPages: report.summary.measuredPages,
      ssimMean: finite(report.summary.ssimMean) ? Number(report.summary.ssimMean.toFixed(6)) : null,
      pdMean: finite(report.summary.pdMean) ? Number(report.summary.pdMean.toFixed(6)) : null,
      rgbMean: finite(report.summary.rgbMean) ? Number(report.summary.rgbMean.toFixed(4)) : null,
      roiSsimMean: finite(report.summary.roiSsimMean) ? Number(report.summary.roiSsimMean.toFixed(6)) : null,
      roiRgbMean: finite(report.summary.roiRgbMean) ? Number(report.summary.roiRgbMean.toFixed(4)) : null,
      ...(oldEnvironment && oldEnvironment.tolerance ? { tolerance: oldEnvironment.tolerance } : {}),
      ...(oldEnvironment && oldEnvironment.pageTolerance ? { pageTolerance: oldEnvironment.pageTolerance } : {}),
      pages,
    };
    corpora[report.corpus] = {
      ...(oldCorpus && oldCorpus.tolerance ? { tolerance: oldCorpus.tolerance } : {}),
      ...(oldCorpus && oldCorpus.pageTolerance ? { pageTolerance: oldCorpus.pageTolerance } : {}),
      environments: {
        ...oldEnvironments,
        [report.environment.key]: environmentEntry,
      },
    };
  }
  const output = {
    _comment: 'Renderer-specific corpus means for PPTTest. Update only with PPT_EVAL_UPDATE_BASELINE=1 on a trusted, fixed environment.',
    updatedAt: new Date().toISOString(),
    corpora,
  };
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, `${JSON.stringify(output, null, 2)}\n`, 'utf8');
  console.log(`ppt-summary: wrote baseline ${filePath}`);
}

function renderHtml(reports, results) {
  const rows = reports.map((report, index) => {
    const result = results[index];
    return `<tr><td><a href="ppt-${report.corpus}/index.html">${report.corpus}</a></td>` +
      `<td>${report.summary.cases}</td><td>${report.summary.measuredPages}</td>` +
      `<td>${format(report.summary.ssimMean)}</td><td>${percent(report.summary.pdMean)}</td>` +
      `<td>${format(report.summary.rgbMean, 2)}</td><td>${format(report.summary.roiSsimMean)}</td>` +
      `<td class="${result.status.toLowerCase()}">${result.status}</td></tr>`;
  }).join('');
  return `<!doctype html><html><head><meta charset="utf-8"><title>PPT eval summary</title>
<style>body{font:14px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:24px}table{border-collapse:collapse;width:100%}th,td{padding:7px 10px;border-bottom:1px solid #ccc;text-align:right}th:first-child,td:first-child{text-align:left}.pass{color:#087f23}.fail{color:#c92a2a}.skip{color:#b26a00}</style></head>
<body><h1>PPT eval summary</h1><table><thead><tr><th>corpus</th><th>cases</th><th>pages</th><th>SSIM</th><th>diff</th><th>RGB Δ</th><th>ROI SSIM</th><th>gate</th></tr></thead><tbody>${rows}</tbody></table></body></html>`;
}

function main() {
  const options = parseArgs(process.argv);
  const reports = options.labels.map((label) => {
    const filePath = path.join(options.outDir, `ppt-${label}`, 'report.json');
    if (!fs.existsSync(filePath)) throw new Error(`missing report: ${filePath}`);
    return readJson(filePath);
  });
  if (reports.some((report) => report.summary.erroredCases > 0)) {
    throw new Error('one or more corpora contain failed cases');
  }
  if (options.updateBaseline) {
    if (!options.baseline) throw new Error('--update-baseline requires --baseline');
    if (reports.some((report) => report.expectedCases === null || report.summary.cases !== report.expectedCases)) {
      throw new Error('refusing to update baseline from a filtered or incomplete report');
    }
    writeBaseline(options.baseline, reports);
  }
  let baseline = null;
  if (options.baseline && fs.existsSync(options.baseline)) baseline = readJson(options.baseline);
  const results = reports.map((report) => checkCorpus(report, baseline, options.requireBaseline));
  console.log('');
  console.log('=== PPT eval baseline gate ===');
  reports.forEach((report, index) => {
    const result = results[index];
    console.log(`  ppt-${report.corpus}: ${result.status}${result.detail ? ` (${result.detail})` : ''}`);
    for (const check of result.checks || []) {
      const operator = check.direction === 'higher' ? '>=' : check.direction === 'lower' ? '<=' : '==';
      console.log(`    [${check.pass ? 'ok ' : 'BAD'}] ${check.name} ${format(check.actual)} ${operator} ${format(check.limit)} (base ${format(check.reference)})`);
    }
    const failedPages = (result.pageChecks || []).filter((check) => !check.pass);
    for (const check of failedPages.slice(0, 20)) {
      if (check.detail) console.log(`    [BAD] ${check.key}: ${check.detail}`);
      else {
        const operator = check.direction === 'higher' ? '>=' : '<=';
        console.log(`    [BAD] ${check.key} ${check.metric} ${format(check.actual)} ${operator} ${format(check.limit)} (base ${format(check.reference)})`);
      }
    }
    if (failedPages.length > 20) console.log(`    ... ${failedPages.length - 20} more per-page failures`);
  });
  fs.writeFileSync(path.join(options.outDir, 'summary.html'), renderHtml(reports, results));
  if (results.some((result) => result.status === 'FAIL')) process.exitCode = 1;
}

try {
  main();
} catch (error) {
  console.error(`ppt-summary: ${error.message}`);
  process.exit(1);
}
