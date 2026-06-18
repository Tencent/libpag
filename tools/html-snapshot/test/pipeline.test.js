'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  PipelineStepError,
  spawnCapture,
  forkSnapshotCli,
  runPagxImportToFile,
  runPagxResolve,
  runPagxFontEmbed,
  runPagxRender,
  collectFontFallbacks,
  readFontManifest,
  runHtmlToPagx,
  isHttpUrl,
} = require('../dist/lib/pipeline');
const { ChildProcessError } = require('../dist/lib/errors');

let dir;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'html-snapshot-pipeline-test-'));
});
afterEach(() => {
  fs.rmSync(dir, { recursive: true, force: true });
});

// Build a tiny shell wrapper that records its argv and exits with the
// requested status. Returns the absolute path to the wrapper, with execute
// bit set. The wrapper writes its argv (NUL-separated) to `argvPath` so
// tests can assert exactly which flags were forwarded.
//
// `stdout` / `stderr` accept either a single string (emitted verbatim with
// `printf '%s'`) or an array of strings (joined with newlines so `\n` works
// without relying on shell-specific escape handling).
function makeArgvCapturingScript({ exitCode = 0, stdout = '', stderr = '', argvPath, sleepMs }) {
  const wrapper = path.join(dir, `wrap-${Math.random().toString(36).slice(2)}.sh`);
  const lines = ['#!/bin/sh'];
  if (argvPath) {
    // Redirect a printf'd argv list into the capture file. Each arg is
    // followed by a NUL byte so empty strings round-trip cleanly.
    lines.push(`printf '%s\\0' "$@" > ${JSON.stringify(argvPath)}`);
  }
  const emit = (payload, redirect) => {
    if (!payload) return;
    if (Array.isArray(payload)) {
      // One `printf '%s\n'` per line keeps shell escape rules simple.
      for (const ln of payload) lines.push(`printf '%s\\n' ${JSON.stringify(ln)}${redirect}`);
    } else {
      lines.push(`printf '%s' ${JSON.stringify(payload)}${redirect}`);
    }
  };
  emit(stdout, '');
  emit(stderr, ' 1>&2');
  if (sleepMs) lines.push(`sleep ${(sleepMs / 1000).toFixed(3)}`);
  lines.push(`exit ${exitCode}`);
  fs.writeFileSync(wrapper, lines.join('\n'));
  fs.chmodSync(wrapper, 0o755);
  return wrapper;
}

function readArgv(argvPath) {
  if (!fs.existsSync(argvPath)) return [];
  const buf = fs.readFileSync(argvPath, 'utf8');
  // The trailing NUL after the last arg yields an empty final segment, drop it.
  const parts = buf.split('\0');
  if (parts.length && parts[parts.length - 1] === '') parts.pop();
  return parts;
}

describe('PipelineStepError', () => {
  test('is a ChildProcessError subclass with a step label', () => {
    const err = new PipelineStepError('snapshot failed', {
      code: 1,
      stderr: 'boom',
      step: 'snapshot',
    });
    expect(err).toBeInstanceOf(ChildProcessError);
    expect(err.name).toBe('PipelineStepError');
    expect(err.step).toBe('snapshot');
    expect(err.code).toBe(1);
    expect(err.stderr).toBe('boom');
  });

  test('defaults step to empty when omitted', () => {
    const err = new PipelineStepError('x');
    expect(err.step).toBe('');
  });
});

describe('spawnCapture', () => {
  test('captures stdout, stderr, and a zero exit code', async () => {
    const cmd = makeArgvCapturingScript({
      stdout: 'hello',
      stderr: 'warn',
    });
    const r = await spawnCapture(cmd, []);
    expect(r.code).toBe(0);
    expect(r.stdout).toBe('hello');
    expect(r.stderr).toBe('warn');
    expect(r.durationMs).toBeGreaterThanOrEqual(0);
  });

  test('reports a non-zero exit code without throwing', async () => {
    const cmd = makeArgvCapturingScript({ exitCode: 3, stderr: 'fail' });
    const r = await spawnCapture(cmd, []);
    expect(r.code).toBe(3);
    expect(r.stderr).toContain('fail');
  });

  test('writes captured stderr to stderrPath when supplied', async () => {
    const stderrPath = path.join(dir, 'cap.stderr.txt');
    const cmd = makeArgvCapturingScript({ stderr: 'on-disk' });
    await spawnCapture(cmd, [], { stderrPath });
    expect(fs.readFileSync(stderrPath, 'utf8')).toBe('on-disk');
  });

  test('reports a non-zero exit when the command does not exist', async () => {
    // Node's child_process emits an asynchronous `error` event for ENOENT
    // and the subsequent `close` resolves with a negative errno code on
    // most platforms. The contract spawnCapture promises is "never reject,
    // never zero" — any non-zero (including null / -2) signals failure to
    // the caller, plus the spawn error message lands on stderr.
    const r = await spawnCapture(path.join(dir, 'nope-not-real'), []);
    expect(r.code).not.toBe(0);
    expect(r.stderr.length).toBeGreaterThan(0);
  });

  test('kills a long-running child after timeoutMs and tags stderr', async () => {
    const cmd = makeArgvCapturingScript({ sleepMs: 5000 });
    const r = await spawnCapture(cmd, [], { timeoutMs: 50 });
    expect(r.stderr).toMatch(/killed by timeout \(50ms\)/);
    // SIGKILLed children typically resolve to code: null on Node.
    expect(r.code === null || typeof r.code === 'number').toBe(true);
  });
});

describe('forkSnapshotCli — argv builder', () => {
  test('rejects when the required fields are missing', async () => {
    await expect(forkSnapshotCli({})).rejects.toThrow(/input is required/);
    await expect(forkSnapshotCli({ input: 'a' })).rejects.toThrow(/output is required/);
    await expect(forkSnapshotCli({ input: 'a', output: 'b' }))
      .rejects.toThrow(/scriptDir is required/);
  });

  test('forwards every snapshot-side flag in the documented shape', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const fakeNode = makeArgvCapturingScript({ argvPath });
    const r = await forkSnapshotCli({
      input: '/page.html',
      output: '/out.subset.html',
      scriptDir: '/script',
      browserEngine: 'playwright',
      inlineIconFonts: false,
      viewportWidth: 1280,
      viewportHeight: 720,
      waitMs: 500,
      selector: '#root',
      cookies: ['k=v'],
      headers: ['X-Y: Z'],
      downloadFonts: true,
      fontDir: '/fonts',
      fontManifest: '/manifest.txt',
      downloadImages: true,
      imageDir: '/imgs',
      extraArgs: ['--debug'],
      nodeBin: fakeNode,
    });
    expect(r.code).toBe(0);
    const argv = readArgv(argvPath);
    // First arg is the script path the fork would invoke.
    expect(argv[0]).toBe(path.join('/script', 'snapshot.js'));
    expect(argv).toContain('/page.html');
    expect(argv).toContain('-o');
    expect(argv).toContain('/out.subset.html');
    expect(argv).toEqual(expect.arrayContaining([
      '--browser-engine', 'playwright',
      '--no-inline-icon-fonts',
      '--viewport-width', '1280',
      '--viewport-height', '720',
      '--wait-ms', '500',
      '--selector', '#root',
      '--cookie', 'k=v',
      '--header', 'X-Y: Z',
      '--download-fonts',
      '--font-dir', '/fonts',
      '--font-manifest', '/manifest.txt',
      '--download-images',
      '--image-dir', '/imgs',
      '--debug',
    ]));
  });

  test('omits download flags by default', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const fakeNode = makeArgvCapturingScript({ argvPath });
    await forkSnapshotCli({
      input: '/p',
      output: '/o',
      scriptDir: '/s',
      nodeBin: fakeNode,
    });
    const argv = readArgv(argvPath);
    expect(argv).not.toContain('--download-fonts');
    expect(argv).not.toContain('--download-images');
    expect(argv).not.toContain('--no-inline-icon-fonts');
  });
});

describe('runPagxImportToFile', () => {
  test('rejects when required fields are missing', async () => {
    await expect(runPagxImportToFile({})).rejects.toThrow(/pagxBin is required/);
    await expect(runPagxImportToFile({ pagxBin: '/p' }))
      .rejects.toThrow(/subsetHtml is required/);
    await expect(runPagxImportToFile({ pagxBin: '/p', subsetHtml: 'h' }))
      .rejects.toThrow(/pagxFile is required/);
  });

  test('forwards --html-infer-flex by default and routes filtered stderr to log', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({
      argvPath,
      stderr: [
        'warning: html: position: absolute on text leaf',
        'real warning: keep me',
      ],
    });
    const logs = [];
    const r = await runPagxImportToFile({
      pagxBin: wrapper,
      subsetHtml: '/in.html',
      pagxFile: '/out.pagx',
      log: (m) => logs.push(m),
    });
    expect(r.code).toBe(0);
    const argv = readArgv(argvPath);
    expect(argv).toEqual([
      'import',
      '--input', '/in.html',
      '--output', '/out.pagx',
      '--format', 'html',
      '--html-infer-flex',
    ]);
    const joined = logs.join('\n');
    expect(joined).toMatch(/real warning: keep me/);
    expect(joined).not.toMatch(/position: absolute on text leaf/);
  });

  test('omits --html-infer-flex when inferFlex=false', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({ argvPath });
    await runPagxImportToFile({
      pagxBin: wrapper,
      subsetHtml: '/in',
      pagxFile: '/out',
      inferFlex: false,
    });
    expect(readArgv(argvPath)).not.toContain('--html-infer-flex');
  });
});

describe('runPagxResolve / runPagxFontEmbed / runPagxRender', () => {
  test('runPagxResolve forwards [resolve <pagx>]', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({ argvPath });
    await runPagxResolve({ pagxBin: wrapper, pagxFile: '/x.pagx' });
    expect(readArgv(argvPath)).toEqual(['resolve', '/x.pagx']);
  });

  test('runPagxResolve rejects on missing args', async () => {
    await expect(runPagxResolve({})).rejects.toThrow(/pagxBin is required/);
    await expect(runPagxResolve({ pagxBin: '/p' }))
      .rejects.toThrow(/pagxFile is required/);
  });

  test('runPagxFontEmbed appends each fallback path', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({ argvPath });
    await runPagxFontEmbed({
      pagxBin: wrapper,
      pagxFile: '/x.pagx',
      fontFiles: ['/a.ttf', '/b.otf'],
    });
    expect(readArgv(argvPath)).toEqual([
      'font', 'embed', '/x.pagx',
      '--fallback', '/a.ttf',
      '--fallback', '/b.otf',
    ]);
  });

  test('runPagxFontEmbed accepts an empty fontFiles list', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({ argvPath });
    await runPagxFontEmbed({ pagxBin: wrapper, pagxFile: '/x.pagx' });
    expect(readArgv(argvPath)).toEqual(['font', 'embed', '/x.pagx']);
  });

  test('runPagxRender forwards scale and fallback args', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({ argvPath });
    await runPagxRender({
      pagxBin: wrapper,
      pagxFile: '/x.pagx',
      output: '/out.png',
      scale: 2,
      fontFallbacks: ['/a.ttf'],
    });
    expect(readArgv(argvPath)).toEqual([
      'render', '/x.pagx',
      '--output', '/out.png',
      '--scale', '2',
      '--fallback', '/a.ttf',
    ]);
  });

  test('runPagxRender defaults scale to 1', async () => {
    const argvPath = path.join(dir, 'argv.bin');
    const wrapper = makeArgvCapturingScript({ argvPath });
    await runPagxRender({
      pagxBin: wrapper,
      pagxFile: '/x.pagx',
      output: '/y.png',
    });
    expect(readArgv(argvPath)).toEqual([
      'render', '/x.pagx',
      '--output', '/y.png',
      '--scale', '1',
    ]);
  });

  test('runPagxRender rejects when output is missing', async () => {
    await expect(runPagxRender({ pagxBin: '/p', pagxFile: '/x' }))
      .rejects.toThrow(/output is required/);
  });
});

describe('collectFontFallbacks', () => {
  test('returns [] when no fontDir is provided', () => {
    expect(collectFontFallbacks({})).toEqual([]);
  });

  test('returns [] for a non-existent directory', () => {
    expect(collectFontFallbacks({ fontDir: path.join(dir, 'missing') })).toEqual([]);
  });

  test('returns absolute SFNT paths sorted alphabetically', () => {
    fs.writeFileSync(path.join(dir, 'b.ttf'), '');
    fs.writeFileSync(path.join(dir, 'a.OTF'), '');
    fs.writeFileSync(path.join(dir, 'c.ttc'), '');
    fs.writeFileSync(path.join(dir, 'note.txt'), '');
    fs.writeFileSync(path.join(dir, 'icon.svg'), '');
    const fonts = collectFontFallbacks({ fontDir: dir });
    expect(fonts.map((p) => path.basename(p))).toEqual(['a.OTF', 'b.ttf', 'c.ttc']);
    fonts.forEach((p) => expect(path.isAbsolute(p)).toBe(true));
  });
});

describe('readFontManifest', () => {
  test('returns [] when manifestPath is missing', () => {
    expect(readFontManifest({})).toEqual([]);
  });

  test('returns [] when the manifest file does not exist', () => {
    expect(readFontManifest({ manifestPath: path.join(dir, 'no.txt') })).toEqual([]);
  });

  test('skips blank lines and non-existent paths', () => {
    const real = path.join(dir, 'a.ttf');
    fs.writeFileSync(real, '');
    const ghost = path.join(dir, 'ghost.ttf');
    const manifest = path.join(dir, 'manifest.txt');
    fs.writeFileSync(manifest, [real, '', '   ', ghost, real].join('\n'));
    const out = readFontManifest({ manifestPath: manifest });
    expect(out).toEqual([real, real]);
  });
});

describe('isHttpUrl re-export', () => {
  test('matches the lib/common implementation', () => {
    expect(isHttpUrl('https://x')).toBe(true);
    expect(isHttpUrl('file:///p')).toBe(false);
  });
});

describe('runHtmlToPagx — orchestrator', () => {
  test('rejects when scriptDir is missing', async () => {
    await expect(runHtmlToPagx({ input: '/foo.html' }))
      .rejects.toThrow(/scriptDir is required/);
  });

  test('rejects when input is missing for a file path', async () => {
    await expect(runHtmlToPagx({ scriptDir: '/s' }))
      .rejects.toThrow(/input is required/);
  });

  test('rejects when the input file does not exist', async () => {
    await expect(runHtmlToPagx({ scriptDir: '/s', input: path.join(dir, 'no.html') }))
      .rejects.toThrow(/input not found/);
  });

  test('rejects URL input without an outputName', async () => {
    await expect(runHtmlToPagx({
      scriptDir: '/s',
      input: 'https://example.com/page',
    })).rejects.toThrow(/outputName is required/);
  });

  test('runs snapshot → import → resolve → render against fakes', async () => {
    // Stage a placeholder input so the existence check passes.
    const inputHtml = path.join(dir, 'page.html');
    fs.writeFileSync(inputHtml, '<html></html>');

    // Fake pagx wrapper that interprets the first arg as a subcommand and
    // writes a placeholder file to the path following --output (or to the
    // first positional argument for `resolve`).
    const fakePagx = path.join(dir, 'fakepagx.sh');
    fs.writeFileSync(fakePagx, [
      '#!/bin/sh',
      'cmd="$1"; shift',
      'out=""',
      'while [ "$#" -gt 0 ]; do',
      '  case "$1" in',
      '    --output) out="$2"; shift;;',
      '  esac',
      '  shift',
      'done',
      'case "$cmd" in',
      // import writes the .pagx file to --output
      '  import) echo -n "<pag/>" > "$out" ;;',
      // resolve takes the pagx path positionally; touch it to record the call
      '  resolve) : ;;',
      // render writes a "PNG" to --output (file content shape doesn\'t matter)
      '  render) echo -n "PNG" > "$out" ;;',
      'esac',
    ].join('\n'));
    fs.chmodSync(fakePagx, 0o755);

    // Inject an in-process snapshot stub instead of forking node snapshot.js;
    // this writes the subset HTML the import step expects.
    const snapshotImpl = async (snapshotArgs) => {
      fs.writeFileSync(snapshotArgs.output, '<html><body>flat</body></html>', 'utf8');
      return { code: 0, durationMs: 1, stdout: '', stderr: '' };
    };

    const outDir = path.join(dir, 'out');
    const result = await runHtmlToPagx({
      input: inputHtml,
      outputDir: outDir,
      pagxBin: fakePagx,
      scriptDir: dir,
      snapshotImpl,
      log: () => {},
    });

    expect(fs.existsSync(result.subsetHtml)).toBe(true);
    expect(fs.existsSync(result.pagx)).toBe(true);
    expect(fs.existsSync(result.png)).toBe(true);
    expect(path.basename(result.pagx)).toBe('page.pagx');
    expect(path.basename(result.png)).toBe('page.png');
  });

  test('throws PipelineStepError when the snapshot step fails', async () => {
    const inputHtml = path.join(dir, 'page.html');
    fs.writeFileSync(inputHtml, '<html></html>');
    const failingSnapshot = async () => ({
      code: 1, durationMs: 1, stdout: '', stderr: 'snapshot crashed',
    });
    // stderr path hijack: avoid spamming jest's console with the error text.
    const realStderrWrite = process.stderr.write.bind(process.stderr);
    process.stderr.write = () => true;
    try {
      await expect(runHtmlToPagx({
        input: inputHtml,
        outputDir: path.join(dir, 'out'),
        pagxBin: '/no-pagx',
        scriptDir: dir,
        snapshotImpl: failingSnapshot,
      })).rejects.toMatchObject({
        name: 'PipelineStepError',
        step: 'snapshot',
        code: 1,
      });
    } finally {
      process.stderr.write = realStderrWrite;
    }
  });

  test('skips resolve+render when doResolve=false', async () => {
    const inputHtml = path.join(dir, 'page.html');
    fs.writeFileSync(inputHtml, '<html></html>');
    const fakePagx = path.join(dir, 'fakepagx2.sh');
    fs.writeFileSync(fakePagx, [
      '#!/bin/sh',
      'cmd="$1"; shift',
      'out=""',
      'while [ "$#" -gt 0 ]; do',
      '  case "$1" in --output) out="$2"; shift;; esac',
      '  shift',
      'done',
      'if [ "$cmd" = "import" ]; then echo -n "<pag/>" > "$out"; fi',
      // For `resolve`/`render` we deliberately exit non-zero so a regression
      // that re-enables those steps would surface immediately.
      'if [ "$cmd" != "import" ]; then exit 99; fi',
    ].join('\n'));
    fs.chmodSync(fakePagx, 0o755);

    const snapshotImpl = async (args) => {
      fs.writeFileSync(args.output, '<html></html>', 'utf8');
      return { code: 0, durationMs: 1, stdout: '', stderr: '' };
    };

    const result = await runHtmlToPagx({
      input: inputHtml,
      outputDir: path.join(dir, 'out2'),
      pagxBin: fakePagx,
      scriptDir: dir,
      doResolve: false,
      snapshotImpl,
      log: () => {},
    });
    expect(result.png).toBeNull();
    expect(fs.existsSync(result.pagx)).toBe(true);
  });

  test('skips render when doRender=false but resolve still runs', async () => {
    const inputHtml = path.join(dir, 'p.html');
    fs.writeFileSync(inputHtml, '<html></html>');
    const fakePagx = makeFakePagx({ allowRender: false });
    let resolveCalls = 0;
    // Parse argv on every invocation to count the resolve runs.
    const argvLog = path.join(dir, 'pagx.argv');
    const wrapper = makeArgvCapturingScript({ argvPath: argvLog });
    void wrapper;
    const snapshotImpl = async (a) => { fs.writeFileSync(a.output, '<html/>'); return { code: 0, durationMs: 1, stdout: '', stderr: '' }; };
    const result = await runHtmlToPagx({
      input: inputHtml,
      outputDir: path.join(dir, 'r-out'),
      pagxBin: fakePagx.path,
      scriptDir: dir,
      doRender: false,
      snapshotImpl,
      log: () => {},
    });
    expect(result.png).toBeNull();
    expect(fs.existsSync(result.pagx)).toBe(true);
    resolveCalls = fakePagx.callsByCmd.resolve;
    expect(resolveCalls).toBeGreaterThanOrEqual(1);
  });

  test('throws when pagx import succeeds but the .pagx file is not on disk', async () => {
    const inputHtml = path.join(dir, 'q.html');
    fs.writeFileSync(inputHtml, '<html></html>');
    // Wrapper that returns 0 from `import` without writing the output file.
    const wrapper = path.join(dir, 'noimport.sh');
    fs.writeFileSync(wrapper, '#!/bin/sh\nexit 0\n');
    fs.chmodSync(wrapper, 0o755);
    const snapshotImpl = async (a) => { fs.writeFileSync(a.output, '<html/>'); return { code: 0, durationMs: 1, stdout: '', stderr: '' }; };
    const realErr = process.stderr.write.bind(process.stderr);
    process.stderr.write = () => true;
    try {
      await expect(runHtmlToPagx({
        input: inputHtml,
        outputDir: path.join(dir, 'q-out'),
        pagxBin: wrapper,
        scriptDir: dir,
        snapshotImpl,
        log: () => {},
      })).rejects.toMatchObject({ name: 'PipelineStepError', step: 'pagx import' });
    } finally {
      process.stderr.write = realErr;
    }
  });

  test('downloadFonts logs when no fonts were captured and runs font embed when faces are present', async () => {
    const inputHtml = path.join(dir, 'fpage.html');
    fs.writeFileSync(inputHtml, '<html></html>');
    const fakePagx = makeFakePagx();

    // Case A: empty font dir → "no web fonts were captured" log.
    let logsA = [];
    const snapImplA = async (a) => {
      // The pipeline pre-creates the font dir via the snapshot child; mimic
      // that here so collectFontFallbacks finds the directory but no entries.
      fs.mkdirSync(a.fontDir, { recursive: true });
      fs.writeFileSync(a.output, '<html/>');
      return { code: 0, durationMs: 1, stdout: '', stderr: '' };
    };
    await runHtmlToPagx({
      input: inputHtml,
      outputDir: path.join(dir, 'fa-out'),
      pagxBin: fakePagx.path,
      scriptDir: dir,
      downloadFonts: true,
      doResolve: false,
      snapshotImpl: snapImplA,
      log: (l) => logsA.push(l),
    });
    expect(logsA.some((l) => /no web fonts were captured/.test(l))).toBe(true);

    // Case B: embedFonts=true with a real font dir → font-embed step runs.
    fakePagx.callsByCmd.resolve = 0;
    fakePagx.callsByCmd.font = 0;
    const snapImplB = async (a) => {
      fs.mkdirSync(a.fontDir, { recursive: true });
      fs.writeFileSync(path.join(a.fontDir, 'Real.ttf'), 'fake');
      fs.writeFileSync(a.output, '<html/>');
      return { code: 0, durationMs: 1, stdout: '', stderr: '' };
    };
    const result = await runHtmlToPagx({
      input: inputHtml,
      outputDir: path.join(dir, 'fb-out'),
      pagxBin: fakePagx.path,
      scriptDir: dir,
      embedFonts: true,
      doRender: false,
      snapshotImpl: snapImplB,
      log: () => {},
    });
    expect(result.fonts).toHaveLength(1);
    expect(fakePagx.callsByCmd.font).toBe(1);
  });

  test('keepSubsetHtml=false writes the subset to a temp dir', async () => {
    const inputHtml = path.join(dir, 't.html');
    fs.writeFileSync(inputHtml, '<html></html>');
    const fakePagx = makeFakePagx();
    const snapshotImpl = async (a) => { fs.writeFileSync(a.output, '<html/>'); return { code: 0, durationMs: 1, stdout: '', stderr: '' }; };
    const outDir = path.join(dir, 't-out');
    const result = await runHtmlToPagx({
      input: inputHtml,
      outputDir: outDir,
      pagxBin: fakePagx.path,
      scriptDir: dir,
      keepSubsetHtml: false,
      doResolve: false,
      snapshotImpl,
      log: () => {},
    });
    // The subset path leaves `outDir` (it lands in os.tmpdir()) and is rm'd at
    // the end of the run, so it must NOT exist on disk after we return.
    expect(result.subsetHtml).not.toContain(outDir);
    expect(fs.existsSync(result.subsetHtml)).toBe(false);
    // The pagx file itself stays put under the requested outputDir.
    expect(fs.existsSync(result.pagx)).toBe(true);
  });

  test('URL inputs default outputDir to cwd and require outputName', async () => {
    const fakePagx = makeFakePagx();
    const cwd = process.cwd();
    const tempCwd = fs.mkdtempSync(path.join(os.tmpdir(), 'pipeline-cwd-'));
    process.chdir(tempCwd);
    try {
      const snapshotImpl = async (a) => { fs.writeFileSync(a.output, '<html/>'); return { code: 0, durationMs: 1, stdout: '', stderr: '' }; };
      const result = await runHtmlToPagx({
        input: 'https://example.com/p',
        outputName: 'remote',
        pagxBin: fakePagx.path,
        scriptDir: dir,
        doResolve: false,
        snapshotImpl,
        log: () => {},
      });
      expect(path.dirname(result.pagx)).toBe(fs.realpathSync(tempCwd));
      expect(path.basename(result.pagx)).toBe('remote.pagx');
    } finally {
      process.chdir(cwd);
      fs.rmSync(tempCwd, { recursive: true, force: true });
    }
  });
});

// ----- helpers used by the runHtmlToPagx tests above -----

// Build a single fake-pagx wrapper that handles every subcommand the pipeline
// invokes (import, resolve, font, render). Tracks per-command call counts so
// tests can assert which steps actually ran. `allowRender: false` makes the
// `render` subcommand exit non-zero — useful for asserting that doRender=false
// really skips the call instead of swallowing a failure.
function makeFakePagx({ allowRender = true } = {}) {
  const id = Math.random().toString(36).slice(2);
  const file = path.join(dir, `fake-pagx-${id}.sh`);
  const log = path.join(dir, `fake-pagx-${id}.log`);
  fs.writeFileSync(file, [
    '#!/bin/sh',
    'cmd="$1"; shift',
    `printf '%s\\n' "$cmd" >> ${JSON.stringify(log)}`,
    'out=""',
    'while [ "$#" -gt 0 ]; do',
    '  case "$1" in',
    '    --output) out="$2"; shift;;',
    '  esac',
    '  shift',
    'done',
    'case "$cmd" in',
    '  import) [ -n "$out" ] && echo -n "<pag/>" > "$out" ;;',
    '  resolve) : ;;',
    '  font) : ;;',                          // font-embed: subcommand is `font embed`
    `  render) ${allowRender ? '[ -n "$out" ] && echo -n "PNG" > "$out"' : 'exit 99'} ;;`,
    'esac',
  ].join('\n'));
  fs.chmodSync(file, 0o755);
  // Read the per-command call counts on demand from the wrapper's log file.
  // Returning a Proxy lets tests reset a specific count by assigning 0; the
  // assignment truncates the on-disk log so the next read returns the fresh
  // state. (We don't allow setting non-zero values — tests don't need that.)
  const handle = {
    path: file,
    get callsByCmd() {
      const text = fs.existsSync(log) ? fs.readFileSync(log, 'utf8') : '';
      const out = { import: 0, resolve: 0, font: 0, render: 0 };
      for (const line of text.split('\n')) {
        if (line && Object.prototype.hasOwnProperty.call(out, line)) out[line]++;
      }
      return new Proxy(out, {
        set(target, prop, val) {
          target[prop] = val;
          if (val === 0) fs.writeFileSync(log, '');
          return true;
        },
      });
    },
  };
  return handle;
}
