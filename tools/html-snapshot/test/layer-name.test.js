'use strict';

// Regression tests for `applyLayerName`: the snapshot rebuilds every element from
// computed style rather than cloning the live tree, so an author `name` attribute is
// dropped unless it is re-emitted here. `render` folds it onto the outer box the element
// produced so the PAGX importer can surface it as `layer->name` (mirroring `id`).

const { applyLayerName } = require('../dist/lib/browser-snapshot');

// Minimal stand-in for a DOM element exposing just the surface `applyLayerName` touches.
function mockEl(attrs) {
  return {
    getAttribute: (name) => (name in attrs ? attrs[name] : null),
  };
}

// A non-`contents` computed style keeps `applyLayerName` off the `getComputedStyle`
// browser global so the helper is exercisable under Node.
const blockStyle = { display: 'block' };

describe('applyLayerName', () => {
  test('injects the name attribute into the emitted outer box', () => {
    const out = applyLayerName(mockEl({ name: 'Hero' }), '<div style="left:0">x</div>', {},
                               blockStyle);
    expect(out).toBe('<div name="Hero" style="left:0">x</div>');
  });

  test('works for a bare tag with no other attributes', () => {
    const out = applyLayerName(mockEl({ name: 'Label' }), '<span>hi</span>', {}, blockStyle);
    expect(out).toBe('<span name="Label">hi</span>');
  });

  test('escapes the name value so it cannot break out of the tag', () => {
    const out = applyLayerName(mockEl({ name: 'a "b" <c>' }), '<div>x</div>', {}, blockStyle);
    expect(out).toContain('name="a &quot;b&quot; &lt;c&gt;"');
    expect(out).not.toContain('<c>');
  });

  test('leaves markup untouched when the element has no name', () => {
    const html = '<div style="left:0">x</div>';
    expect(applyLayerName(mockEl({}), html, {}, blockStyle)).toBe(html);
  });

  test('leaves empty markup untouched', () => {
    expect(applyLayerName(mockEl({ name: 'Hero' }), '', {}, blockStyle)).toBe('');
  });

  test('injects safely when the outer style contains a raw > (inline-SVG data URI)', () => {
    const html =
      `<div style="background-image:url('data:image/svg+xml,<svg><rect/></svg>')">x</div>`;
    const out = applyLayerName(mockEl({ name: 'Hero' }), html, {}, blockStyle);
    expect(out).toBe(
      `<div name="Hero" style="background-image:url('data:image/svg+xml,<svg><rect/></svg>')">x</div>`);
  });

  test('defers to the outer call on stripped-transform re-entry', () => {
    const html = '<div style="left:0">x</div>';
    expect(applyLayerName(mockEl({ name: 'Hero' }), html, { _strippedTransform: true },
                          blockStyle)).toBe(html);
  });

  test('skips display:contents hosts (they emit children, not their own box)', () => {
    const html = '<div>child</div>';
    expect(applyLayerName(mockEl({ name: 'Hero' }), html, {}, { display: 'contents' })).toBe(html);
  });

  test('tolerates a missing element', () => {
    expect(applyLayerName(null, '<div>x</div>', {}, blockStyle)).toBe('<div>x</div>');
  });
});
