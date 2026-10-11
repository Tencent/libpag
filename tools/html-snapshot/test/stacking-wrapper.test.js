'use strict';

// `resolveEscapingWrapperZIndex` decides whether a static box that paints
// nothing of its own should be ordered by the stacking of its children instead
// of by its own in-flow position. The subset flattens every box to `position:
// absolute` and replays paint order per parent; without this a stack of
// `z-index` photos inside a plain wrapper sorts to the flow band and loses to a
// later `z-index: auto` card the browser paints underneath them — the
// negative-margin pull-card pattern from 06-deep-nesting-stacking.html. The
// function is plain DOM/CSSOM code, so we drive it in node behind the same
// minimal style stubs the pseudo tests use.

const { resolveEscapingWrapperZIndex } = require('../dist/lib/browser-snapshot');

const NODE_STUB = { ELEMENT_NODE: 1, TEXT_NODE: 3, DOCUMENT_NODE: 9 };

// A live CSSStyleDeclaration-shaped stub. `props` may hold getter functions
// (read at call time) or plain strings.
function makeCs(props) {
  const read = (k) => {
    const v = props[k];
    return (typeof v === 'function' ? v() : v) || '';
  };
  return {
    getPropertyValue: read,
    // Real computed styles always resolve these to a keyword; mirror that so a
    // missing key behaves like the browser default rather than an empty string.
    get position() { return read('position') || 'static'; },
    get display() { return read('display') || 'block'; },
    get zIndex() { return read('z-index'); },
  };
}

// Minimal Element stub. `__self` is the element's own computed style (what
// `getComputedStyle(el)` returns with no pseudo argument).
class FakeElement {
  constructor(tag) {
    this.tagName = (tag || 'DIV').toUpperCase();
    this.nodeType = NODE_STUB.ELEMENT_NODE;
    this._attrs = {};
    this._children = [];
    this.__self = makeCs({});
  }
  get children() { return this._children; }
  get childNodes() { return this._children; }
  appendChild(node) {
    this._children.push(node);
    return node;
  }
  hasAttribute(k) { return k in this._attrs; }
  getAttribute(k) { return k in this._attrs ? this._attrs[k] : null; }
  setAttribute(k, v) { this._attrs[k] = String(v); }
}

// Build a child element with the given computed style, then attach it.
function addChild(parent, style) {
  const child = new FakeElement('div');
  child.__self = makeCs(style);
  parent.appendChild(child);
  return child;
}

let savedGlobals;

beforeEach(() => {
  savedGlobals = { Node: global.Node, getComputedStyle: global.getComputedStyle };
  global.Node = NODE_STUB;
  global.getComputedStyle = (el) => el.__self || makeCs({});
});

afterEach(() => {
  global.Node = savedGlobals.Node;
  global.getComputedStyle = savedGlobals.getComputedStyle;
});

// A plain static, transparent wrapper — the negative-margin `.over` shape.
function makeWrapper(style) {
  const el = new FakeElement('div');
  el.__self = makeCs(style || {});
  return el;
}

describe('resolveEscapingWrapperZIndex — pure stacking wrapper', () => {
  test('a transparent static wrapper sorts by its highest child z-index', () => {
    const wrap = makeWrapper();
    addChild(wrap, { position: 'relative', 'z-index': '3' });
    addChild(wrap, { position: 'relative', 'z-index': '10' });
    addChild(wrap, { position: 'relative', 'z-index': '7' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBe(10);
  });

  test('all-positioned auto-z children collapse to z 0', () => {
    const wrap = makeWrapper();
    addChild(wrap, { position: 'absolute' });
    addChild(wrap, { position: 'relative', 'z-index': 'auto' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBe(0);
  });

  test('a flex wrapper stacks its static flex items (z 0)', () => {
    const wrap = makeWrapper({ display: 'flex' });
    addChild(wrap, { position: 'static' });
    addChild(wrap, { position: 'static' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBe(0);
  });

  test('a background disqualifies it (it paints its own box)', () => {
    const wrap = makeWrapper({ 'background-color': 'rgb(255, 255, 255)' });
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('a box-shadow disqualifies it', () => {
    const wrap = makeWrapper({ 'box-shadow': 'rgba(0, 0, 0, 0.12) 0px 20px 40px 0px' });
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('a non-stackable (flow) child disqualifies it', () => {
    const wrap = makeWrapper();
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    addChild(wrap, { position: 'static' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('a negative-z child disqualifies it (single z cannot split the band)', () => {
    const wrap = makeWrapper();
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    addChild(wrap, { position: 'relative', 'z-index': '-1' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('clipping the overflow disqualifies it', () => {
    const wrap = makeWrapper({ 'overflow-x': 'hidden', 'overflow-y': 'hidden' });
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('a positioned wrapper is handled by its own stacking, not here', () => {
    const wrap = makeWrapper({ position: 'relative' });
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('a transform disqualifies it (it is itself a stacking context)', () => {
    const wrap = makeWrapper({ transform: 'matrix(1, 0, 0, 1, 0, 0)' });
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('an empty wrapper has no stacking to replay', () => {
    const wrap = makeWrapper();
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });

  test('direct text content disqualifies it', () => {
    const wrap = makeWrapper();
    const text = { nodeType: NODE_STUB.TEXT_NODE, nodeValue: 'hello' };
    wrap._children.push(text);
    addChild(wrap, { position: 'relative', 'z-index': '9' });
    expect(resolveEscapingWrapperZIndex(wrap, wrap.__self)).toBeNull();
  });
});
