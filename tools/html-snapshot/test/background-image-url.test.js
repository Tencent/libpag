'use strict';

// Regression tests for normalizeBackgroundImage: a `url(...)` background must be
// emitted with SINGLE quotes so the value embeds safely inside the double-quoted
// `style="…"` attribute the snapshot writes. The <body> background path pushes
// this value verbatim (it does not route through appendStyleProp's double-quote
// rewrite), so a double-quoted url() prematurely closed the style attribute and
// broke the pagx importer's XML parse — e.g. `background-image: url('bg.jpg')`
// referencing a missing local file made `pagx import` fail with
// "failed to parse '<file>'" instead of importing with a graceful warning.

const { normalizeBackgroundImage } = require('../dist/lib/browser-snapshot');

describe('normalizeBackgroundImage', () => {
  test('wraps a plain absolute path in single quotes (never double)', () => {
    const out = normalizeBackgroundImage('url("/Users/me/Downloads/bg.jpg")');
    expect(out).toBe("url('/Users/me/Downloads/bg.jpg')");
    expect(out).not.toContain('"');
  });

  test('rewrites a file:// url to a bare path in single quotes', () => {
    const out = normalizeBackgroundImage('url("file:///Users/me/bg.jpg")');
    expect(out).toBe("url('/Users/me/bg.jpg')");
    expect(out).not.toContain('"');
  });

  test('keeps a data: URI, single-quoted', () => {
    const out = normalizeBackgroundImage('url("data:image/png;base64,AAAA")');
    expect(out).toBe("url('data:image/png;base64,AAAA')");
    expect(out).not.toContain('"');
  });

  test('keeps a remote http(s) url, single-quoted', () => {
    const out = normalizeBackgroundImage("url('https://cdn.test/bg.jpg')");
    expect(out).toBe("url('https://cdn.test/bg.jpg')");
    expect(out).not.toContain('"');
  });

  test('never leaves a double quote that would close the style attribute', () => {
    // A pathological path containing a double quote must not survive as one.
    const out = normalizeBackgroundImage('url("/tmp/a\\"b.jpg")');
    expect(out).not.toContain('"');
  });

  test('passes gradients through unchanged', () => {
    const grad = 'linear-gradient(90deg, rgb(0, 0, 0) 0%, rgb(255, 255, 255) 100%)';
    expect(normalizeBackgroundImage(grad)).toBe(grad);
  });

  test('drops none / empty', () => {
    expect(normalizeBackgroundImage('none')).toBe('');
    expect(normalizeBackgroundImage('')).toBe('');
  });
});
