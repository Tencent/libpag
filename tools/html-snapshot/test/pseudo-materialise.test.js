'use strict';

// `materializeDecorativePseudoElements` normally runs inside `page.evaluate`
// against a real DOM: it walks every element, reads `getComputedStyle(el,
// '::before' | '::after')`, and appends a synthetic `<div>` mirroring each
// decorative pseudo's box so the snapshot walker (which only sees real DOM
// nodes) can emit it. The function is plain DOM/CSSOM code, so we drive it in
// node behind minimal `document` / `Element` / `getComputedStyle` stubs, the
// same way icon-font-browser.test.js exercises the icon-font helpers.
//
// The focus here is the *animated* decorative pseudo path: when the capture
// pipeline freezes the page at t=0, an entrance-animated pseudo (e.g. an
// underline that draws in with `scaleX(0) -> scaleX(1)`) reads as invisible /
// zero-scaled. The materialiser must copy the pseudo's *resting* style (so the
// synthetic box keeps real geometry and paint) plus the `animation-*`
// longhands (so the capture pass re-samples the motion into a `pagxAnim*`),
// instead of baking the transient 0%-keyframe state that the walker would drop
// (`opacity: 0`) or collapse to zero size (`scaleX(0)`).

const {
  materializeDecorativePseudoElements,
} = require('../dist/lib/browser-snapshot');

// A live CSSStyleDeclaration-shaped stub. `props` may hold getter functions
// (read at call time) or plain strings, so a test can flip a value after the
// pseudo's animation is cancelled.
function makeCs(props) {
  return {
    getPropertyValue(k) {
      const v = props[k];
      return (typeof v === 'function' ? v() : v) || '';
    },
  };
}

// Minimal Element stub. `__cs` maps a pseudo selector to its computed-style
// stub; `__anims` is what `getAnimations({ subtree: true })` returns.
class FakeElement {
  constructor(tag) {
    this.tagName = (tag || 'DIV').toUpperCase();
    this.children = [];
    this.firstChild = null;
    this.offsetHeight = 0;
    this._attrs = {};
    this.__cs = { '::before': makeCs({}), '::after': makeCs({}) };
    this.__anims = [];
  }
  closest() { return null; }
  hasAttribute(k) { return k in this._attrs; }
  getAttribute(k) { return k in this._attrs ? this._attrs[k] : null; }
  setAttribute(k, v) { this._attrs[k] = String(v); }
  getAnimations() { return this.__anims; }
  appendChild(node) { this.children.push(node); if (!this.firstChild) this.firstChild = node; return node; }
  insertBefore(node) { this.children.unshift(node); this.firstChild = node; return node; }
  get style() { return this._attrs.style || ''; }
}

let savedGlobals;

beforeEach(() => {
  savedGlobals = {
    document: global.document,
    getComputedStyle: global.getComputedStyle,
    Element: global.Element,
  };
  global.Element = FakeElement;
  global.getComputedStyle = (el, pseudo) =>
    (el && el.__cs && el.__cs[pseudo]) || makeCs({});
});

afterEach(() => {
  global.document = savedGlobals.document;
  global.getComputedStyle = savedGlobals.getComputedStyle;
  global.Element = savedGlobals.Element;
});

// Build a host carrying a single decorative `::after`. `animated` wires up a
// cancellable WAAPI-shaped animation whose cancel() flips the animatable
// channels from their frozen 0% phase to their resting values, mirroring how a
// real browser reverts computed style once an animation is cancelled.
function makeHostWithAfter({ animated }) {
  const host = new FakeElement('div');
  const state = { cancelled: false };
  const afterProps = {
    content: '""',
    position: 'absolute',
    left: '0px',
    right: '18px',
    bottom: '0px',
    width: '450px',
    height: '2px',
    'background-image':
      'linear-gradient(90deg, rgb(95, 255, 240), rgb(40, 224, 208) 55%, rgba(40, 224, 208, 0))',
  };
  if (animated) {
    afterProps['animation-name'] = 'nbLine';
    afterProps['animation-duration'] = '0.6s';
    afterProps['animation-timing-function'] = 'ease';
    afterProps['animation-delay'] = '0.55s';
    afterProps['animation-iteration-count'] = '1';
    afterProps['animation-direction'] = 'normal';
    afterProps['animation-fill-mode'] = 'both';
    // Frozen at 0% until cancelled, then resting.
    afterProps.opacity = () => (state.cancelled ? '1' : '0');
    afterProps.transform = () => (state.cancelled ? 'none' : 'matrix(0, 0, 0, 1, 0, 0)');
    host.__anims = [{
      effect: { target: host, pseudoElement: '::after' },
      cancel() { state.cancelled = true; },
    }];
  }
  host.__cs = {
    '::before': makeCs({ content: 'none' }),
    '::after': makeCs(afterProps),
  };
  return { host, state };
}

function runOn(host) {
  global.document = {
    querySelectorAll: () => [host],
    createElement: (tag) => new FakeElement(tag),
  };
  return materializeDecorativePseudoElements();
}

describe('materializeDecorativePseudoElements — animated decorative pseudo', () => {
  test('bakes the resting style and forwards the animation longhands', async () => {
    const { host, state } = makeHostWithAfter({ animated: true });
    await runOn(host);

    // Exactly one synthetic child (the ::after box) was appended to the host.
    expect(host.children).toHaveLength(1);
    const div = host.children[0];
    expect(div.getAttribute('data-snapshot-pseudo')).toBe('::after');
    expect(host.getAttribute('data-snapshot-pseudo-host')).toBe('');

    const style = div.getAttribute('style');
    // Resting geometry + paint survive.
    expect(style).toContain('width: 450px');
    expect(style).toContain('height: 2px');
    expect(style).toContain('linear-gradient(90deg, rgb(95, 255, 240)');
    // The animation is forwarded so the capture pass reattaches it as pagxAnim*.
    expect(style).toContain('animation-name: nbLine');
    expect(style).toContain('animation-duration: 0.6s');
    expect(style).toContain('animation-delay: 0.55s');
    expect(style).toContain('animation-fill-mode: both');

    // The pseudo's own animation was cancelled so the resting read is taken,
    // and the transient 0% phase (opacity: 0 / scaleX(0)) is NOT baked in —
    // those would drop or zero-size the box in the walker.
    expect(state.cancelled).toBe(true);
    expect(style).not.toContain('opacity: 0');
    expect(style).not.toContain('matrix(0, 0, 0');
  });

  test('a non-animated decorative pseudo still materialises without animation props', async () => {
    const { host, state } = makeHostWithAfter({ animated: false });
    await runOn(host);

    expect(host.children).toHaveLength(1);
    const style = host.children[0].getAttribute('style');
    expect(style).toContain('width: 450px');
    expect(style).toContain('height: 2px');
    expect(style).not.toContain('animation-name');
    // No animation to cancel.
    expect(state.cancelled).toBe(false);
  });
});
