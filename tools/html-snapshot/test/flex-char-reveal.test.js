'use strict';

// Regression tests for flexCharCarriesReveal: the predicate that keeps a
// per-character decode/typewriter reveal alive when the animated `.ch` span is
// a *flex item* rather than a nested child.
//
// Bug it guards: loose value text in a `<div class="spec"><b>CLASS</b>/ ROGUE
// BLADE</div>` flex row is wrapped one-span-per-char by the page's typewriter
// (each char `opacity: 0` + its own `pagxAnim*` opacity/colour keyframes). The
// `<b>` label takes the absolute char-box path and reveals correctly, but each
// loose value char is a flex item and hit renderTextLeaf's bare pure-inline
// `<span>` branch, which forwards only text-scope declarations and drops the
// box-scope `opacity`. The importer then folds the reveal into a static colour
// alpha and froze every value glyph fully visible from t=0 — `/ROGUEBLADE`
// popped in at frame 0 while `CLASS` stayed hidden until typed. When this
// predicate fires, renderTextLeaf instead emits a sized flex-item box that
// keeps the opacity reveal on the outer Layer.

const { flexCharCarriesReveal } = require('../dist/lib/browser-snapshot');

// Minimal computed-style stub: only `animation-name` + `opacity` are read.
function computed({ animationName = 'none', opacity = '1' } = {}) {
  const props = { 'animation-name': animationName, opacity };
  return {
    getPropertyValue: (p) => (Object.prototype.hasOwnProperty.call(props, p) ? props[p] : ''),
  };
}

describe('flexCharCarriesReveal', () => {
  test('pagxAnim reveal with hidden base opacity (typewriter char) → true', () => {
    expect(flexCharCarriesReveal(computed({ animationName: 'pagxAnim314', opacity: '0' }))).toBe(true);
  });

  test('pagxAnim reveal caught mid-fade (partial opacity) → true', () => {
    expect(flexCharCarriesReveal(computed({ animationName: 'pagxAnim314', opacity: '0.4' }))).toBe(true);
  });

  test('pagxAnim on an already fully-visible char → false (bare span is fine)', () => {
    expect(flexCharCarriesReveal(computed({ animationName: 'pagxAnim314', opacity: '1' }))).toBe(false);
  });

  test('hidden char with no pagxAnim → false (static, not a reveal)', () => {
    expect(flexCharCarriesReveal(computed({ animationName: 'none', opacity: '0' }))).toBe(false);
  });

  test('author animation (non-pagx) with hidden base → false', () => {
    // Only the capture pass owns the bundled opacity/colour reveal; an author
    // keyframe named otherwise is not our reveal shape and keeps the existing
    // pure-inline path.
    expect(flexCharCarriesReveal(computed({ animationName: 'pulse', opacity: '0' }))).toBe(false);
  });

  test('comma-separated animation list led by pagxAnim → true', () => {
    expect(flexCharCarriesReveal(computed({ animationName: 'pagxAnim7, blink', opacity: '0' }))).toBe(true);
  });
});
