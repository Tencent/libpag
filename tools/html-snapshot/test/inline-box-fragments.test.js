'use strict';

// Regression tests for inlineBoxLineRects: an inline-level box (a `<mark>`, a
// styled inline `<span>`, …) that wraps across several lines must paint its
// background/border ONCE PER LINE FRAGMENT, tightly hugging each line — never as
// one rectangle spanning the union bounding box. Before the fix a wrapped
// `<mark>` emitted a single `getBoundingClientRect()` box, painting a solid
// colour slab over the empty leading/trailing space around the wrapped text.
//
// `inlineBoxLineRects` owns the decision + per-line geometry (kept free of the
// browser-only style schema so it is unit-testable); `emitInlineBoxFragments`
// is the thin `buildStyle` wrapper exercised end-to-end by the eval pipeline.

const { inlineBoxLineRects } = require('../dist/lib/browser-snapshot');

const GRADIENT =
  'linear-gradient(120deg, rgba(100, 200, 120, 0.5) 0%, rgba(100, 200, 120, 0.5) 100%)';

function mockComputed({ display = 'inline', props = {} } = {}) {
  return {
    display,
    getPropertyValue: (p) => (Object.prototype.hasOwnProperty.call(props, p) ? props[p] : ''),
  };
}

function mockEl(rects) {
  return { getClientRects: () => rects };
}

describe('inlineBoxLineRects', () => {
  const parentRect = { left: 10, top: 20 };

  test('splits a wrapped inline gradient box into one rect per line fragment', () => {
    const el = mockEl([
      { left: 300, top: 100, width: 60, height: 22 }, // line 1 tail
      { left: 20, top: 128, width: 20, height: 22 }, // line 2 head
    ]);
    const computed = mockComputed({ props: { 'background-image': GRADIENT } });
    const rects = inlineBoxLineRects(el, parentRect, computed);
    expect(rects).toEqual([
      { left: 290, top: 80, width: 60, height: 22 },
      { left: 10, top: 108, width: 20, height: 22 },
    ]);
    // Never the union bounding box (full column width) as one slab.
    const totalWidth = rects.reduce((sum, r) => sum + r.width, 0);
    expect(totalWidth).toBeLessThan(350);
  });

  test('also splits a wrapped inline solid background-color box', () => {
    const el = mockEl([
      { left: 300, top: 100, width: 60, height: 22 },
      { left: 20, top: 128, width: 20, height: 22 },
    ]);
    const computed = mockComputed({ props: { 'background-color': 'rgb(255, 255, 0)' } });
    expect(inlineBoxLineRects(el, parentRect, computed)).toHaveLength(2);
  });

  test('also splits a wrapped inline box carrying only a border', () => {
    const el = mockEl([
      { left: 300, top: 100, width: 60, height: 22 },
      { left: 20, top: 128, width: 20, height: 22 },
    ]);
    const computed = mockComputed({ props: { 'border-top-width': '1px' } });
    expect(inlineBoxLineRects(el, parentRect, computed)).toHaveLength(2);
  });

  test('returns null for a single-line inline box (caller keeps single-box path)', () => {
    const el = mockEl([{ left: 300, top: 100, width: 60, height: 22 }]);
    const computed = mockComputed({ props: { 'background-image': GRADIENT } });
    expect(inlineBoxLineRects(el, parentRect, computed)).toBeNull();
  });

  test('returns null for a block-level box even when it reports multiple rects', () => {
    const el = mockEl([
      { left: 300, top: 100, width: 60, height: 22 },
      { left: 20, top: 128, width: 20, height: 22 },
    ]);
    const computed = mockComputed({ display: 'block', props: { 'background-image': GRADIENT } });
    expect(inlineBoxLineRects(el, parentRect, computed)).toBeNull();
  });

  test('returns null when the inline box carries no paintable visuals', () => {
    const el = mockEl([
      { left: 300, top: 100, width: 60, height: 22 },
      { left: 20, top: 128, width: 20, height: 22 },
    ]);
    expect(inlineBoxLineRects(el, parentRect, mockComputed())).toBeNull();
  });

  test('coalesces same-line glyph-run rects into one fragment per visual line', () => {
    // Two rects share line 1's top (a bidi / soft-hyphen split); they must merge
    // into a single fragment, so a two-line run yields exactly two rects.
    const el = mockEl([
      { left: 300, top: 100, width: 40, height: 22 },
      { left: 340, top: 100, width: 20, height: 22 },
      { left: 20, top: 128, width: 20, height: 22 },
    ]);
    const computed = mockComputed({ props: { 'background-image': GRADIENT } });
    const rects = inlineBoxLineRects(el, parentRect, computed);
    expect(rects).toHaveLength(2);
    // Merged line-1 fragment spans both runs: left 300→290 (rel), width 60.
    expect(rects[0]).toEqual({ left: 290, top: 80, width: 60, height: 22 });
  });
});
