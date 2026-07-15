'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { runBatch, collectHtmlInputs } = require('../dist/lib/batch');

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

// Live-run tests for runBatch. Cover the path that actually opens a browser
// (mocked) and feeds each HTML through `runHtmlToPagx`. Exercising this needs
// browser-engine.launchBrowser to return a stub handle and pipeline.runHtmlToPagx
// to be replaced with a counting fake — we use jest.doMock so the real modules
// are still loaded by every other test file.
describe('runBatch — live execution (mocked engine + pipeline)', () => {
  function withCapturedIO(fn) {
    const realOut = process.stdout.write.bind(process.stdout);
    const realErr = process.stderr.write.bind(process.stderr);
    const out = [];
    const err = [];
    process.stdout.write = (s) => { out.push(String(s)); return true; };
    process.stderr.write = (s) => { err.push(String(s)); return true; };
    return Promise.resolve(fn())
      .finally(() => {
        process.stdout.write = realOut;
        process.stderr.write = realErr;
      })
      .then((r) => ({ result: r, stdout: out.join(''), stderr: err.join('') }));
  }

  // Reload `dist/lib/batch` after mocking its two collaborators so tests in
  // this `describe` can swap behaviour per case without polluting the cache
  // for other suites.
  function loadBatchWith({ runHtmlToPagx, launchBrowser, browserClose }) {
    jest.resetModules();
    jest.doMock('../dist/lib/browser-engine', () => {
      const real = jest.requireActual('../dist/lib/browser-engine');
      return {
        ...real,
        launchBrowser: launchBrowser || (async () => ({
          browser: { close: browserClose || (async () => {}) },
          engine: 'puppeteer',
        })),
      };
    });
    jest.doMock('../dist/lib/pipeline', () => {
      const real = jest.requireActual('../dist/lib/pipeline');
      return {
        ...real,
        runHtmlToPagx: runHtmlToPagx || (async () => ({ subsetHtml: '', pagx: '', png: null, fonts: [] })),
      };
    });
    return require('../dist/lib/batch');
  }

  afterEach(() => {
    jest.dontMock('../dist/lib/browser-engine');
    jest.dontMock('../dist/lib/pipeline');
    jest.resetModules();
  });

  test('processes each html, closes the browser, and reports the totals', async () => {
    touch('a.html');
    touch('sub/b.htm');
    touch('skip.subset.html');
    let closeCalls = 0;
    const seen = [];
    const { runBatch: liveRun } = loadBatchWith({
      runHtmlToPagx: async (opts) => {
        seen.push(opts.input);
        return { subsetHtml: '', pagx: opts.outputDir + '/' + opts.outputName + '.pagx', png: null, fonts: [] };
      },
      browserClose: async () => { closeCalls++; },
    });
    const { result, stdout } = await withCapturedIO(() => liveRun({
      inputDir: dir,
      pagxBin: '/p',
      scriptDir: dir,
    }));
    expect(result).toEqual({ processed: 2, skipped: 0, failed: 0, total: 2 });
    expect(stdout).toMatch(/\[1\/2\] process /);
    expect(stdout).toMatch(/\[2\/2\] process /);
    expect(stdout).toMatch(/Summary: processed=2 skipped=0 failed=0 total=2/);
    expect(closeCalls).toBe(1);
    expect(seen).toHaveLength(2);
  });

  test('skipExisting marks pre-existing .pagx targets without invoking the pipeline', async () => {
    touch('a.html');
    touch('b.html');
    fs.writeFileSync(path.join(dir, 'a.pagx'), '');
    let pipelineCalls = 0;
    const { runBatch: liveRun } = loadBatchWith({
      runHtmlToPagx: async () => { pipelineCalls++; return { subsetHtml: '', pagx: '', png: null, fonts: [] }; },
    });
    const { result, stdout } = await withCapturedIO(() => liveRun({
      inputDir: dir,
      pagxBin: '/p',
      scriptDir: dir,
      skipExisting: true,
    }));
    expect(result).toEqual({ processed: 1, skipped: 1, failed: 0, total: 2 });
    expect(stdout).toMatch(/\[1\/2\] skip      a\.html/);
    expect(pipelineCalls).toBe(1);
  });

  test('PipelineStepError surfaces with the step label in stderr', async () => {
    touch('a.html');
    const { PipelineStepError } = require('../dist/lib/pipeline');
    const { runBatch: liveRun } = loadBatchWith({
      runHtmlToPagx: async () => {
        throw new PipelineStepError('snapshot failed', { step: 'snapshot', code: 1, stderr: 'boom' });
      },
    });
    const { result, stderr } = await withCapturedIO(() => liveRun({
      inputDir: dir, pagxBin: '/p', scriptDir: dir,
    }));
    expect(result.failed).toBe(1);
    expect(result.processed).toBe(0);
    expect(stderr).toMatch(/failed: a\.html: snapshot failed/);
  });

  test('non-pipeline errors fall through with their message', async () => {
    touch('a.html');
    const { runBatch: liveRun } = loadBatchWith({
      runHtmlToPagx: async () => { throw new Error('weird non-pipeline'); },
    });
    const { result, stderr } = await withCapturedIO(() => liveRun({
      inputDir: dir, pagxBin: '/p', scriptDir: dir,
    }));
    expect(result.failed).toBe(1);
    expect(stderr).toMatch(/failed: a\.html: weird non-pipeline/);
  });

  test('logs a browser-close error without changing the result', async () => {
    touch('a.html');
    const { runBatch: liveRun } = loadBatchWith({
      runHtmlToPagx: async () => ({ subsetHtml: '', pagx: '', png: null, fonts: [] }),
      browserClose: async () => { throw new Error('close exploded'); },
    });
    const { result, stderr } = await withCapturedIO(() => liveRun({
      inputDir: dir, pagxBin: '/p', scriptDir: dir,
    }));
    expect(result.processed).toBe(1);
    expect(stderr).toMatch(/browser close error: close exploded/);
  });
});

// The in-process snapshot adapter (`makeInProcessSnapshot`) is private — it is
// only reachable as `opts.snapshotImpl` inside `runHtmlToPagx`. We mock
// `runHtmlToPagx` to invoke that adapter (and the per-file `log` callback)
// directly, and mock `snapshot-runner.runSnapshot` so no real browser work
// happens. This drives the file:// / http URL building, cookie/header parsing,
// snapshot write, and the success / failure return shapes.
describe('runBatch — in-process snapshot adapter', () => {
  function withCapturedIO(fn) {
    const realOut = process.stdout.write.bind(process.stdout);
    const realErr = process.stderr.write.bind(process.stderr);
    const out = [];
    const err = [];
    process.stdout.write = (s) => { out.push(String(s)); return true; };
    process.stderr.write = (s) => { err.push(String(s)); return true; };
    return Promise.resolve(fn())
      .finally(() => {
        process.stdout.write = realOut;
        process.stderr.write = realErr;
      })
      .then((r) => ({ result: r, stdout: out.join(''), stderr: err.join('') }));
  }

  function loadBatchWithRunner({ runSnapshot, runHtmlToPagx }) {
    jest.resetModules();
    jest.doMock('../dist/lib/browser-engine', () => {
      const real = jest.requireActual('../dist/lib/browser-engine');
      return {
        ...real,
        launchBrowser: async () => ({ browser: { close: async () => {} }, engine: 'puppeteer' }),
      };
    });
    jest.doMock('../dist/lib/snapshot-runner', () => {
      const real = jest.requireActual('../dist/lib/snapshot-runner');
      return { ...real, runSnapshot };
    });
    jest.doMock('../dist/lib/pipeline', () => {
      const real = jest.requireActual('../dist/lib/pipeline');
      return { ...real, runHtmlToPagx };
    });
    return require('../dist/lib/batch');
  }

  afterEach(() => {
    jest.dontMock('../dist/lib/browser-engine');
    jest.dontMock('../dist/lib/snapshot-runner');
    jest.dontMock('../dist/lib/pipeline');
    jest.resetModules();
  });

  test('builds a file:// URL, parses cookies/headers, writes the snapshot, and reports code 0', async () => {
    touch('page.html');
    const outFile = path.join(dir, 'page.subset.html');
    let snapshotResult = null;
    let seenUrl = null;
    let seenOpts = null;
    const { runBatch: liveRun } = loadBatchWithRunner({
      runSnapshot: async (engineHandle, url, opts) => {
        seenUrl = url;
        seenOpts = opts;
        opts.log('snapshot progress note');
        return { html: '<flat/>', width: 100, height: 50, fonts: [], images: [] };
      },
      runHtmlToPagx: async (opts) => {
        snapshotResult = await opts.snapshotImpl({
          input: path.join(dir, 'page.html'),
          output: outFile,
          viewportWidth: 800,
          viewportHeight: 600,
          waitMs: 10,
          selector: '#root',
          cookies: ['session=abc', 'bare'],
          headers: ['X-Test: yes', 'NoColonHeader'],
          inlineIconFonts: true,
          downloadFonts: false,
          downloadImages: false,
        });
        opts.log('pipeline line');
        return { subsetHtml: outFile, pagx: '', png: null, fonts: [] };
      },
    });

    const { result, stdout, stderr } = await withCapturedIO(() => liveRun({
      inputDir: dir, pagxBin: '/p', scriptDir: dir,
    }));

    expect(result.processed).toBe(1);
    expect(snapshotResult.code).toBe(0);
    expect(typeof snapshotResult.durationMs).toBe('number');
    // file:// URL was synthesised from the local input path.
    expect(seenUrl.startsWith('file://')).toBe(true);
    expect(seenUrl.endsWith('page.html')).toBe(true);
    // Cookies / headers were split into name/value pairs (bare entries → empty value).
    expect(seenOpts.cookies).toEqual([
      { name: 'session', value: 'abc' },
      { name: 'bare', value: '' },
    ]);
    expect(seenOpts.headers).toEqual([
      ['X-Test', 'yes'],
      ['NoColonHeader', ''],
    ]);
    expect(seenOpts.inlineIconFonts).toBe(true);
    // The flattened HTML was written to the requested output path.
    expect(fs.readFileSync(outFile, 'utf8')).toBe('<flat/>');
    // The adapter forwarded runSnapshot's log line to stderr.
    expect(stderr).toMatch(/snapshot progress note/);
    // The pipeline's own log callback writes to stdout.
    expect(stdout).toMatch(/pipeline line/);
  });

  test('passes an http(s) input through untouched as the target URL', async () => {
    touch('ignored.html');
    let seenUrl = null;
    const { runBatch: liveRun } = loadBatchWithRunner({
      runSnapshot: async (engineHandle, url) => {
        seenUrl = url;
        return { html: '<x/>', width: 1, height: 1, fonts: [], images: [] };
      },
      runHtmlToPagx: async (opts) => {
        await opts.snapshotImpl({
          input: 'https://example.com/live',
          output: path.join(dir, 'http.subset.html'),
        });
        return { subsetHtml: '', pagx: '', png: null, fonts: [] };
      },
    });
    await withCapturedIO(() => liveRun({ inputDir: dir, pagxBin: '/p', scriptDir: dir }));
    expect(seenUrl).toBe('https://example.com/live');
  });

  test('returns code 1 with the error text when runSnapshot throws', async () => {
    touch('page.html');
    let snapshotResult = null;
    const { runBatch: liveRun } = loadBatchWithRunner({
      runSnapshot: async () => { throw new Error('navigation timed out'); },
      runHtmlToPagx: async (opts) => {
        snapshotResult = await opts.snapshotImpl({
          input: path.join(dir, 'page.html'),
          output: path.join(dir, 'page.subset.html'),
        });
        // Surface the adapter failure the way the real pipeline would.
        if (snapshotResult.code !== 0) {
          throw new Error('snapshot exited with code ' + snapshotResult.code);
        }
        return { subsetHtml: '', pagx: '', png: null, fonts: [] };
      },
    });
    const { result } = await withCapturedIO(() => liveRun({ inputDir: dir, pagxBin: '/p', scriptDir: dir }));
    expect(snapshotResult.code).toBe(1);
    expect(snapshotResult.stderr).toMatch(/navigation timed out/);
    expect(result.failed).toBe(1);
  });
});
