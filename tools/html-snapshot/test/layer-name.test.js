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

  // Wrapped inline box: the renderer emits `${fragments}<div data-pagx-name-anchor …>children`,
  // where the leading tag is a visuals-only line fragment. The name must land on the
  // child-bearing wrapper (the anchor), not the fragment, or `layer->name` strands on a
  // childless box.
  test('targets the anchor-marked wrapper, not the leading line fragment', () => {
    const html =
      '<div style="frag1"></div><div style="frag2"></div>' +
      '<div data-pagx-name-anchor style="wrapper">child</div>';
    const out = applyLayerName(mockEl({ name: 'Mark' }), html, {}, blockStyle);
    expect(out).toBe(
      '<div style="frag1"></div><div style="frag2"></div>' +
      '<div name="Mark" style="wrapper">child</div>');
    // The leading fragment stays unnamed.
    expect(out.startsWith('<div style="frag1">')).toBe(true);
  });

  test('strips the anchor marker when the element has no name', () => {
    const html = '<div style="frag"></div><div data-pagx-name-anchor style="wrapper">child</div>';
    expect(applyLayerName(mockEl({}), html, {}, blockStyle)).toBe(
      '<div style="frag"></div><div style="wrapper">child</div>');
  });

  test('strips the anchor marker on display:contents hosts', () => {
    const html = '<div data-pagx-name-anchor style="wrapper">child</div>';
    expect(applyLayerName(mockEl({ name: 'Mark' }), html, {}, { display: 'contents' })).toBe(
      '<div style="wrapper">child</div>');
  });

  test('strips the anchor marker on stripped-transform re-entry', () => {
    const html = '<div data-pagx-name-anchor style="wrapper">child</div>';
    expect(applyLayerName(mockEl({ name: 'Mark' }), html, { _strippedTransform: true },
                          blockStyle)).toBe('<div style="wrapper">child</div>');
  });
});
