'use strict';

const path = require('path');
const { parseArgs, printUsage } = require('../dist/lib/cli');
const { captureExit, argv } = require('./helpers');

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
  test('--scroll-reveal enables lazy-content walking', () => {
    expect(parseArgs(argv('/tmp/page.html', '--scroll-reveal')).scrollReveal).toBe(true);
  });

  test('rejects a whitespace-only header name', () => {
    const result = captureExit(() => parseArgs(argv('/tmp/page.html', '--header', '  : value')));
    expect(result.code).toBe(2);
    expect(result.messages.join('\n')).toMatch(/empty header name/);
  });
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

  test('captureAnimations is off by default and enabled by --capture-animations', () => {
    expect(parseArgs(argv('/tmp/page.html')).captureAnimations).toBe(false);
    const opts = parseArgs(argv('/tmp/page.html', '--capture-animations'));
    expect(opts.captureAnimations).toBe(true);
    expect(opts.reducedMotion).toBe(false);
  });

  test('reduced motion defaults on and --no-reduced-motion disables it', () => {
    expect(parseArgs(argv('/tmp/page.html')).reducedMotion).toBe(true);
    const opts = parseArgs(argv('/tmp/page.html', '--no-reduced-motion'));
    expect(opts.reducedMotion).toBe(false);
  });

  test('--download-fonts defaults --font-dir to a sibling .fonts dir', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '/tmp/out.subset.html', '--download-fonts'));
    expect(opts.downloadFonts).toBe(true);
    expect(opts.fontDir).toBe(path.resolve('/tmp/out.fonts'));
  });

  test('defaults --font-dir without --download-fonts so inline data: fonts land on disk', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '/tmp/out.subset.html'));
    expect(opts.downloadFonts).toBe(false);
    expect(opts.fontDir).toBe(path.resolve('/tmp/out.fonts'));
  });

  test('strips a .subset suffix from the derived --font-dir', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '/tmp/case.subset.html'));
    expect(opts.fontDir).toBe(path.resolve('/tmp/case.fonts'));
  });

  test('leaves --font-dir unset for stdout output unless the caller asks for fonts', () => {
    const opts = parseArgs(argv('/tmp/page.html', '-o', '-'));
    expect(opts.fontDir).toBe('');
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
  test('parses a scenario and selected checkpoint', () => {
    const opts=parseArgs(argv('/tmp/page.html','--scenario','states.json','--checkpoint','ready'));
    expect(opts.scenario).toBe(path.resolve('states.json'));
    expect(opts.checkpoint).toBe('ready');
  });

  test.each(['--capture-animations','--scroll-reveal','--viewport-width','--wait-ms','--selector'])(
    'rejects scenario conflicts with %s', flag => {
      const value=['--viewport-width','--wait-ms','--selector'].includes(flag) ? ['500'] : [];
      const {exited,messages}=captureExit(()=>parseArgs(argv('/tmp/page.html','--scenario','states.json',flag,...value)));
      expect(exited).toBe(true);
      expect(messages[0]).toContain('cannot be combined with --scenario');
    });

  test('rejects a checkpoint without a scenario',()=>{
    const {messages}=captureExit(()=>parseArgs(argv('/tmp/page.html','--checkpoint','ready')));
    expect(messages[0]).toContain('--checkpoint requires --scenario');
  });

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
