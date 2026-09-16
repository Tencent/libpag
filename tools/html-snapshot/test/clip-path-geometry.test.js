'use strict';

// Unit tests for the geometric clip-path → `<clipPath>` synthesis
// (browser-snapshot.ts). CSS `clip-path` values that are NOT `url(#id)` — the
// basic shape functions `inset()` / `circle()` / `ellipse()` / `polygon()` /
// `path()` — are converted into the INNER markup of a `<clipPath>` def, with all
// `%` / `calc()` resolved into the element's border-box pixel space. The C++
// importer serializes exactly those children into a white `<svg>` framed to the
// box and reads them as a contour mask, so the coordinates here must match the
// element's `width`/`height` (border-box) coordinate system.

const {
  pagxEvalLengthPx,
  pagxParseClipShape,
  pagxClipPathInnerMarkup,
} = require('../dist/lib/browser-snapshot');

// Minimal `computed` stub. Border / padding longhands default to 0 (the common
// case: clip-path against the border-box), overridable per test.
function mockComputed(props = {}) {
  return {
    getPropertyValue: (p) =>
      (Object.prototype.hasOwnProperty.call(props, p) ? String(props[p]) : ''),
  };
}

describe('pagxEvalLengthPx', () => {
  test('resolves px, unitless and percentage against the basis', () => {
    expect(pagxEvalLengthPx('10px', 200)).toBe(10);
    expect(pagxEvalLengthPx('10', 200)).toBe(10);
    expect(pagxEvalLengthPx('50%', 200)).toBe(100);
    expect(pagxEvalLengthPx('0', 200)).toBe(0);
  });

  test('evaluates calc() mixing percentage and px (the sci-fi HUD case)', () => {
    expect(pagxEvalLengthPx('calc(100% - 10px)', 200)).toBe(190);
    expect(pagxEvalLengthPx('calc(100% - 26px)', 100)).toBe(74);
    expect(pagxEvalLengthPx('calc(50% + 5px)', 200)).toBe(105);
  });

  test('honours calc multiplication / division and precedence', () => {
    expect(pagxEvalLengthPx('calc(50% * 2)', 200)).toBe(200);
    expect(pagxEvalLengthPx('calc(100px / 4)', 0)).toBe(25);
    expect(pagxEvalLengthPx('calc(10px + 2px * 3)', 0)).toBe(16);
  });

  test('empty / whitespace yields 0', () => {
    expect(pagxEvalLengthPx('', 100)).toBe(0);
    expect(pagxEvalLengthPx('   ', 100)).toBe(0);
  });
});

describe('pagxParseClipShape', () => {
  test('returns null for none and url() references', () => {
    expect(pagxParseClipShape('none')).toBeNull();
    expect(pagxParseClipShape('url(#clip)')).toBeNull();
    expect(pagxParseClipShape('url("#clip")')).toBeNull();
    expect(pagxParseClipShape('')).toBeNull();
  });

  test('extracts the shape function and its balanced args', () => {
    expect(pagxParseClipShape('inset(10px 20px)')).toEqual({
      shape: 'inset', args: '10px 20px', box: 'border-box',
    });
    const poly = pagxParseClipShape('polygon(0 0, calc(100% - 10px) 0, 100% 100%)');
    expect(poly.shape).toBe('polygon');
    expect(poly.args).toBe('0 0, calc(100% - 10px) 0, 100% 100%');
  });

  test('detects a trailing / leading geometry-box keyword', () => {
    expect(pagxParseClipShape('circle(50%) content-box').box).toBe('content-box');
    expect(pagxParseClipShape('padding-box inset(0)').box).toBe('padding-box');
  });
});

describe('pagxClipPathInnerMarkup — polygon', () => {
  const computed = mockComputed();

  test('resolves a hexagon cut-corner polygon with calc() into pixel points', () => {
    // The sci-fi HUD button border: polygon with 10px cut corners on a 200x100 box.
    const inner = pagxClipPathInnerMarkup(
      'polygon(0 0, calc(100% - 10px) 0, 100% 10px, 100% 100%, 10px 100%, 0 calc(100% - 10px))',
      200, 100, computed,
    );
    expect(inner).toBe(
      '<polygon points="0,0 190,0 200,10 200,100 10,100 0,90"/>',
    );
  });

  test('honours an evenodd fill-rule prefix', () => {
    const inner = pagxClipPathInnerMarkup('polygon(evenodd, 0 0, 100% 0, 50% 100%)', 100, 100, computed);
    expect(inner).toBe(
      '<polygon points="0,0 100,0 50,100" clip-rule="evenodd" fill-rule="evenodd"/>',
    );
  });

  test('drops a degenerate polygon with fewer than three points', () => {
    expect(pagxClipPathInnerMarkup('polygon(0 0, 100% 0)', 100, 100, computed)).toBe('');
  });
});

describe('pagxClipPathInnerMarkup — inset', () => {
  const computed = mockComputed();

  test('single value insets on all four sides', () => {
    expect(pagxClipPathInnerMarkup('inset(10%)', 200, 100, computed)).toBe(
      '<rect x="20" y="10" width="160" height="80"/>',
    );
  });

  test('four-value shorthand (top right bottom left)', () => {
    expect(pagxClipPathInnerMarkup('inset(0 49% 0 49%)', 200, 100, computed)).toBe(
      // left/right resolve against width (200): 49% = 98 each → w = 4.
      '<rect x="98" y="0" width="4" height="100"/>',
    );
  });

  test('round keyword produces a uniform rx/ry', () => {
    expect(pagxClipPathInnerMarkup('inset(0 0 0 0 round 8px)', 100, 100, computed)).toBe(
      '<rect x="0" y="0" width="100" height="100" rx="8" ry="8"/>',
    );
  });

  test('fully-collapsed inset is dropped', () => {
    expect(pagxClipPathInnerMarkup('inset(50%)', 200, 100, computed)).toBe('');
  });
});

describe('pagxClipPathInnerMarkup — circle & ellipse', () => {
  const computed = mockComputed();

  test('circle with explicit radius at center', () => {
    expect(pagxClipPathInnerMarkup('circle(40px at 50% 50%)', 200, 100, computed)).toBe(
      '<circle cx="100" cy="50" r="40"/>',
    );
  });

  test('circle closest-side uses the nearest edge distance', () => {
    // 200x100 box, center (100,50): nearest side distance = 50.
    expect(pagxClipPathInnerMarkup('circle(closest-side)', 200, 100, computed)).toBe(
      '<circle cx="100" cy="50" r="50"/>',
    );
  });

  test('ellipse with per-axis radii and an at position', () => {
    expect(pagxClipPathInnerMarkup('ellipse(50% 25% at 50% 50%)', 200, 100, computed)).toBe(
      '<ellipse cx="100" cy="50" rx="100" ry="25"/>',
    );
  });
});

describe('pagxClipPathInnerMarkup — path', () => {
  const computed = mockComputed();

  test('emits the raw path data verbatim when the reference box is at origin', () => {
    expect(pagxClipPathInnerMarkup('path("M0 0 L10 0 L10 10 Z")', 100, 100, computed)).toBe(
      '<path d="M0 0 L10 0 L10 10 Z"/>',
    );
  });

  test('carries an evenodd fill-rule', () => {
    expect(pagxClipPathInnerMarkup('path(evenodd, "M0 0 H10 V10 Z")', 100, 100, computed)).toBe(
      '<path d="M0 0 H10 V10 Z" clip-rule="evenodd" fill-rule="evenodd"/>',
    );
  });
});

describe('pagxClipPathInnerMarkup — reference box', () => {
  test('content-box shifts the shape inward by border + padding', () => {
    const computed = mockComputed({
      'border-top-width': '5px',
      'border-right-width': '5px',
      'border-bottom-width': '5px',
      'border-left-width': '5px',
      'padding-top': '10px',
      'padding-right': '10px',
      'padding-bottom': '10px',
      'padding-left': '10px',
    });
    // border-box 200x100 → content-box origin (15,15), size 170x70.
    // inset(0) fills the content box.
    expect(pagxClipPathInnerMarkup('inset(0) content-box', 200, 100, computed)).toBe(
      '<rect x="15" y="15" width="170" height="70"/>',
    );
  });

  test('url() and none produce no synthesized markup', () => {
    const computed = mockComputed();
    expect(pagxClipPathInnerMarkup('url(#foo)', 100, 100, computed)).toBe('');
    expect(pagxClipPathInnerMarkup('none', 100, 100, computed)).toBe('');
  });
});
