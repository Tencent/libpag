'use strict';

// Regression tests for mergeRectsOnSameLine: getClientRects reports glyph INK
// bounds that routinely exceed the line-height, so two ADJACENT lines whose
// tops differ by exactly the line-height still overlap by ~1px of ink. That
// sliver must not merge them into one line, while same-line glyph runs (soft
// hyphen glyph, bidi split) that share the line's top must merge.

const { mergeRectsOnSameLine } = require('../dist/lib/browser-snapshot');

describe('mergeRectsOnSameLine', () => {
  test('does not merge two adjacent lines whose ink overlaps by ~1px (y axis)', () => {
    // line-height 16, ink height 17: line 2 top (364) sits 1px above line 1
    // bottom (365). The bug merged these into a single line; they must stay 2.
    const rects = [
      { left: 24, top: 348, width: 149.89, height: 17 },
      { left: 24, top: 364, width: 112, height: 17 },
    ];
    const merged = mergeRectsOnSameLine(rects, 'y');
    expect(merged).toHaveLength(2);
    expect(merged[0].top).toBe(348);
    expect(merged[1].top).toBe(364);
  });

  test('merges same-line glyph runs that share the line top (soft hyphen / bidi)', () => {
    // Two runs on the same visual line: the text rect and a trailing hyphen
    // glyph rect, both at top=0, height=17. They must collapse into one rect
    // whose inline extent spans both runs.
    const rects = [
      { left: 0, top: 0, width: 60, height: 17 },
      { left: 60, top: 0, width: 8, height: 17 },
    ];
    const merged = mergeRectsOnSameLine(rects, 'y');
    expect(merged).toHaveLength(1);
    expect(merged[0].left).toBe(0);
    expect(merged[0].width).toBe(68);
  });

  test('does not merge adjacent columns whose ink overlaps by ~1px (vertical-rl)', () => {
    // Vertical writing mode: columns stack along x. line-width 16, ink 17,
    // so adjacent columns overlap by 1px on the block (x) axis but are still
    // distinct columns.
    const rects = [
      { left: 40, top: 0, width: 17, height: 100 },
      { left: 24, top: 0, width: 17, height: 80 },
    ];
    const merged = mergeRectsOnSameLine(rects, 'x-rl');
    expect(merged).toHaveLength(2);
  });

  test('returns a single-element copy unchanged', () => {
    const rects = [{ left: 0, top: 0, width: 50, height: 16 }];
    const merged = mergeRectsOnSameLine(rects, 'y');
    expect(merged).toHaveLength(1);
    expect(merged[0]).toEqual({ left: 0, top: 0, width: 50, height: 16 });
  });

  test('merges three same-line runs and keeps three distinct lines separate', () => {
    const rects = [
      // line 1: two runs sharing top=0
      { left: 0, top: 0, width: 40, height: 17 },
      { left: 40, top: 0, width: 10, height: 17 },
      // line 2
      { left: 0, top: 16, width: 55, height: 17 },
      // line 3
      { left: 0, top: 32, width: 30, height: 17 },
    ];
    const merged = mergeRectsOnSameLine(rects, 'y');
    expect(merged).toHaveLength(3);
    expect(merged[0].width).toBe(50);
    expect(merged[1].top).toBe(16);
    expect(merged[2].top).toBe(32);
  });
});
