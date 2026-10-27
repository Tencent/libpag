'use strict';

// Regression tests for `dashedBorderSideSvg`: the per-side dashed / dotted
// border painter used by `borderOverlayHTML` for asymmetric borders (a single
// side, or unequal per-side widths — e.g. `.hero-sep { border-top: 1.5px
// dashed … }`). Before the fix such a side was downgraded to a solid
// `background-color` <div>, flattening a dashed hairline divider into a
// continuous strip. Each dashed/dotted side is now emitted as an inline <svg>
// line carrying `stroke-dasharray`, so the importer's SVG pass reproduces the
// dash pattern as a PAGX `<Stroke dashes=…>`. Uniform borders keep flowing
// through the `border` shorthand (covered in PAGXHTMLImporterTest).
//
// The dash proportions must match the importer's uniform-border path so a
// dashed divider looks identical whichever route it takes: `dashed` → dash
// 2×width, gap 1×width; `dotted` → zero-length round-capped dashes spaced
// 2×width apart (each renders as a dot of diameter width with a 1×width gap).

const { dashedBorderSideSvg } = require('../dist/lib/browser-snapshot');

describe('dashedBorderSideSvg — dashed sides', () => {
  const b = { width: 1.5, style: 'dashed', color: 'rgb(255, 255, 255)' };

  test('emits a stroked <svg> line, never a solid background-color div', () => {
    const svg = dashedBorderSideSvg('top', 100, 1.5, b);
    expect(svg).toContain('<svg');
    expect(svg).toContain('fill="none"');
    expect(svg).toContain('stroke="rgb(255, 255, 255)"');
    expect(svg).not.toContain('background-color');
  });

  test('uses dash 2×width, gap 1×width with a butt cap', () => {
    const svg = dashedBorderSideSvg('top', 100, 1.5, b);
    expect(svg).toContain('stroke-dasharray="3 1.5"');
    expect(svg).toContain('stroke-linecap="butt"');
    expect(svg).toContain('stroke-width="1.5px"');
  });

  test('centres the top line at half the border width', () => {
    // y = width/2 = 0.75 → roundPx → 0.8
    expect(dashedBorderSideSvg('top', 100, 40, b)).toContain('d="M 0 0.8 L 100 0.8"');
  });

  test('centres the bottom line at height minus half the border width', () => {
    const svg = dashedBorderSideSvg('bottom', 100, 40, { ...b, width: 2 });
    expect(svg).toContain('d="M 0 39 L 100 39"'); // y = 40 - 2/2 = 39
  });

  test('draws a vertical line for the left side', () => {
    const svg = dashedBorderSideSvg('left', 80, 50, { ...b, width: 2 });
    expect(svg).toContain('d="M 1 0 L 1 50"'); // x = width/2 = 1
  });

  test('draws a vertical line inset from the right edge', () => {
    const svg = dashedBorderSideSvg('right', 80, 50, { ...b, width: 2 });
    expect(svg).toContain('d="M 79 0 L 79 50"'); // x = 80 - 2/2 = 79
  });
});

describe('dashedBorderSideSvg — dotted sides', () => {
  const b = { width: 3, style: 'dotted', color: 'rgb(248, 113, 113)' };

  test('uses round-capped zero-length dashes spaced 2×width apart', () => {
    const svg = dashedBorderSideSvg('bottom', 100, 60, b);
    expect(svg).toContain('stroke-linecap="round"'); // round cap → dots
    expect(svg).toContain('stroke-dasharray="0 6"'); // zero dash, gap 2×width
    expect(svg).toContain('stroke="rgb(248, 113, 113)"');
    expect(svg).not.toContain('background-color');
  });
});
