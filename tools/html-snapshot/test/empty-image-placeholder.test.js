'use strict';

const {
  normalizeEmptyImagePlaceholders,
  restoreEmptyImagePlaceholders,
  imgAlt,
} = require('../dist/lib/browser-snapshot');

function mockImage({ src, srcset, alt, rectWithAlt, rectWithoutAlt }) {
  const attrs = new Map();
  if (src !== undefined) attrs.set('src', src);
  if (srcset !== undefined) attrs.set('srcset', srcset);
  if (alt !== undefined) attrs.set('alt', alt);
  function makeImage(clonedAttrs) {
    return {
      _attrs: clonedAttrs,
      hasAttribute(name) {
        return this._attrs.has(name);
      },
      getAttribute(name) {
        return this._attrs.has(name) ? this._attrs.get(name) : null;
      },
      setAttribute(name, value) {
        this._attrs.set(name, String(value));
      },
      removeAttribute(name) {
        this._attrs.delete(name);
      },
      cloneNode() {
        return makeImage(new Map(this._attrs));
      },
      getBoundingClientRect() {
        return this._attrs.get('alt') ? rectWithAlt : rectWithoutAlt;
      },
    };
  }
  return makeImage(attrs);
}

function rootOf(...images) {
  const root = {
    images,
    querySelectorAll(selector) {
      return selector === 'img' ? this.images : [];
    },
    replaceChild(next, previous) {
      const index = this.images.indexOf(previous);
      if (index === -1) throw new Error('child not found');
      this.images[index] = next;
      previous.parentNode = null;
      next.parentNode = this;
      return previous;
    },
  };
  for (const image of images) image.parentNode = root;
  return root;
}

describe('source-less image placeholder layout normalisation', () => {
  test.each([
    ['empty src + percentage size', {
      src: '', alt: 'Hero',
      rectWithAlt: { width: 36, height: 18 },
      rectWithoutAlt: { width: 300, height: 200 },
    }],
    ['missing src + fixed size', {
      alt: 'Avatar',
      rectWithAlt: { width: 48, height: 18 },
      rectWithoutAlt: { width: 80, height: 60 },
    }],
  ])('uses the authored box for %s and preserves the output alt', (_name, init) => {
    const image = mockImage(init);
    const root = rootOf(image);

    expect(normalizeEmptyImagePlaceholders(root)).toBe(1);
    const probe = root.querySelectorAll('img')[0];
    expect(probe).not.toBe(image);
    expect(probe.getAttribute('alt')).toBe('');
    expect(imgAlt(probe)).toBe(init.alt);

    expect(restoreEmptyImagePlaceholders(root)).toBe(1);
    expect(root.querySelectorAll('img')[0]).toBe(image);
    expect(image.getAttribute('alt')).toBe(init.alt);
    expect(imgAlt(image)).toBe(init.alt);
  });

  test('keeps fallback text for a genuinely unsized placeholder', () => {
    const image = mockImage({
      src: '', alt: 'No dimensions',
      rectWithAlt: { width: 96, height: 18 },
      rectWithoutAlt: { width: 0, height: 0 },
    });

    const root = rootOf(image);
    expect(normalizeEmptyImagePlaceholders(root)).toBe(0);
    expect(image.getAttribute('alt')).toBe('No dimensions');
    expect(imgAlt(image)).toBe('No dimensions');
    expect(root.querySelectorAll('img')[0]).toBe(image);
  });

  test.each([
    ['a real src', { src: 'hero.png', alt: 'Hero' }],
    ['a real srcset', { src: '', srcset: 'hero@2x.png 2x', alt: 'Hero' }],
    ['an empty alt', { src: '', alt: '' }],
    ['a missing alt', { src: '' }],
  ])('does not alter %s', (_name, attrs) => {
    const image = mockImage({
      ...attrs,
      rectWithAlt: { width: 36, height: 18 },
      rectWithoutAlt: { width: 300, height: 200 },
    });
    const originalAlt = image.getAttribute('alt');

    expect(normalizeEmptyImagePlaceholders(rootOf(image))).toBe(0);
    expect(image.getAttribute('alt')).toBe(originalAlt);
    expect(Object.prototype.hasOwnProperty.call(
      image, '__pagxSnapshotImagePlaceholderState')).toBe(false);
  });

  test('is idempotent while the placeholder remains normalised', () => {
    const image = mockImage({
      src: '', alt: 'Hero',
      rectWithAlt: { width: 36, height: 18 },
      rectWithoutAlt: { width: 300, height: 200 },
    });
    const root = rootOf(image);

    expect(normalizeEmptyImagePlaceholders(root)).toBe(1);
    expect(normalizeEmptyImagePlaceholders(root)).toBe(0);
    expect(restoreEmptyImagePlaceholders(root)).toBe(1);
    expect(restoreEmptyImagePlaceholders(root)).toBe(0);
  });

  test('restores the exact empty src/srcset attribute state', () => {
    const image = mockImage({
      src: '  ', srcset: '', alt: 'Hero',
      rectWithAlt: { width: 36, height: 18 },
      rectWithoutAlt: { width: 300, height: 200 },
    });
    const root = rootOf(image);

    expect(normalizeEmptyImagePlaceholders(root)).toBe(1);
    const probe = root.querySelectorAll('img')[0];
    expect(probe.hasAttribute('src')).toBe(true);
    expect(probe.getAttribute('src')).toBe('  ');
    expect(probe.hasAttribute('srcset')).toBe(true);
    expect(probe.getAttribute('srcset')).toBe('');

    expect(restoreEmptyImagePlaceholders(root)).toBe(1);
    expect(root.querySelectorAll('img')[0]).toBe(image);
    expect(image.hasAttribute('src')).toBe(true);
    expect(image.getAttribute('src')).toBe('  ');
    expect(image.hasAttribute('srcset')).toBe(true);
    expect(image.getAttribute('srcset')).toBe('');
  });
});
