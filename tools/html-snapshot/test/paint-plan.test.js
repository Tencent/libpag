/** @jest-environment jsdom */
'use strict';

const {
  paintStyleRecord, planPaintContext, beginPaintCapture, capturePaintGeometry,
  markPaintSource, markPaintDecoration, markPaintSurface, paintNeedsSeparateChildren,
  compilePaintMarkup, endPaintCapture,
} = require('../dist/lib/paint-plan');
const { computedStyle, setRect } = require('./dom-helpers');

let styles;
beforeEach(() => {
  document.head.innerHTML = '';
  document.body.innerHTML = '';
  styles = new WeakMap();
  jest.spyOn(window, 'getComputedStyle').mockImplementation(el => computedStyle(el, styles.get(el)));
  const matches = Element.prototype.matches;
  jest.spyOn(Element.prototype, 'matches').mockImplementation(function (selector) {
    if (selector === ':modal' || selector === ':popover-open') return this.dataset.topLayer === selector;
    return matches.call(this, selector);
  });
});
afterEach(() => {
  endPaintCapture();
  jest.restoreAllMocks();
});

function record(css = {}, flexItem = false, transformable = true, pseudo) {
  return paintStyleRecord(computedStyle(document.createElement('div'), css), pseudo, flexItem, transformable);
}

describe('paintStyleRecord', () => {
  test('normal flow does not establish a context or containing block', () => {
    expect(record()).toMatchObject({
      display: 'block', position: 'static', positioned: false, zIndex: null, context: false,
      flexItem: false, paintContainment: false, floating: false, order: 0, clips: false,
      absContainingBlock: false, fixedContainingBlock: false, preserve3D: false,
    });
  });

  test.each([
    [{ position: 'relative', zIndex: '-2' }, false, -2, true],
    [{ position: 'relative', zIndex: 'auto' }, false, null, false],
    [{ position: 'absolute', zIndex: '0' }, false, 0, true],
    [{ zIndex: '5' }, false, null, false],
    [{ zIndex: '5' }, true, 5, true],
    [{ zIndex: '1.5' }, true, null, false],
    [{ position: 'relative', zIndex: 'NaN' }, false, null, false],
  ])('applies z-index only to positioned boxes or flex/grid items: %j', (css, flex, zIndex, context) => {
    expect(record(css, flex)).toMatchObject({ zIndex, context });
  });

  test.each([
    ['position', 'fixed'], ['position', 'sticky'], ['opacity', '0.5'],
    ['transform', 'scale(2)'], ['translate', '2px'], ['rotate', '5deg'],
    ['scale', '2'], ['perspective', '200px'], ['filter', 'blur(2px)'],
    ['backdrop-filter', 'blur(2px)'], ['-webkit-backdrop-filter', 'blur(2px)'],
    ['clip-path', 'inset(2px)'], ['mask-image', 'url(mask.png)'],
    ['-webkit-mask-image', 'url(mask.png)'], ['isolation', 'isolate'],
    ['mix-blend-mode', 'multiply'], ['contain', 'layout'], ['contain', 'paint'],
    ['contain', 'content'], ['contain', 'strict'], ['content-visibility', 'auto'],
    ['content-visibility', 'hidden'], ['will-change', 'opacity, transform'],
    ['will-change', 'filter'], ['will-change', 'backdrop-filter'],
  ])('%s: %s creates a stacking context', (property, value) => {
    expect(record({ [property]: value }).context).toBe(true);
  });

  test.each(['none', 'contents'])('display:%s has no box to clip or establish containing blocks', display => {
    expect(record({ display, position: 'absolute', transform: 'scale(2)', 'overflow-x': 'hidden' }))
      .toMatchObject({ context: false, clips: false, absContainingBlock: false, fixedContainingBlock: false });
  });

  test('non-transformable inline boxes ignore transforms, layout containment and will-change transforms', () => {
    expect(record({ transform: 'scale(2)', contain: 'paint', 'will-change': 'transform' }, false, false))
      .toMatchObject({ context: false, absContainingBlock: false, fixedContainingBlock: false, clips: false });
    expect(record({ 'will-change': 'z-index' })).toMatchObject({ context: false });
    expect(record({ position: 'relative', 'will-change': 'z-index' })).toMatchObject({ context: true });
    expect(record({ 'will-change': 'z-index' }, true)).toMatchObject({ context: true });
  });

  test('separates clipping, layout containment and fixed-position containing blocks', () => {
    expect(record({ position: 'relative', 'overflow-y': 'hidden', float: 'left', order: '-2', 'transform-style': 'preserve-3d' }))
      .toMatchObject({ context: false, clips: true, floating: true, order: -2, preserve3D: true,
        absContainingBlock: true, fixedContainingBlock: false });
    expect(record({ contain: 'layout' })).toMatchObject({ paintContainment: false, clips: false, fixedContainingBlock: true });
    expect(record({ contain: 'paint' })).toMatchObject({ paintContainment: true, clips: true, fixedContainingBlock: true });
    expect(record({}, false, true, { position: 'absolute', zIndex: ' 3 ' }))
      .toMatchObject({ position: 'absolute', zIndex: 3, context: true });
  });
});

function start(html) {
  document.body.innerHTML = html;
  for (const el of [document.body, ...document.body.querySelectorAll('*')]) {
    setRect(el, { width: parseFloat(el.style.width) || 100, height: parseFloat(el.style.height) || 20 });
  }
  beginPaintCapture(document.body);
  for (const el of [document.body, ...document.body.querySelectorAll('*')]) {
    capturePaintGeometry(el, el.parentElement?.getBoundingClientRect() || { left: 0, top: 0 });
  }
}

// Serialize a measured tree through the same public marker/compiler boundary
// as browser-snapshot. Tests inspect the emitted DOM rather than helper calls.
function markup(el) {
  if (el.nodeType !== 1) return el.textContent;
  const clone = el.cloneNode(false);
  clone.innerHTML = Array.from(el.childNodes, markup).join('');
  if (el.tagName === 'IMG') return markPaintSource(el, `<div>${clone.outerHTML}</div>`);
  return markPaintSource(el, clone.outerHTML);
}

function compile(html = Array.from(document.body.childNodes, markup).join('')) {
  const output = document.createElement('div');
  output.innerHTML = compilePaintMarkup(html);
  return output;
}
function leaves(output) { return Array.from(output.querySelectorAll('[data-leaf]'), el => el.dataset.leaf); }

describe('paint capture and markup', () => {
  test('markers are inert outside capture and are removed from compiled XML-compatible output', () => {
    const el = document.createElement('div');
    expect(markPaintSource(el, '<div/>')).toBe('<div/>');
    expect(markPaintDecoration('<div/>')).toBe('<div/>');
    expect(markPaintSurface('<img/>')).toBe('<img/>');
    expect(paintNeedsSeparateChildren(el)).toBe(false);
    expect(() => capturePaintGeometry(el, {})).not.toThrow();
    start('<div id="host"><img src="data:image/png;base64,AA==">\u00a0x</div>');
    const host = document.querySelector('#host');
    expect(markPaintSource(host, '<span data-pagx-name-anchor="true"></span>')).toContain('data-pagx-name-anchor data-pagx-paint-source="1"');
    expect(markPaintSource(host, '', {})).toBe('');
    expect(markPaintSource(host, '<div/>', { _strippedTransform: true })).toBe('<div/>');
    expect(markPaintSurface('')).toBe('');
    const html = compilePaintMarkup(markup(host));
    expect(html).toMatch(/<img\b[^>]*\/>/);
    expect(html).not.toContain('&nbsp;');
    expect(html).not.toMatch(/data-pagx-paint-(source|surface|decoration)=/);
    expect(html).toContain('data-pagx-paint-order="fixed"');
    endPaintCapture();
    expect(markPaintSource(host, '<div/>')).toBe('<div/>');
    expect(endPaintCapture()).toEqual([]);
  });

  test('skips contents wrappers and SVG/hidden subtrees while propagating escaping children', () => {
    start('<div id="contents" style="display:contents"><div id="moving" style="position:absolute">text</div></div>' +
      '<svg><g id="svg-child"></g></svg><div style="display:none"><span id="hidden-child"></span></div>');
    expect(markPaintSource(document.querySelector('#contents'), '<div/>')).toBe('<div/>');
    expect(markPaintSource(document.querySelector('#svg-child'), '<g/>')).toBe('<g/>');
    expect(markPaintSource(document.querySelector('#hidden-child'), '<span/>')).toBe('<span/>');
    expect(paintNeedsSeparateChildren(document.body)).toBe(true);
    expect(compile().textContent).toBe('text');
  });

  test('reports unsupported 3D, top layer and animated stacking and resets warnings between captures', () => {
    document.head.innerHTML = '<style id="__pagx_anim_keyframes">' +
      '@keyframes depth { from { z-index:1; opacity:0; transform:scale(1) } to { z-index:2; opacity:1; transform:scale(2) } }' +
      'div { color:red }</style>';
    start('<div id="animated" style="animation-name:depth;transform-style:preserve-3d" data-top-layer=":modal"><span data-leaf="a">a</span></div>' +
      '<div data-top-layer=":popover-open"></div>');
    expect(compile().querySelector('#animated').style.isolation).toBe('isolate');
    const warnings = endPaintCapture();
    expect(warnings).toEqual([
      expect.stringContaining('paint:preserve-3d: <div>#animated'),
      expect.stringContaining('paint:animated-stacking: <div>#animated'),
      expect.stringContaining('paint:top-layer: <div>#animated'),
      expect.stringContaining('paint:top-layer: <div>'),
    ]);
    start('<p>plain</p>');
    expect(endPaintCapture()).toEqual([]);
  });

  test('empty markup and whitespace produce no output', () => {
    start('');
    expect(compilePaintMarkup(' \n ')).toBe('');
  });

  test('marker-free generated content can be serialized without an active capture', () => {
    const output = compile('<span>generated</span>');
    expect(output.textContent).toBe('generated');
    expect(output.querySelector('span').getAttribute('data-pagx-paint-order')).toBe('fixed');
  });

  test.each(['div', 'span'])('paints a native surface before the contents of a %s', tag => {
    start(`<${tag} id="control">label</${tag}>`);
    const el = document.querySelector('#control');
    const surface = markPaintSurface('<img data-leaf="surface" src="native.png"/>');
    const output = compile(markPaintSource(el, `<${tag}>${surface}<b data-leaf="text">label</b></${tag}>`));
    expect(leaves(output)).toEqual(['surface', 'text']);
    expect(output.textContent).toBe('label');
  });
});

describe('stacking and flow order', () => {
  test('sorts negative, flow, zero and positive phases with stable equal-z tree order', () => {
    start('<span data-leaf="inline">inline</span>' +
      '<div style="position:absolute;z-index:5" data-leaf="high">high</div>' +
      '<div style="position:absolute;z-index:-1" data-leaf="negative">negative</div>' +
      '<div style="position:relative;z-index:0" data-leaf="zero">zero</div>' +
      '<div style="position:absolute;z-index:-3" data-leaf="lowest">lowest</div>' +
      '<div style="position:absolute;z-index:5" data-leaf="equal">equal</div>');
    expect(leaves(compile())).toEqual(['lowest', 'negative', 'inline', 'zero', 'high', 'equal']);
  });

  test('context children cannot escape but descendants of z-index:auto can', () => {
    start('<div id="isolated" style="opacity:0.5"><span style="position:absolute;z-index:99" data-leaf="trapped">trapped</span></div>' +
      '<div id="auto" style="position:relative"><span style="position:absolute;z-index:9" data-leaf="escaped">escaped</span></div>' +
      '<span style="position:absolute;z-index:2" data-leaf="cover">cover</span>');
    const output = compile();
    expect(leaves(output)).toEqual(['trapped', 'cover', 'escaped']);
    expect(output.querySelector('#isolated').style.isolation).toBe('isolate');
    expect(output.querySelector('#isolated').style.opacity).toBe('0.5');
    expect(output.querySelectorAll('#auto')).toHaveLength(1);
  });

  test('block decorations precede floats and inline content; synthetic surfaces precede negative children', () => {
    start('<div id="block" style="background:red"><span data-leaf="inline">inline</span></div>' +
      '<div style="float:left;background:blue"><span data-leaf="float">float</span></div>' +
      '<div style="position:absolute;z-index:-1" data-leaf="negative">negative</div>');
    const surface = markPaintSurface('<img data-leaf="surface" src="surface.png"/>');
    const decoration = markPaintDecoration('<div data-leaf="decoration"></div>');
    const output = compile(surface + decoration + Array.from(document.body.childNodes, markup).join(''));
    expect(leaves(output)).toEqual(['surface', 'negative', 'decoration', 'float', 'inline']);
    expect(output.querySelectorAll('#block')).toHaveLength(1);
    expect(output.querySelector('#block').style.background).toBe('');
    expect(output.querySelector('[style*="background: red"]')).not.toBeNull();
  });

  test('orders flex/grid items by order and uses tree order for ties', () => {
    start('<div style="display:grid">' +
      '<span style="order:2" data-leaf="last">last</span>' +
      '<span style="order:-1" data-leaf="first">first</span>' +
      '<span style="order:2" data-leaf="tie">tie</span>' +
      '<span style="position:absolute;order:-10" data-leaf="positioned">positioned</span></div>');
    expect(leaves(compile())).toEqual(['first', 'last', 'tie', 'positioned']);
  });

  test.each(['', 'position:relative;', 'opacity:0.5;'])('preserves an intact flex subtree: %s', prefix => {
    start(`<div id="flex" style="${prefix}display:flex;background:red"><span data-leaf="one">one</span><span data-leaf="two">two</span></div>`);
    expect(paintNeedsSeparateChildren(document.querySelector('#flex'))).toBe(false);
    const output = compile();
    expect(leaves(output)).toEqual(['one', 'two']);
    expect(output.querySelectorAll('#flex')).toHaveLength(1);
    expect(output.querySelector('#flex').style.display).toBe('flex');
    expect(output.querySelector('#flex').textContent).toBe('onetwo');
  });

  test('uses measured coordinates when a flex item escapes its original flow', () => {
    start('<div style="display:flex"><div id="item" style="position:relative;flex:1;margin:12px;background:red"><span style="position:absolute" data-leaf="child">child</span></div></div>');
    const item = document.querySelector('#item');
    setRect(item, { left: 25, top: 35, width: 80, height: 40 });
    capturePaintGeometry(item, { left: 10, top: 20 });
    const clone = compile().querySelector('#item');
    expect(clone.style.position).toBe('absolute');
    expect(clone.style.left).toBe('15px');
    expect(clone.style.top).toBe('15px');
    expect(clone.style.width).toBe('80px');
    expect(clone.style.height).toBe('40px');
    expect(clone.style.margin).toBe('0px');
    expect(clone.style.flex).toBe('');
  });

  test('inline backgrounds clipped to text remain on their content wrapper', () => {
    start('<span id="text" style="display:inline;background:red;background-clip:text;box-shadow:0px 0px 2px red">text</span>');
    const output = compile();
    expect(output.querySelector('#text').style.background).toBe('red');
    expect(output.textContent).toBe('text');
  });
});

describe('overflow and containing blocks', () => {
  test('splits inline decorations without duplicating identity or border paint', () => {
    start('<span id="inline" name="label" style="background:blue;border-left:2px solid red;outline:1px solid black;box-shadow:0px 0px 2px red">' +
      '<b data-leaf="first">first</b><i data-leaf="second">second</i></span>');
    const output = compile();
    expect(leaves(output)).toEqual(['first', 'second']);
    expect(output.querySelectorAll('#inline')).toHaveLength(1);
    expect(output.querySelectorAll('[name=label]')).toHaveLength(1);
    const content = output.querySelector('#inline');
    expect(content.style.background).toBe('');
    expect(content.style.outline).toBe('');
    expect(content.style.boxShadow).toBe('');
    expect(content.style.borderLeftWidth).toBe('2px');
    expect(content.style.borderLeftColor).toBe('transparent');
    expect(output.firstElementChild.style.background).toBe('blue');
  });

  test('clips renderer-generated image content to the padding edge with scaled elliptical radii', () => {
    start('<div id="clip" style="position:relative;z-index:0;overflow-x:hidden;border:4px solid red;' +
      'width:100px;height:80px;border-top-left-radius:80% 50%;border-top-right-radius:80% 50%;' +
      'border-bottom-right-radius:80% 50%;border-bottom-left-radius:80% 50%"></div>');
    const source = document.querySelector('#clip');
    const wrapper = source.cloneNode(false);
    wrapper.innerHTML = '<img src="generated.png" data-leaf="image"/>';
    const output = compile(markPaintSource(source, wrapper.outerHTML));
    const image = output.querySelector('img');
    const clip = image.closest('[style*="overflow: hidden"]');
    expect(clip).not.toBeNull();
    expect(clip.style.width).toBe('92px');
    expect(clip.style.height).toBe('72px');
    expect(clip.style.borderTopLeftRadius).toBe('46px 21px');
    expect(output.querySelectorAll('#clip')).toHaveLength(1);
  });

  test('absolute content escapes an intervening overflow clip while in-flow content remains clipped', () => {
    start('<div id="clip" style="overflow-x:hidden;overflow-y:hidden;border:4px solid red;border-radius:20%;width:100px;height:80px;background:blue">' +
      '<span data-leaf="flow">flow</span><span style="position:absolute;z-index:1" data-leaf="absolute">absolute</span></div>');
    const output = compile();
    const flow = output.querySelector('[data-leaf=flow]');
    const absolute = output.querySelector('[data-leaf=absolute]');
    expect(flow.parentElement.style.width).toBe('92px');
    expect(flow.parentElement.style.height).toBe('72px');
    expect(flow.parentElement.style.borderTopWidth).toBe('0px');
    expect(absolute.parentElement.style.overflow).toBe('visible');
    expect(output.querySelectorAll('#clip')).toHaveLength(1);
    expect(leaves(output)).toEqual(['flow', 'absolute']);
  });

  test('fixed content keeps clips only when a transform establishes its containing block', () => {
    for (const transform of ['', 'transform:scale(1);']) {
      start(`<div style="${transform}overflow-x:hidden;width:100px;height:80px"><span style="position:fixed" data-leaf="fixed">fixed</span></div>`);
      const fixed = compile().querySelector('[data-leaf=fixed]');
      const ancestors = [];
      for (let el = fixed.parentElement; el; el = el.parentElement) ancestors.push(el.style.overflow);
      expect(ancestors.includes('hidden')).toBe(Boolean(transform));
      endPaintCapture();
    }
  });

  test('mixed clipping within a context applies inherited clips to its own decoration and flow only', () => {
    start('<div id="clip" style="overflow-x:hidden;border:2px solid black;width:100px;height:80px;opacity:0.8">' +
      '<div id="group" style="opacity:0.5;background:red;animation-name:fade">' +
      '<span data-leaf="flow">flow</span><span style="position:absolute" data-leaf="escape">escape</span></div></div>');
    const output = compile();
    expect(leaves(output)).toEqual(['flow', 'escape']);
    expect(output.querySelectorAll('#clip')).toHaveLength(1);
    expect(output.querySelectorAll('#group')).toHaveLength(1);
    const group = output.querySelector('#group');
    expect(group.style.opacity).toBe('0.5');
    expect(group.style.background).toBe('');
    const decoration = group.querySelector('[style*="background: red"]');
    expect(decoration).not.toBeNull();
    expect(decoration.style.opacity).toBe('');
    expect(decoration.style.animationName).toBe('');
    expect(decoration.hasAttribute('id')).toBe(false);
    expect(output.querySelector('[data-leaf=escape]').parentElement).toBe(group);
  });

  test('paint containment adds a clip even without explicit overflow', () => {
    start('<div style="contain:paint"><span data-leaf="flow">flow</span></div>');
    const flow = compile().querySelector('[data-leaf=flow]');
    expect(flow.closest('[style*="overflow: hidden"]')).not.toBeNull();
  });
});

test('planPaintContext accepts generated text atoms without authored sources', () => {
  const text = { el: document.createTextNode('text'), source: null, parent: null, children: [], atomic: true, order: 0 };
  const root = { el: document.createElement('div'), children: [text] };
  expect(planPaintContext(root)).toEqual([{ box: text, kind: 'atom' }]);
});
