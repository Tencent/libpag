'use strict';

const path = require('path');
const {
  parseArgs,
  printUsage,
  isHttpUrl,
  fail,
  makeFail,
  parseNumber,
  errMessage,
  LOG_PREFIX,
} = require('../lib/cli');
const { captureExit, argv } = require('./helpers');

describe('isHttpUrl', () => {
  test.each([
    ['http://example.com', true],
    ['https://example.com/path?q=1', true],
    ['HTTP://EXAMPLE.COM', true],
    ['HTTPS://example.com', true],
    ['ftp://example.com', false],
    ['file:///tmp/page.html', false],
    ['/local/path.html', false],
    ['page.html', false],
    ['htttp://typo.com', false],
    ['', false],
  ])('isHttpUrl(%j) === %j', (input, expected) => {
    expect(isHttpUrl(input)).toBe(expected);
  });
});

describe('errMessage', () => {
  test('returns the message of a real Error', () => {
    expect(errMessage(new Error('boom'))).toBe('boom');
  });

  test('stringifies a thrown string', () => {
    expect(errMessage('plain string')).toBe('plain string');
  });

  test('stringifies undefined and null', () => {
    expect(errMessage(undefined)).toBe('undefined');
    expect(errMessage(null)).toBe('null');
  });

  test('falls back to String() when message is empty', () => {
    expect(errMessage({ message: '' })).toBe('[object Object]');
  });
});

describe('parseNumber', () => {
  test('parses a valid non-negative number', () => {
    expect(parseNumber('--wait-ms', '800')).toBe(800);
  });

  test('honours a custom min bound', () => {
    expect(parseNumber('--viewport-width', '1', { min: 1 })).toBe(1);
  });

  test('reports via the injected fail and rejects values below min', () => {
    const calls = [];
    const failFn = (msg) => { calls.push(msg); };
    parseNumber('--viewport-width', '0', { min: 1, fail: failFn });
    expect(calls).toHaveLength(1);
    expect(calls[0]).toMatch(/>= 1/);
  });

  test('rejects non-numeric input', () => {
    const calls = [];
    parseNumber('--wait-ms', 'abc', { fail: (m) => calls.push(m) });
    expect(calls[0]).toMatch(/non-negative/);
  });

  test('reports a missing argument', () => {
    const calls = [];
    parseNumber('--wait-ms', undefined, { fail: (m) => calls.push(m) });
    expect(calls[0]).toMatch(/requires a numeric argument/);
  });

  test('default fail path calls process.exit(2)', () => {
    const { exited, code, messages } = captureExit(() =>
      parseNumber('--wait-ms', 'nope'));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toContain(LOG_PREFIX);
  });
});

describe('fail / makeFail', () => {
  test('fail logs with the standard prefix and exits 2', () => {
    const { exited, code, messages } = captureExit(() => fail('nope'));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toBe(`${LOG_PREFIX}nope`);
  });

  test('makeFail uses the supplied prefix and default code', () => {
    const failWith = makeFail('eval');
    const { exited, code, messages } = captureExit(() => failWith('bad'));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toBe('eval: bad');
  });

  test('makeFail honours an explicit exit code', () => {
    const failWith = makeFail('eval');
    const { code } = captureExit(() => failWith('bad', 7));
    expect(code).toBe(7);
  });
});

describe('printUsage', () => {
  test('writes help text to stdout', () => {
    const spy = jest.spyOn(console, 'log').mockImplementation(() => {});
    try {
      printUsage();
      expect(spy).toHaveBeenCalledTimes(1);
      expect(spy.mock.calls[0][0]).toMatch(/Usage: html-snapshot/);
    } finally {
      spy.mockRestore();
    }
  });
});

describe('parseArgs — file inputs', () => {
  test('derives the default sibling output for a local file', () => {
    const opts = parseArgs(argv('/tmp/page.html'));
    expect(opts.isUrl).toBe(false);
    expect(opts.input).toBe(path.resolve('/tmp/page.html'));
    expect(opts.output).toBe(path.resolve('/tmp/page.subset.html'));
    expect(opts.outputToStdout).toBe(false);
  });

  test('respects an explicit -o output', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '/tmp/out.html'));
    expect(opts.output).toBe(path.resolve('/tmp/out.html'));
  });

  test('"-o -" routes the HTML to stdout', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '-'));
    expect(opts.outputToStdout).toBe(true);
    expect(opts.output).toBe('');
  });

  test('parses numeric viewport / wait flags', () => {
    const opts = parseArgs(argv(
      '/tmp/page.html',
      '--viewport-width', '800',
      '--viewport-height', '600',
      '--wait-ms', '1200',
      '--selector', '#app',
    ));
    expect(opts.viewportWidth).toBe(800);
    expect(opts.viewportHeight).toBe(600);
    expect(opts.waitMs).toBe(1200);
    expect(opts.selector).toBe('#app');
  });

  test('--no-inline-icon-fonts disables the icon pass', () => {
    const opts = parseArgs(argv('/tmp/page.html', '--no-inline-icon-fonts'));
    expect(opts.inlineIconFonts).toBe(false);
  });

  test('--download-fonts defaults --font-dir to a sibling .fonts dir', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '/tmp/out.subset.html', '--download-fonts'));
    expect(opts.downloadFonts).toBe(true);
    expect(opts.fontDir).toBe(path.resolve('/tmp/out.fonts'));
  });

  test('--font-dir overrides the default destination', () => {
    const opts = parseArgs(argv('/tmp/page.html', '--download-fonts', '--font-dir', '/tmp/fonts'));
    expect(opts.fontDir).toBe(path.resolve('/tmp/fonts'));
  });

  test('--font-manifest is resolved to an absolute path', () => {
    const opts = parseArgs(argv(
      '/tmp/page.html', '--download-fonts', '--font-manifest', 'manifest.txt',
    ));
    expect(opts.fontManifest).toBe(path.resolve('manifest.txt'));
  });

  test('--download-images defaults --image-dir to a sibling .images dir', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '/tmp/out.subset.html', '--download-images'));
    expect(opts.downloadImages).toBe(true);
    expect(opts.imageDir).toBe(path.resolve('/tmp/out.images'));
  });

  test('--image-dir overrides the default destination', () => {
    const opts = parseArgs(argv('/tmp/page.html', '--download-images', '--image-dir', '/tmp/imgs'));
    expect(opts.imageDir).toBe(path.resolve('/tmp/imgs'));
  });

  test('rejects --cookie / --header for file inputs', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('/tmp/page.html', '--cookie', 'a=b')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/only supported with URL inputs/);
  });

  test('rejects --font-manifest without --download-fonts', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('/tmp/page.html', '--font-manifest', 'm.txt')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/--font-manifest requires --download-fonts/);
  });
});

describe('parseArgs — URL inputs', () => {
  test('requires -o for URL inputs', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('https://example.com')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/-o\/--output is required/);
  });

  test('accepts cookies and headers scoped to the URL', () => {
    const opts = parseArgs(argv(
      'https://example.com', '-o', '/tmp/out.html',
      '--cookie', 'session=abc',
      '--header', 'Authorization: Bearer xyz',
    ));
    expect(opts.isUrl).toBe(true);
    expect(opts.input).toBe('https://example.com');
    expect(opts.cookies).toEqual([{ name: 'session', value: 'abc' }]);
    expect(opts.headers).toEqual([['Authorization', 'Bearer xyz']]);
  });

  test('--download-fonts to stdout requires an explicit --font-dir', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('https://example.com', '-o', '-', '--download-fonts')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/requires --font-dir/);
  });

  test('--download-images to stdout requires an explicit --image-dir', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('https://example.com', '-o', '-', '--download-images')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/requires --image-dir/);
  });
});

describe('parseArgs — argument errors', () => {
  test('unknown option exits 2', () => {
    const { exited, code, messages } = captureExit(() =>
      parseArgs(argv('/tmp/page.html', '--bogus')));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toMatch(/unknown option '--bogus'/);
  });

  test('no positional argument prints usage and exits 2', () => {
    const logSpy = jest.spyOn(console, 'log').mockImplementation(() => {});
    try {
      const { exited, code } = captureExit(() => parseArgs(argv()));
      expect(exited).toBe(true);
      expect(code).toBe(2);
    } finally {
      logSpy.mockRestore();
    }
  });

  test('-h prints usage and exits 0', () => {
    const logSpy = jest.spyOn(console, 'log').mockImplementation(() => {});
    try {
      const { exited, code } = captureExit(() => parseArgs(argv('-h')));
      expect(exited).toBe(true);
      expect(code).toBe(0);
    } finally {
      logSpy.mockRestore();
    }
  });

  test('malformed --cookie exits 2', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('https://example.com', '-o', '/tmp/o.html', '--cookie', 'nope')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/expects 'name=value'/);
  });

  test('malformed --header exits 2', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('https://example.com', '-o', '/tmp/o.html', '--header', 'nocolon')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/expects 'Key: Value'/);
  });

  test('unknown --browser-engine exits 2', () => {
    const { exited, messages } = captureExit(() =>
      parseArgs(argv('/tmp/page.html', '--browser-engine', 'webkit-nope')));
    expect(exited).toBe(true);
    expect(messages[0]).toMatch(/unsupported browser engine/);
  });

  test('valid --browser-engine is accepted', () => {
    const opts = parseArgs(argv('/tmp/page.html', '--browser-engine', 'playwright'));
    expect(opts.browserEngine).toBe('playwright');
  });
});
