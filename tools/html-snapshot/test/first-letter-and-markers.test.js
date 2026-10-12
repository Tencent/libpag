'use strict';

// `materializeFirstLetter` / `materializeListMarkers` run inside `page.evaluate`
// against a live DOM. Both lift a UA-painted pseudo-element into real markup:
//
//   - `::first-letter` (drop caps) has no DOM node, and its tall float box overlaps
//     the paragraph's first line rects, so the line splitter folds several lines into
//     one run-together span. The pass inserts a real floated <span> carrying the cap
//     and removes the character from the text node.
//   - `::marker` (list numbers / bullets) likewise has no node, so lists lose their
//     markers. Chromium does not expose the marker text (`content` computes to
//     `normal`), so the pass derives it from `list-style-type` + the item index.
//
// Both are plain DOM code, so they are driven here behind minimal `document` /
// `getComputedStyle` / `Element` stubs — the same style as mask-image-inline.test.js.

const {
  materializeFirstLetter,
  materializeListMarkers,
} = require('../dist/lib/browser-snapshot');

class FakeElement {
  constructor(tagName, attrs = {}) {
    this.tagName = tagName.toUpperCase();
    this.attrs = Object.assign({}, attrs);
    this.style = {};
    this.childNodes = [];
    this.children = [];
    this.parentElement = null;
  }
  getAttribute(name) { return name in this.attrs ? this.attrs[name] : null; }
  setAttribute(name, value) { this.attrs[name] = String(value); }
  hasAttribute(name) { return name in this.attrs; }
  closest() { return null; }
  insertBefore(node) { this.childNodes.unshift(node); return node; }
  get firstChild() { return this.childNodes.length ? this.childNodes[0] : null; }
}

function fakeStyle(props) {
  return { getPropertyValue: (p) => (p in props ? props[p] : '') };
}

let saved;

beforeEach(() => {
  saved = {
    document: global.document,
    getComputedStyle: global.getComputedStyle,
    Element: global.Element,
  };
  global.Element = FakeElement;
});

afterEach(() => {
  global.document = saved.document;
  global.getComputedStyle = saved.getComputedStyle;
  global.Element = saved.Element;
});

describe('materializeListMarkers', () => {
  // `<ol>` with three items; the middle one carries an explicit `value`.
  function makeList() {
    const li1 = new FakeElement('li');
    const li2 = new FakeElement('li', { value: '7' });
    const li3 = new FakeElement('li');
    const ol = new FakeElement('ol');
    ol.children = [li1, li2, li3];
    li1.parentElement = li2.parentElement = li3.parentElement = ol;
    const markerStyle = fakeStyle({
      'font-size': '15px', 'font-family': 'Songti SC', color: 'rgb(90, 83, 71)',
      width: '14.0781px', height: '29.25px',
    });
    const listItemStyle = fakeStyle({ 'list-style-type': 'decimal', position: 'static' });
    global.getComputedStyle = (el, pseudo) => (pseudo === '::marker' ? markerStyle : listItemStyle);
    const created = [];
    global.document = {
      querySelectorAll: (sel) => (sel === 'li' ? [li1, li2, li3] : []),
      createElement: () => {
        const el = new FakeElement('span');
        created.push(el);
        return el;
      },
    };
    return { li1, li2, li3, ol, created };
  }

  test('emits one marker per item, honouring <ol start> and per-item value', () => {
    const { li1, li2, li3, created } = makeList();
    materializeListMarkers();

    expect(created.map((el) => el.textContent)).toEqual(['1.', '7.', '8.']);
    for (const li of [li1, li2, li3]) {
      expect(li.firstChild.getAttribute('data-snapshot-marker')).toBe('');
      // A static item becomes the containing block for its absolutely-positioned marker.
      expect(li.style.position).toBe('relative');
    }
  });

  test('styles the marker box from ::marker and pins it to the item-start edge', () => {
    const { created } = makeList();
    materializeListMarkers();
    const style = created[0].getAttribute('style');
    expect(style).toContain('position: absolute');
    expect(style).toContain('right: 100%');
    expect(style).toContain('width: 14.0781px');
    expect(style).toContain('font-size: 15px');
    expect(style).toContain('color: rgb(90, 83, 71)');
  });

  test('skips list-style-type: none and a repeated item', () => {
    const { li1, li2, li3, created } = makeList();
    global.getComputedStyle = (el, pseudo) => (
      pseudo === '::marker'
        ? fakeStyle({ 'font-size': '15px', width: '10px', height: '20px' })
        : fakeStyle({ 'list-style-type': 'none', position: 'static' })
    );
    materializeListMarkers();
    expect(created).toHaveLength(0);
    materializeListMarkers.calls = [];
    expect(li1.firstChild).toBeNull();
    expect(li2.firstChild).toBeNull();
    expect(li3.firstChild).toBeNull();
  });
});

describe('materializeFirstLetter', () => {
  function makeParagraph() {
    const textNode = { nodeType: 3, nodeValue: '当我们在屏幕上读到' };
    const para = new FakeElement('p', { class: 'first' });
    para.childNodes = [textNode];
    const firstLetterStyle = fakeStyle({
      float: 'left', width: '98.3125px', height: '86.3906px',
      'font-size': '96px', 'font-family': 'XiaoWei', color: 'rgb(181, 69, 42)',
      'margin-top': '10px', 'margin-right': '14px',
    });
    const ownStyle = fakeStyle({
      float: 'none', 'font-size': '18px', 'font-family': 'Songti SC', color: 'rgb(35, 32, 27)',
    });
    global.getComputedStyle = (el, pseudo) => (pseudo === '::first-letter' ? firstLetterStyle : ownStyle);
    const created = [];
    const deleted = [];
    const rule = { selectorText: '.body p.first::first-letter' };
    const sheet = {
      cssRules: [rule],
      deleteRule(i) { deleted.push(i); this.cssRules.splice(i, 1); },
    };
    global.document = {
      querySelectorAll: () => [para],
      createElement: () => {
        const el = new FakeElement('span');
        created.push(el);
        return el;
      },
      styleSheets: [sheet],
    };
    return { para, textNode, created, deleted, sheet };
  }

  test('lifts the cap into a real float and strips it from the text node', () => {
    const { para, textNode, created } = makeParagraph();
    materializeFirstLetter();

    expect(created).toHaveLength(1);
    expect(created[0].textContent).toBe('当');
    expect(created[0].getAttribute('data-snapshot-first-letter')).toBe('');
    const style = created[0].getAttribute('style');
    expect(style).toContain('float: left');
    expect(style).toContain('font-size: 96px');
    expect(style).toContain('width: 98.3125px');
    // The stand-in is placed before the text, and the cap character is gone from it.
    expect(para.childNodes[0]).toBe(created[0]);
    expect(textNode.nodeValue).toBe('我们在屏幕上读到');
  });

  test('removes the ::first-letter rule so the pseudo cannot style the cap twice', () => {
    const { deleted, sheet } = makeParagraph();
    materializeFirstLetter();
    expect(deleted).toEqual([0]);
    expect(sheet.cssRules).toHaveLength(0);
  });

  test('leaves an element with no ::first-letter rule untouched', () => {
    const { textNode, created } = makeParagraph();
    const same = fakeStyle({ float: 'none', 'font-size': '18px' });
    global.getComputedStyle = () => same;
    materializeFirstLetter();
    expect(created).toHaveLength(0);
    expect(textNode.nodeValue).toBe('当我们在屏幕上读到');
  });
});
