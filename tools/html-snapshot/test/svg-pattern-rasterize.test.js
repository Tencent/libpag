// Copyright (C) 2026 Tencent. All rights reserved.

'use strict';

// `inlineSvgVectorPatterns` normally runs inside `page.evaluate` against a real DOM: it finds
// `<pattern>` defs whose content is vector geometry and replaces that content with a single tile
// `<image>` rasterized through an offscreen `<canvas>`. PAGX can only tile a bitmap, so without
// this the importer drops the pattern and the painted shape loses its texture.
//
// The function is plain DOM/canvas code, so we drive it in node behind minimal `document` /
// `Image` / canvas stubs, the same way mask-image-inline.test.js exercises `inlineMaskImages`.

const { inlineSvgVectorPatterns } = require('../dist/lib/browser-snapshot');

const PNG = 'data:image/png;base64,AAAA';

class FakeNode {
  constructor(tag, attrs = {}, children = []) {
    this.tagName = tag;
    this._attrs = Object.assign({}, attrs);
    this.children = children.slice();
  }

  getAttribute(name) {
    return Object.prototype.hasOwnProperty.call(this._attrs, name) ? this._attrs[name] : null;
  }

  setAttribute(name, value) {
    this._attrs[name] = value;
  }

  // Only the tag matters for the tile markup this pass builds; the attributes are irrelevant to
  // the assertions below.
  get outerHTML() {
    return '<' + this.tagName + '></' + this.tagName + '>';
  }

  get firstChild() {
    return this.children.length ? this.children[0] : null;
  }

  removeChild(child) {
    const i = this.children.indexOf(child);
    if (i >= 0) this.children.splice(i, 1);
  }

  appendChild(child) {
    this.children.push(child);
    return child;
  }
}

class FakeSvg extends FakeNode {
  constructor(defs, patterns) {
    super('svg', {}, [defs].concat(patterns));
    this._defs = defs;
    this._patterns = patterns;
  }

  querySelector(sel) {
    return sel === 'defs' ? this._defs : null;
  }

  querySelectorAll(sel) {
    return sel === 'pattern' ? this._patterns : [];
  }
}

class FakeImage {
  set src(value) {
    this._src = value;
    setTimeout(() => this.onload && this.onload(), 0);
  }

  get src() {
    return this._src;
  }
}

let savedGlobals;

beforeEach(() => {
  savedGlobals = { document: global.document, Image: global.Image };
});

afterEach(() => {
  global.document = savedGlobals.document;
  global.Image = savedGlobals.Image;
});

function makeSvg(patternAttrs, patternChildren) {
  const defs = new FakeNode('defs');
  const pattern = new FakeNode('pattern', patternAttrs, patternChildren);
  const svg = new FakeSvg(defs, [pattern]);
  return { svg, pattern };
}

function run(svgs) {
  global.document = {
    querySelectorAll: (sel) => (sel === 'svg' ? svgs : []),
    createElement: () => ({
      getContext: () => ({ drawImage: () => {} }),
      toDataURL: () => PNG,
    }),
    createElementNS: (ns, tag) => new FakeNode(tag),
  };
  global.Image = FakeImage;
  return inlineSvgVectorPatterns();
}

describe('inlineSvgVectorPatterns', () => {
  test('rasterizes a vector userSpaceOnUse pattern into a tile image', async () => {
    const { svg, pattern } = makeSvg(
      { id: 'wave', width: '40', height: '12', patternUnits: 'userSpaceOnUse' },
      [new FakeNode('path', { d: 'M0 6 q10 -6 20 0' })],
    );
    await run([svg]);

    expect(pattern.children.length).toBe(1);
    const image = pattern.children[0];
    expect(image.tagName).toBe('image');
    expect(image.getAttribute('href')).toBe(PNG);
    expect(image.getAttribute('width')).toBe('40');
    expect(image.getAttribute('height')).toBe('12');
  });

  test('leaves an objectBoundingBox pattern untouched', async () => {
    const { svg, pattern } = makeSvg(
      { id: 'hatch', width: '0.1', height: '0.1', patternUnits: 'objectBoundingBox' },
      [new FakeNode('path', { d: 'M0 0 L1 1' })],
    );
    await run([svg]);

    expect(pattern.children.length).toBe(1);
    expect(pattern.children[0].tagName).toBe('path');
  });

  test('leaves an image-only pattern untouched', async () => {
    const { svg, pattern } = makeSvg(
      { id: 'photo', width: '40', height: '12', patternUnits: 'userSpaceOnUse' },
      [new FakeNode('image', { href: 'data:image/png;base64,BBBB' })],
    );
    await run([svg]);

    expect(pattern.children.length).toBe(1);
    expect(pattern.children[0].getAttribute('href')).toBe('data:image/png;base64,BBBB');
  });

  test('skips a userSpaceOnUse pattern without an explicit size', async () => {
    const { svg, pattern } = makeSvg(
      { id: 'auto', patternUnits: 'userSpaceOnUse' },
      [new FakeNode('path', { d: 'M0 0 L1 1' })],
    );
    await run([svg]);

    expect(pattern.children[0].tagName).toBe('path');
  });
});
