'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

function fixture(t) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ppt-summary-test-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const environment = { key: 'test', platform: 'darwin', arch: 'arm64', renderer: 'LO', rasterizer: 'Poppler' };
  const metrics = { ssim: 0.9, pixelDiffRatio: 0.1, meanRgbDelta: 10, roiSsim: 0.8, roiMeanRgbDelta: 12 };
  const report = {
    corpus: 'smoke', environment, expectedCases: 1,
    summary: { cases: 1, measuredPages: 1, erroredCases: 0, ssimMean: 0.9, pdMean: 0.1, rgbMean: 10, roiSsimMean: 0.8, roiRgbMean: 12 },
    rows: [{ case: 'sample', page: 1, ...metrics }],
  };
  const entry = { environment, ...report.summary, pages: { 'sample#1': { ...metrics } } };
  const baseline = { corpora: { smoke: { environments: { test: entry } } } };
  const baselinePath = path.join(dir, 'baseline.json');
  fs.mkdirSync(path.join(dir, 'ppt-smoke'));
  const run = (args = []) => {
    fs.writeFileSync(path.join(dir, 'ppt-smoke/report.json'), JSON.stringify(report));
    fs.writeFileSync(baselinePath, JSON.stringify(baseline));
    return spawnSync(process.execPath, [path.join(__dirname, 'summary.js'), '--out', dir,
      '--baseline', baselinePath, ...args, 'smoke'], { encoding: 'utf8' });
  };
  return { dir, report, baseline, entry, run };
}

test('empty baselines fail strict mode and warn in report-only mode', (t) => {
  const f = fixture(t);
  f.baseline.corpora = {};
  const strict = f.run(['--require-baseline']);
  assert.equal(strict.status, 1);
  assert.match(strict.stdout, /FAIL \(no baseline entry\)/);
  assert.match(strict.stderr, /WARNING: PPT baseline is empty/);
  const reportOnly = f.run();
  assert.equal(reportOnly.status, 0);
  assert.match(reportOnly.stdout, /SKIP/);
  assert.match(reportOnly.stderr, /WARNING/);
});

test('incompatible environments never pass silently', (t) => {
  const f = fixture(t);
  f.entry.environment = { ...f.entry.environment, renderer: 'different LO' };
  assert.equal(f.run(['--require-baseline']).status, 1);
  const result = f.run();
  assert.equal(result.status, 0);
  assert.match(result.stderr, /all PPT baseline gates were SKIP/);
});

test('partial tolerances merge defaults, corpus and environment for means and pages', (t) => {
  const f = fixture(t);
  f.baseline.corpora.smoke.tolerance = { ssim: 0.01, rgb: 4 };
  f.baseline.corpora.smoke.pageTolerance = { ssim: 0.01, rgb: 7 };
  f.entry.tolerance = { ssim: 0.05 };
  f.entry.pageTolerance = { ssim: 0.06 };
  f.report.summary.ssimMean = 0.86;
  f.report.summary.rgbMean = 13;
  f.report.rows[0].ssim = 0.85;
  f.report.rows[0].meanRgbDelta = 16;
  const result = f.run(['--require-baseline']);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /PASS/);
  assert.doesNotMatch(result.stdout, /BAD/);
  f.report.rows[0].roiMeanRgbDelta = 21;
  const regression = f.run(['--require-baseline']);
  assert.equal(regression.status, 1);
  assert.match(regression.stdout, /sample#1 roiMeanRgbDelta/);
});

test('baseline updates are explicitly ungated and preserve tolerance overrides', (t) => {
  const f = fixture(t);
  f.entry.tolerance = { ssim: 0.04 };
  f.entry.pageTolerance = { rgb: 9 };
  f.report.summary.ssimMean = 0.5;
  const result = f.run(['--update-baseline', '--require-baseline']);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /regression gate was not run/);
  assert.doesNotMatch(result.stdout, /PASS|\[ok/);
  const html = fs.readFileSync(path.join(f.dir, 'summary.html'), 'utf8');
  assert.match(html, /UPDATED/);
  assert.doesNotMatch(html, />PASS</);
  const saved = JSON.parse(fs.readFileSync(path.join(f.dir, 'baseline.json')));
  const entry = saved.corpora.smoke.environments.test;
  assert.equal(entry.ssimMean, 0.5);
  assert.deepEqual(entry.tolerance, { ssim: 0.04 });
  assert.deepEqual(entry.pageTolerance, { rgb: 9 });
});

test('filtered or failed runs cannot seed a baseline', (t) => {
  const f = fixture(t);
  f.report.expectedCases = null;
  assert.equal(f.run(['--update-baseline']).status, 1);
  f.report.expectedCases = 1;
  f.report.summary.erroredCases = 1;
  assert.equal(f.run(['--update-baseline']).status, 1);
});
