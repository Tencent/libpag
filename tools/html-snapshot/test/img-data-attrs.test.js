'use strict';

// Regression tests for `forwardDataAttrs`: the snapshot rebuilds every element
// from scratch (it never `cloneNode`s the live tree into the output), so author
// `data-*` attributes on an `<img>` were dropped and never reached the PAGX
// importer's `customData` forwarding. `renderImg` now re-emits them via this
// helper. The snapshot pipeline's own bookkeeping attributes (`data-snapshot-*`,
// `data-pagx-*`) are internal plumbing and must NOT leak into the output.

const { forwardDataAttrs } = require('../dist/lib/browser-snapshot');

// Minimal stand-in for a DOM element exposing just the attribute-reflection
// surface `forwardDataAttrs` touches.
function mockEl(attrs) {
  return {
    getAttributeNames: () => Object.keys(attrs),
    getAttribute: (name) => (name in attrs ? attrs[name] : null),
  };
}

describe('forwardDataAttrs', () => {
  test('forwards author data-* attributes with a leading space', () => {
    const out = forwardDataAttrs(mockEl({ 'data-id': 'hero', 'data-role': 'cover' }));
    expect(out).toBe(' data-id="hero" data-role="cover"');
  });

  test('drops internal data-snapshot-* bookkeeping attributes', () => {
    const out = forwardDataAttrs(mockEl({
      'data-snapshot-src': '/tmp/inlined.png',
      'data-snapshot-canvas-src': 'data:image/png;base64,AAAA',
      'data-id': 'keep',
    }));
    expect(out).toBe(' data-id="keep"');
  });

  test('drops internal data-pagx-* diagnostic attributes', () => {
    const out = forwardDataAttrs(mockEl({ 'data-pagx-transform-dropped': 'rotate(2deg)' }));
    expect(out).toBe('');
  });

  test('ignores non-data attributes (src / alt / style)', () => {
    const out = forwardDataAttrs(mockEl({ src: 'logo.png', alt: 'Logo', style: 'width:1px' }));
    expect(out).toBe('');
  });

  test('escapes attribute values so they cannot break out of the tag', () => {
    const out = forwardDataAttrs(mockEl({ 'data-label': 'a "b" <c>' }));
    expect(out).toContain('data-label="a &quot;b&quot; &lt;c&gt;"');
    expect(out).not.toContain('<c>');
  });

  test('returns an empty string for an element with no data-* attributes', () => {
    expect(forwardDataAttrs(mockEl({}))).toBe('');
  });

  test('tolerates a missing element', () => {
    expect(forwardDataAttrs(null)).toBe('');
    expect(forwardDataAttrs({})).toBe('');
  });
});
