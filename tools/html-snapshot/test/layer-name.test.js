'use strict';

// Regression tests for `applyLayerAttributes`: the snapshot rebuilds every element from
// computed style rather than cloning the live tree, so an author `name` attribute is
// dropped unless it is re-emitted here. `render` folds it onto the outer box the element
// produced so the PAGX importer can surface it as `layer->name` (mirroring `id`).

const { applyLayerAttributes } = require('../dist/lib/browser-snapshot');

// Minimal stand-in for a DOM element exposing just the surface `applyLayerAttributes` touches.
function mockEl(attrs) {
  return {
    getAttribute: (name) => (name in attrs ? attrs[name] : null),
  };
}

// A non-`contents` computed style keeps `applyLayerAttributes` off the `getComputedStyle`
// browser global so the helper is exercisable under Node.
const blockStyle = { display: 'block' };

describe('applyLayerAttributes', () => {
  test('injects the name attribute into the emitted outer box', () => {
    const out = applyLayerAttributes(mockEl({ name: 'Hero' }), '<div style="left:0">x</div>', {},
                               blockStyle);
    expect(out).toBe('<div name="Hero" style="left:0">x</div>');
  });

  test('works for a bare tag with no other attributes', () => {
    const out = applyLayerAttributes(mockEl({ name: 'Label' }), '<span>hi</span>', {}, blockStyle);
    expect(out).toBe('<span name="Label">hi</span>');
  });

  test('escapes the name value so it cannot break out of the tag', () => {
    const out = applyLayerAttributes(mockEl({ name: 'a "b" <c>' }), '<div>x</div>', {}, blockStyle);
    expect(out).toContain('name="a &quot;b&quot; &lt;c&gt;"');
    expect(out).not.toContain('<c>');
  });

  test('leaves markup untouched when the element has no name', () => {
    const html = '<div style="left:0">x</div>';
    expect(applyLayerAttributes(mockEl({}), html, {}, blockStyle)).toBe(html);
  });

  test('leaves empty markup untouched', () => {
    expect(applyLayerAttributes(mockEl({ name: 'Hero' }), '', {}, blockStyle)).toBe('');
  });

  test('injects safely when the outer style contains a raw > (inline-SVG data URI)', () => {
    const html =
      `<div style="background-image:url('data:image/svg+xml,<svg><rect/></svg>')">x</div>`;
    const out = applyLayerAttributes(mockEl({ name: 'Hero' }), html, {}, blockStyle);
    expect(out).toBe(
      `<div name="Hero" style="background-image:url('data:image/svg+xml,<svg><rect/></svg>')">x</div>`);
  });

  test('defers to the outer call on stripped-transform re-entry', () => {
    const html = '<div style="left:0">x</div>';
    expect(applyLayerAttributes(mockEl({ name: 'Hero' }), html, { _strippedTransform: true },
                          blockStyle)).toBe(html);
  });

  test('skips display:contents hosts (they emit children, not their own box)', () => {
    const html = '<div>child</div>';
    expect(applyLayerAttributes(mockEl({ name: 'Hero' }), html, {}, { display: 'contents' })).toBe(html);
  });

  test('tolerates a missing element', () => {
    expect(applyLayerAttributes(null, '<div>x</div>', {}, blockStyle)).toBe('<div>x</div>');
  });

  // Wrapped inline box: the renderer emits `${fragments}<div data-pagx-name-anchor …>children`,
  // where the leading tag is a visuals-only line fragment. The name must land on the
  // child-bearing wrapper (the anchor), not the fragment, or `layer->name` strands on a
  // childless box.
  test('targets the anchor-marked wrapper, not the leading line fragment', () => {
    const html =
      '<div style="frag1"></div><div style="frag2"></div>' +
      '<div data-pagx-name-anchor style="wrapper">child</div>';
    const out = applyLayerAttributes(mockEl({ name: 'Mark' }), html, {}, blockStyle);
    expect(out).toBe(
      '<div style="frag1"></div><div style="frag2"></div>' +
      '<div name="Mark" style="wrapper">child</div>');
    // The leading fragment stays unnamed.
    expect(out.startsWith('<div style="frag1">')).toBe(true);
  });

  test('strips the anchor marker when the element has no name', () => {
    const html = '<div style="frag"></div><div data-pagx-name-anchor style="wrapper">child</div>';
    expect(applyLayerAttributes(mockEl({}), html, {}, blockStyle)).toBe(
      '<div style="frag"></div><div style="wrapper">child</div>');
  });

  test('strips the anchor marker on display:contents hosts', () => {
    const html = '<div data-pagx-name-anchor style="wrapper">child</div>';
    expect(applyLayerAttributes(mockEl({ name: 'Mark' }), html, {}, { display: 'contents' })).toBe(
      '<div style="wrapper">child</div>');
  });

  test('strips the anchor marker on stripped-transform re-entry', () => {
    const html = '<div data-pagx-name-anchor style="wrapper">child</div>';
    expect(applyLayerAttributes(mockEl({ name: 'Mark' }), html, { _strippedTransform: true },
                          blockStyle)).toBe('<div style="wrapper">child</div>');
  });
});

describe('original text metadata', () => {
  test('preserves only data-pagx-text and keeps rendered glyph content', () => {
    const out = applyLayerAttributes(mockEl({
      'data-pagx-text': '萌宝鞋屋 "<&',
      'data-pagx-font-family': 'OriginalFont',
      'data-pagx-text-part': '1',
      'data-pagx-faux-bold': '1',
    }), '<span>\uE000</span>', {}, blockStyle);
    expect(out).toBe('<span data-pagx-text="萌宝鞋屋 &quot;&lt;&amp;">\uE000</span>');
  });

  test('preserves empty original text values', () => {
    expect(applyLayerAttributes(mockEl({ 'data-pagx-text': '' }), '<div>x</div>', {},
      blockStyle)).toBe('<div data-pagx-text="">x</div>');
  });

  test('puts original text on the anchored group with its name', () => {
    const html = '<div>fragment</div><div data-pagx-name-anchor>glyphs</div>';
    expect(applyLayerAttributes(mockEl({ name: 'Label', 'data-pagx-text': '搜索' }), html,
      {}, blockStyle)).toBe('<div>fragment</div><div name="Label" data-pagx-text="搜索">glyphs</div>');
  });

  test('does not duplicate original text during transform re-entry', () => {
    const html = '<div>glyphs</div>';
    expect(applyLayerAttributes(mockEl({ 'data-pagx-text': '搜索' }), html,
      { _strippedTransform: true }, blockStyle)).toBe(html);
  });
});
