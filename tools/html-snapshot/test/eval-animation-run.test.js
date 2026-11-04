'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const EVAL_DIR = path.resolve(__dirname, '../eval-animation');
const run = require(path.join(EVAL_DIR, 'run.js'));

function tempDir() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'anim-eval-'));
}

describe('case cache key', () => {
  test('reuses frames only for the same source and sample grid', () => {
    const dir = tempDir();
    const source = path.join(dir, 'case.pagx');
    const keyPath = path.join(dir, '.cache-key.json');
    const opts = { samples: 8, roundtrip: false };
    fs.writeFileSync(source, '<pagx width="10" height="10"/>');

    const key = run.buildCaseCacheKey(source, opts);
    // No manifest yet: nothing can be reused.
    expect(run.cacheKeyMatches(keyPath, key)).toBe(false);

    run.writeCaseCacheKey(keyPath, key);
    expect(run.cacheKeyMatches(keyPath, key)).toBe(true);

    // A different sample grid renders different frames, so the cached ones no longer apply.
    expect(run.cacheKeyMatches(keyPath, run.buildCaseCacheKey(source, { samples: 5 }))).toBe(false);
  });

  test('misses when the source file changes', () => {
    const dir = tempDir();
    const source = path.join(dir, 'case.pagx');
    const keyPath = path.join(dir, '.cache-key.json');
    const opts = { samples: 8, roundtrip: false };
    fs.writeFileSync(source, '<pagx width="10" height="10"/>');
    const before = run.buildCaseCacheKey(source, opts);
    run.writeCaseCacheKey(keyPath, before);

    fs.writeFileSync(source, '<pagx width="10" height="10"><Layer/></pagx>');
    const after = run.buildCaseCacheKey(source, opts);
    expect(after.source.size).not.toBe(before.source.size);
    expect(run.cacheKeyMatches(keyPath, after)).toBe(false);
  });
});

describe('run.js exit code', () => {
  test('reports failure when every case errors', () => {
    const dir = tempDir();
    const corpus = path.join(dir, 'corpus');
    fs.mkdirSync(corpus);
    fs.writeFileSync(path.join(corpus, 'case.pagx'), '<pagx width="10" height="10"/>');

    // A pagx binary that exists but refuses to run: every case fails, and the run must say so
    // instead of exiting 0 just because it managed to write a report.
    const fakeBin = path.join(dir, 'pagx');
    fs.writeFileSync(fakeBin, '#!/bin/sh\nexit 1\n');
    fs.chmodSync(fakeBin, 0o755);

    const result = spawnSync(
      process.execPath,
      [
        path.join(EVAL_DIR, 'run.js'),
        '--source', 'pagx',
        '--corpus', corpus,
        '--out', path.join(dir, 'out'),
        '--pagx-bin', fakeBin,
      ],
      { encoding: 'utf8' },
    );

    expect(result.status).toBe(1);
    expect(result.stderr).toMatch(/case\(s\) failed/);
  }, 60000);
});

describe('baseline-frames sample grid', () => {
  test('rejects a missing times file instead of computing its own samples', () => {
    const dir = tempDir();
    const html = path.join(dir, 'case.html');
    fs.writeFileSync(html, '<!DOCTYPE html><html><body></body></html>');

    const result = spawnSync(
      process.execPath,
      [
        path.join(EVAL_DIR, 'baseline-frames.js'),
        html,
        '-o', path.join(dir, 'frames'),
        '--times-json', path.join(dir, 'missing.json'),
      ],
      { encoding: 'utf8' },
    );

    expect(result.status).not.toBe(0);
    expect(result.stderr).toMatch(/times file not found/);
  }, 30000);
});
