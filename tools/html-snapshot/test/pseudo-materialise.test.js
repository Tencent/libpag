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

describe('materializeDecorativePseudoElements — in-flow decorative pseudo', () => {
  // Mirrors the getflect.app `.free-tag` chip: an inline-flex host whose
  // ::before is a static, 6x6 flex-item status dot with rounded corners.
  function makeHostWithInFlowBefore() {
    const host = new FakeElement('span');
    host.appendChild(new FakeElement('span'));
    host.__cs = {
      '::before': makeCs({
        content: '""',
        position: 'static',
        display: 'block',
        width: '6px',
        height: '6px',
        'border-top-left-radius': '50%',
        'background-color': 'rgba(116, 138, 42, 0.18)',
        'flex-grow': '0',
        'flex-shrink': '0',
        'flex-basis': 'auto',
      }),
      '::after': makeCs({ content: 'none' }),
    };
    return host;
  }

  function runOnCapturingStyles(host) {
    const styleEls = [];
    global.document = {
      querySelectorAll: () => [host],
      createElement: (tag) => new FakeElement(tag),
      head: { appendChild: (el) => styleEls.push(el) },
    };
    return materializeDecorativePseudoElements().then(() => styleEls);
  }

  test('materialises a static in-flow pseudo as a layout stand-in and switches the original off', async () => {
    const host = makeHostWithInFlowBefore();
    const styleEls = await runOnCapturingStyles(host);

    // Exactly one stand-in div, inserted in front of the host's real child
    // (the ::before slot), so the document order mirrors paint order.
    expect(host.children).toHaveLength(2);
    const div = host.children[0];
    expect(div.getAttribute('data-snapshot-pseudo')).toBe('::before');
    expect(host.getAttribute('data-snapshot-pseudo-host')).toBe('');

    const style = div.getAttribute('style');
    // Layout identity is copied so the stand-in occupies the same flow slot.
    expect(style).toContain('display: block');
    expect(style).toContain('width: 6px');
    expect(style).toContain('height: 6px');
    expect(style).toContain('background-color: rgba(116, 138, 42, 0.18)');
    expect(style).toContain('border-top-left-radius: 50%');
    expect(style).toContain('flex-shrink: 0');

    // The original pseudo is switched off via an injected content:none rule
    // so pseudo + stand-in cannot double-occupy the flow slot.
    expect(host.getAttribute('data-snapshot-pseudo-off-0')).toBe('');
    expect(styleEls).toHaveLength(1);
    expect(styleEls[0].getAttribute('data-snapshot-pseudo-off')).toBe('');
    expect(styleEls[0].textContent)
      .toContain('[data-snapshot-pseudo-off-0]::before { content: none !important; }');
  });

  test('an out-of-flow pseudo does not inject a content:none rule', async () => {
    const { host } = makeHostWithAfter({ animated: false });
    const styleEls = await runOnCapturingStyles(host);

    expect(host.children).toHaveLength(1);
    expect(styleEls).toHaveLength(0);
    expect(host.hasAttribute('data-snapshot-pseudo-off-0')).toBe(false);
  });

  test('a sticky pseudo is still rejected', async () => {
    const host = new FakeElement('div');
    host.__cs = {
      '::before': makeCs({
        content: '""',
        position: 'sticky',
        width: '6px',
        height: '6px',
        'background-color': 'rgb(255, 0, 0)',
      }),
      '::after': makeCs({ content: 'none' }),
    };
    const styleEls = await runOnCapturingStyles(host);

    expect(host.children).toHaveLength(0);
    expect(styleEls).toHaveLength(0);
    expect(host.getAttribute('data-snapshot-pseudo-skipped')).toBe('position-sticky');
  });
});

describe('materializeDecorativePseudoElements — out-of-flow text pseudo', () => {
  // Mirrors baidu-pan's "current device" badge: the circle host paints its icon as a background
  // image and hangs a `本机` band off its bottom edge through `::before`. The pseudo's own box —
  // absolute bottom anchor, fixed size, translucent background — is exactly what the text-leaf
  // path drops, leaving the glyphs flush with the host's leading edge.
  function makeHostWithBadgeBefore() {
    const host = new FakeElement('div');
    host.__cs = {
      '::before': makeCs({
        content: '"本机"',
        position: 'absolute',
        bottom: '-3px',
        width: '90px',
        height: '22px',
        'background-color': 'rgba(73, 83, 102, 0.1)',
        color: 'rgb(129, 137, 153)',
        'font-size': '12px',
        'line-height': '24px',
        'text-align': 'center',
        'white-space': 'nowrap',
      }),
      '::after': makeCs({ content: 'none' }),
    };
    return host;
  }

  test('carries the glyphs on a stand-in that keeps the pseudo box', async () => {
    const host = makeHostWithBadgeBefore();
    await runOn(host);

    expect(host.children).toHaveLength(1);
    const div = host.children[0];
    expect(div.getAttribute('data-snapshot-pseudo')).toBe('::before');
    expect(host.getAttribute('data-snapshot-pseudo-host')).toBe('');

    const style = div.getAttribute('style');
    expect(style).toContain('position: absolute');
    expect(style).toContain('bottom: -3px');
    expect(style).toContain('width: 90px');
    expect(style).toContain('height: 22px');
    expect(style).toContain('background-color: rgba(73, 83, 102, 0.1)');
    // The box was sized for one line, so the glyphs must not rewrap.
    expect(style).toContain('white-space: nowrap');
    // The glyphs ride the stand-in instead of the host's flow.
    expect(div.textContent).toBe('本机');
  });

  test('an in-flow text pseudo keeps the text-leaf path', async () => {
    const host = new FakeElement('div');
    host.__cs = {
      '::before': makeCs({ content: '"→"', position: 'static', width: 'auto', height: 'auto' }),
      '::after': makeCs({ content: 'none' }),
    };
    await runOn(host);

    expect(host.children).toHaveLength(0);
    expect(host.hasAttribute('data-snapshot-pseudo-host')).toBe(false);
  });

  test('a host mixing a box pseudo with in-flow text is left to the text-leaf path', async () => {
    const host = new FakeElement('div');
    host.__cs = {
      '::before': makeCs({ content: '"→"', position: 'static', width: 'auto', height: 'auto' }),
      '::after': makeCs({
        content: '""',
        position: 'absolute',
        width: '10px',
        height: '10px',
      }),
    };
    await runOn(host);

    // Appending a stand-in suppresses `renderPseudoTextLeaf` for the whole host, so materialising
    // the box pseudo would silently drop the in-flow glyphs — the host keeps the text path.
    expect(host.children).toHaveLength(0);
    expect(host.hasAttribute('data-snapshot-pseudo-host')).toBe(false);
  });

  test('a host with an already inlined icon-font glyph is not materialised again', async () => {
    const host = new FakeElement('div');
    host.setAttribute('data-snapshot-icon-svg-id', 'icon-1');
    host.__cs = {
      '::before': makeCs({
        content: '"\\e901"',
        position: 'absolute',
        width: '32px',
        height: '32px',
      }),
      '::after': makeCs({ content: 'none' }),
    };
    await runOn(host);

    // The icon-font pass already replaced the glyph with an inline `<svg>`; materialising the
    // pseudo box would paint the raw glyph next to it.
    expect(host.children).toHaveLength(0);
    expect(host.hasAttribute('data-snapshot-pseudo-host')).toBe(false);
  });
});
