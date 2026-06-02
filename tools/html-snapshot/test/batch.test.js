'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { runBatch, collectHtmlInputs } = require('../lib/batch');

let dir;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-batch-test-'));
});
afterEach(() => {
  fs.rmSync(dir, { recursive: true, force: true });
});

// Minimal helper — write an empty file at <dir>/<rel>, creating any
// missing parents along the way.
function touch(rel) {
  const full = path.join(dir, rel);
  fs.mkdirSync(path.dirname(full), { recursive: true });
  fs.writeFileSync(full, '');
  return full;
}

describe('collectHtmlInputs', () => {
  test('returns [] for an empty directory', () => {
    expect(collectHtmlInputs(dir)).toEqual([]);
  });

  test('returns [] when the directory does not exist', () => {
    expect(collectHtmlInputs(path.join(dir, 'missing'))).toEqual([]);
  });

  test('finds .html and .htm recursively, sorted', () => {
    touch('top.html');
    touch('a/inner.htm');
    touch('a/b/deep.html');
    touch('a/note.txt');
    const out = collectHtmlInputs(dir).map((p) => path.relative(dir, p));
    expect(out).toEqual([
      path.join('a', 'b', 'deep.html'),
      path.join('a', 'inner.htm'),
      'top.html',
    ]);
  });

  test('drops .subset.html and .subset.htm by-products', () => {
    touch('foo.html');
    touch('foo.subset.html');
    touch('bar.htm');
    touch('bar.subset.htm');
    const out = collectHtmlInputs(dir).map((p) => path.basename(p));
    expect(out.sort()).toEqual(['bar.htm', 'foo.html']);
  });

  test('matches case-insensitively', () => {
    touch('UPPER.HTML');
    touch('Mixed.Htm');
    const out = collectHtmlInputs(dir).map((p) => path.basename(p));
    expect(out.sort()).toEqual(['Mixed.Htm', 'UPPER.HTML']);
  });

  test('ignores non-files (symlinks to directories, etc.)', () => {
    touch('keep.html');
    fs.mkdirSync(path.join(dir, 'foo.html'), { recursive: true });
    const out = collectHtmlInputs(dir).map((p) => path.basename(p));
    expect(out).toEqual(['keep.html']);
  });
});

describe('runBatch — input validation', () => {
  test('throws when inputDir is missing', async () => {
    await expect(runBatch({})).rejects.toThrow(/inputDir is required/);
  });

  test('throws when inputDir does not exist', async () => {
    await expect(runBatch({ inputDir: path.join(dir, 'nope') }))
      .rejects.toThrow(/input directory not found/);
  });

  test('throws when inputDir is a file, not a directory', async () => {
    const file = touch('a.html');
    await expect(runBatch({ inputDir: file }))
      .rejects.toThrow(/input directory not found/);
  });

  test('returns zeroes when no html files are found', async () => {
    fs.mkdirSync(path.join(dir, 'in'));
    const realWrite = process.stdout.write.bind(process.stdout);
    const lines = [];
    process.stdout.write = (s) => { lines.push(String(s)); return true; };
    try {
      const r = await runBatch({ inputDir: path.join(dir, 'in'), pagxBin: '/p', scriptDir: dir });
      expect(r).toEqual({ processed: 0, skipped: 0, failed: 0, total: 0 });
    } finally {
      process.stdout.write = realWrite;
    }
    expect(lines.join('')).toMatch(/No non-subset HTML files found/);
  });
});

describe('runBatch — dryRun', () => {
  // Capture stdout for the dryRun path so we can check the per-file lines and
  // the Summary footer without polluting jest's console.
  function withCapturedStdout(fn) {
    const realWrite = process.stdout.write.bind(process.stdout);
    const lines = [];
    process.stdout.write = (s) => { lines.push(String(s)); return true; };
    return Promise.resolve(fn())
      .finally(() => { process.stdout.write = realWrite; })
      .then((r) => ({ result: r, output: lines.join('') }));
  }

  test('lists every matched file and prints the summary footer', async () => {
    const a = touch('a.html');
    const b = touch('sub/b.htm');
    touch('skip.subset.html');
    const { result, output } = await withCapturedStdout(() => runBatch({
      inputDir: dir,
      pagxBin: '/no',
      scriptDir: dir,
      dryRun: true,
    }));
    expect(result).toEqual({ processed: 2, skipped: 0, failed: 0, total: 2 });
    expect(output).toMatch(/Found 2 non-subset HTML files under:/);
    expect(output).toMatch(/\[1\/2\] would process /);
    expect(output).toMatch(/\[2\/2\] would process /);
    expect(output).toMatch(/Summary: processed=2 skipped=0 failed=0 total=2/);
    // Sanity: both inputs are referenced relative to inputDir.
    expect(output).toContain(path.relative(dir, a));
    expect(output).toContain(path.relative(dir, b));
  });

  test('skipExisting marks pre-existing .pagx targets as skipped', async () => {
    touch('a.html');
    touch('b.html');
    // Pre-stage `a.pagx` next to its sibling so the skipExisting check fires.
    fs.writeFileSync(path.join(dir, 'a.pagx'), '');
    const { result, output } = await withCapturedStdout(() => runBatch({
      inputDir: dir,
      pagxBin: '/no',
      scriptDir: dir,
      dryRun: true,
      skipExisting: true,
    }));
    expect(result).toEqual({ processed: 1, skipped: 1, failed: 0, total: 2 });
    expect(output).toMatch(/\[1\/2\] skip      a\.html/);
    expect(output).toMatch(/\[2\/2\] would process b\.html/);
  });

  test('outputRoot mirrors the input tree', async () => {
    touch('top.html');
    touch('nested/leaf.html');
    const outRoot = path.join(dir, 'out');
    const { result } = await withCapturedStdout(() => runBatch({
      inputDir: dir,
      outputRoot: outRoot,
      pagxBin: '/no',
      scriptDir: dir,
      dryRun: true,
    }));
    expect(result.processed).toBe(2);
    // The directory was created by runBatch even though dry-run never wrote
    // anything underneath it.
    expect(fs.existsSync(outRoot)).toBe(true);
  });
});
