'use strict';

// `hoistTableRowBackgrounds` runs inside `page.evaluate` against a live DOM,
// but its body is plain DOM code, so it is driven in node behind minimal
// `document` / element stubs — the same approach collapsed-details.test.js
// takes for `dropCollapsedDetailsContent`.
//
// What it must encode, all confirmed against Chromium:
//   - a painted row background is hoisted into a standalone box placed ahead
//     of every row, carrying `z-index: -1` so the walker's paint order keeps
//     it beneath the (static, flow-band) row content — a plain absolute box
//     sorts into the positioned band and covers the row it belongs to;
//   - the host row's own background is cleared so the paint is not doubled;
//   - the row group gains `position: relative` so the hoisted box resolves
//     against it;
//   - rows whose sheet carries an image / border / shadow / radius, and
//     fully transparent rows, keep their box untouched.

const { hoistTableRowBackgrounds } = require('../dist/lib/browser-snapshot');

function fakeStyle() {
  const style = {};
  return new Proxy(style, {
    get: (target, key) => (key in target ? target[key] : ''),
    set: (target, key, value) => {
      target[key] = value;
      return true;
    },
  });
}

class FakeElement {
  constructor(tag) {
    this.nodeType = 1;
    this.tagName = String(tag).toUpperCase();
    this.childNodes = [];
    this.parentNode = null;
    this.style = fakeStyle();
    this.attributes = {};
  }
  get children() {
    return this.childNodes.filter((n) => n.nodeType === 1);
  }
  get firstChild() {
    return this.childNodes[0] || null;
  }
  get parentElement() {
    return this.parentNode;
  }
  appendChild(node) {
    node.parentNode = this;
    this.childNodes.push(node);
    return node;
  }
  insertBefore(node, ref) {
    node.parentNode = this;
    const at = ref ? this.childNodes.indexOf(ref) : -1;
    if (at < 0) {
      this.childNodes.push(node);
    } else {
      this.childNodes.splice(at, 0, node);
    }
    return node;
  }
  setAttribute(name, value) {
    this.attributes[name] = String(value);
  }
  getAttribute(name) {
    return name in this.attributes ? this.attributes[name] : null;
  }
}

function makeRow({ rect, style = {} }) {
  const row = new FakeElement('tr');
  row.getBoundingClientRect = () => rect;
  row.computed = style;
  return row;
}

function installEnvironment(rows) {
  const created = [];
  global.document = {
    querySelectorAll: (selector) => (selector === 'tr' ? rows : []),
    createElement: (tag) => {
      const el = new FakeElement(tag);
      created.push(el);
      return el;
    },
  };
  global.getComputedStyle = (el) => ({
    position: el === undefined || el === null ? 'static' : (el.position || 'static'),
    getPropertyValue: (prop) => {
      const style = el.computed || {};
      return prop in style ? style[prop] : '';
    },
  });
  return created;
}

const ROW_RECT = { left: 10, top: 37, width: 1149, height: 37 };
const GROUP_RECT = { left: 10, top: 0, width: 1149, height: 334 };

const PAINTED_STYLE = {
  'background-image': 'none',
  'background-color': 'rgb(251, 252, 253)',
  'box-shadow': 'none',
  'border-top-style': 'none',
  'border-right-style': 'none',
  'border-bottom-style': 'none',
  'border-left-style': 'none',
  'border-top-left-radius': '0px',
  'border-top-right-radius': '0px',
  'border-bottom-right-radius': '0px',
  'border-bottom-left-radius': '0px',
};

function buildRow(styleOverrides = {}) {
  const group = new FakeElement('tbody');
  group.getBoundingClientRect = () => GROUP_RECT;
  group.position = 'static';
  const row = makeRow({ rect: ROW_RECT, style: { ...PAINTED_STYLE, ...styleOverrides } });
  group.appendChild(row);
  return { group, row };
}

describe('hoistTableRowBackgrounds', () => {
  test('hoists a painted row background ahead of the rows with z-index:-1', () => {
    const { group, row } = buildRow();
    const created = installEnvironment([row]);

    const stats = hoistTableRowBackgrounds();

    expect(stats).toEqual({ hoisted: 1 });
    expect(created.length).toBe(1);
    const box = created[0];
    const style = box.getAttribute('style');
    expect(style).toContain('position:absolute');
    expect(style).toContain('z-index:-1');
    expect(style).toContain('left:0px');
    expect(style).toContain('top:37px');
    expect(style).toContain('width:1149px');
    expect(style).toContain('height:37px');
    expect(style).toContain('background-color:rgb(251, 252, 253)');
    // Inserted ahead of every row, and the host row's paint is cleared.
    expect(group.childNodes[0]).toBe(box);
    expect(group.childNodes[1]).toBe(row);
    expect(row.style.backgroundColor).toBe('transparent');
    // The row group became the containing block for the hoisted box.
    expect(group.style.position).toBe('relative');
  });

  test('leaves a fully transparent row alone', () => {
    const { row } = buildRow({ 'background-color': 'rgba(0, 0, 0, 0)' });
    const created = installEnvironment([row]);

    const stats = hoistTableRowBackgrounds();

    expect(stats).toEqual({ hoisted: 0 });
    expect(created.length).toBe(0);
    expect(row.style.backgroundColor).toBe('');
  });

  test('skips rows whose sheet carries other visuals', () => {
    const cases = [
      { 'background-image': 'url("data:image/png;base64,AAAA")' },
      { 'box-shadow': '0 1px 2px #0003' },
      { 'border-top-style': 'solid', 'border-top-width': '1px' },
      { 'border-top-left-radius': '4px' },
    ];
    for (const overrides of cases) {
      const { row } = buildRow(overrides);
      const created = installEnvironment([row]);
      const stats = hoistTableRowBackgrounds();
      expect(stats).toEqual({ hoisted: 0 });
      expect(created.length).toBe(0);
      expect(row.style.backgroundColor).toBe('');
    }
  });

  test('keeps a positioned row group untouched', () => {
    const { group, row } = buildRow();
    group.position = 'relative';
    installEnvironment([row]);

    const stats = hoistTableRowBackgrounds();

    expect(stats).toEqual({ hoisted: 1 });
    expect(group.style.position).toBe('');
  });
});
