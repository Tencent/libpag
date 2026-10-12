'use strict';

// `-webkit-line-clamp` support in the line-span emitter. The browser cuts a
// clamped box after its Nth line and appends "…"; the DOM keeps every
// character, so without these helpers the walker materialises the hidden
// lines too and PAGX paints them (clipped mid-glyph by the box height) with
// no ellipsis. `readLineClamp` reads the author's count off the computed
// style; `ellipsizeClampedLine` rebuilds the browser's cut by measuring a
// scratch canvas. Both take injected `computed` / `document` seams, so a
// fake monospace canvas drives them deterministically in node.

const { readLineClamp, ellipsizeClampedLine } = require('../dist/lib/browser-snapshot');

// A proportional-free stub: every code point is `charWidth` px wide.
function installCanvas(charWidth) {
  const ctx = {
    font: '',
    measureText: (text) => ({ width: Array.from(text).length * charWidth }),
  };
  global.document = {
    createElement: (tag) => {
      if (tag !== 'canvas') throw new Error(`unexpected element: ${tag}`);
      return { getContext: (kind) => (kind === '2d' ? ctx : null) };
    },
  };
  return ctx;
}

function computedStub(props) {
  return {
    getPropertyValue: (prop) => (prop in props ? props[prop] : ''),
  };
}

const FONT_PROPS = {
  'font-style': 'normal',
  'font-variant': 'normal',
  'font-weight': '400',
  'font-size': '13px',
  'font-family': '"PingFang SC", sans-serif',
};

let savedDocument;

beforeEach(() => {
  savedDocument = global.document;
});

afterEach(() => {
  global.document = savedDocument;
});

describe('readLineClamp', () => {
  test('reads the authored count', () => {
    expect(readLineClamp(computedStub({ '-webkit-line-clamp': '2' }))).toBe(2);
    expect(readLineClamp(computedStub({ '-webkit-line-clamp': ' 3 ' }))).toBe(3);
  });

  test('treats none / empty / garbage as disabled', () => {
    expect(readLineClamp(computedStub({ '-webkit-line-clamp': 'none' }))).toBe(0);
    expect(readLineClamp(computedStub({ '-webkit-line-clamp': '' }))).toBe(0);
    expect(readLineClamp(computedStub({ '-webkit-line-clamp': 'abc' }))).toBe(0);
    expect(readLineClamp(computedStub({ '-webkit-line-clamp': '0' }))).toBe(0);
  });
});

describe('ellipsizeClampedLine', () => {
  const boxLeft = 100;
  const lineLeft = 100;

  test('keeps the longest prefix that leaves room for the ellipsis', () => {
    installCanvas(10);
    // 15 chars at 10px + 10px ellipsis = 160px; 100px fits 9 chars + "…".
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '0px' });
    const out = ellipsizeClampedLine(
      'abcdefghijklmno', { left: lineLeft, top: 0, width: 150, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('abcdefghi\u2026');
  });

  test('shortens the available width by the line indent', () => {
    installCanvas(10);
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '0px' });
    // The line starts 20px into the content box → 80px usable → 7 chars + "…".
    const out = ellipsizeClampedLine(
      'abcdefghijklmno', { left: lineLeft + 20, top: 0, width: 150, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('abcdefg\u2026');
  });

  test('subtracts the container padding from the line start', () => {
    installCanvas(10);
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '10px' });
    // With 10px padding the content starts at boxLeft+10; a line at that same
    // x has zero indent → the full 100px is available.
    const out = ellipsizeClampedLine(
      'abcdefghijklmno', { left: boxLeft + 10, top: 0, width: 150, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('abcdefghi\u2026');
  });

  test('appends the ellipsis to the full text when it already fits', () => {
    installCanvas(10);
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '0px' });
    const out = ellipsizeClampedLine(
      'abcde', { left: lineLeft, top: 0, width: 50, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('abcde\u2026');
  });

  test('drops trailing whitespace before the ellipsis', () => {
    installCanvas(10);
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '0px' });
    // The 9-char prefix ends on the space at index 8 → it must not survive.
    const out = ellipsizeClampedLine(
      'abcdefgh ijklmnop', { left: lineLeft, top: 0, width: 150, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('abcdefgh\u2026');
  });

  test('falls back to a bare ellipsis when nothing fits', () => {
    installCanvas(10);
    const computed = computedStub({ ...FONT_PROPS, width: '5px', 'padding-left': '0px' });
    const out = ellipsizeClampedLine(
      'abcdefghijklmno', { left: lineLeft, top: 0, width: 150, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('\u2026');
  });

  test('returns the text unchanged without a resolvable width', () => {
    installCanvas(10);
    const text = 'abcdefghijklmno';
    for (const width of ['', 'auto', '0px']) {
      const computed = computedStub({ ...FONT_PROPS, width, 'padding-left': '0px' });
      expect(ellipsizeClampedLine(
        text, { left: lineLeft, top: 0, width: 150, height: 18 },
        computed, { left: boxLeft, top: 0 },
      )).toBe(text);
    }
  });

  test('returns the text unchanged when no 2d context is available', () => {
    global.document = { createElement: () => ({ getContext: () => null }) };
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '0px' });
    const text = 'abcdefghijklmno';
    expect(ellipsizeClampedLine(
      text, { left: lineLeft, top: 0, width: 150, height: 18 },
      computed, { left: boxLeft, top: 0 },
    )).toBe(text);
  });

  test('leaves the text untouched when the indent consumes the content box', () => {
    installCanvas(10);
    const computed = computedStub({ ...FONT_PROPS, width: '100px', 'padding-left': '0px' });
    // A line starting beyond the content box is already overflowing; cutting
    // it again would only guess, so the text passes through untouched.
    const out = ellipsizeClampedLine(
      'abcdef', { left: lineLeft + 500, top: 0, width: 60, height: 18 },
      computed, { left: boxLeft, top: 0 },
    );
    expect(out).toBe('abcdef');
  });
});
