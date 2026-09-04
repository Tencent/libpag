'use strict';

// `expandStickyScrollytelling` normally runs inside `page.evaluate` against a
// real DOM: it finds scrollytelling blocks (a sticky panel inside a tall
// scroll track whose stacked step layers cross-fade on scroll — Flect's
// "How Flect works" is the canonical case) and rewrites them into N vertically
// tiled panels so the static snapshot shows every step. The function is plain
// DOM/CSSOM code, so we drive it in node behind minimal element /
// getComputedStyle / document stubs, the same way pseudo-materialise.test.js
// and icon-font-browser.test.js exercise their in-page helpers.

const { expandStickyScrollytelling } = require('../dist/lib/browser-snapshot');

// ---- Fake-DOM builders -----------------------------------------------------

// Computed-style stub. The pass reads IDL properties (cs.position) while
// other passes use getPropertyValue(); real CSSStyleDeclaration supports both,
// so the stub proxies the prop map for either shape.
function makeCs(props) {
  return new Proxy(props, {
    get(target, k) {
      if (k === 'getPropertyValue') return (key) => target[key] || '';
      return target[k];
    },
  });
}

// Inline style stub supporting property assignment (style.opacity = '1') and
// removeProperty, the two shapes the pass uses.
function makeStyle() {
  const store = {};
  return {
    setProperty(k, v) { store[k] = String(v); },
    removeProperty(k) { delete store[k]; },
    get opacity() { return store.opacity; },
    set opacity(v) { store.opacity = String(v); },
    get filter() { return store.filter; },
    set filter(v) { store.filter = String(v); },
    get visibility() { return store.visibility; },
    set visibility(v) { store.visibility = String(v); },
    get transform() { return store.transform; },
    set transform(v) { store.transform = String(v); },
    get position() { return store.position; },
    set position(v) { store.position = String(v); },
    get top() { return store.top; },
    set top(v) { store.top = String(v); },
    get left() { return store.left; },
    set left(v) { store.left = String(v); },
    get width() { return store.width; },
    set width(v) { store.width = String(v); },
    get height() { return store.height; },
    set height(v) { store.height = String(v); },
    get boxSizing() { return store['box-sizing']; },
    set boxSizing(v) { store['box-sizing'] = String(v); },
    get margin() { return store.margin; },
    set margin(v) { store.margin = String(v); },
  };
}

// Element stub covering the DOM slice the pass touches. `opts.rect` is
// `{ top, left, width, height }` (bottom is derived); `opts.cs` is the
// computed-style prop map.
function makeEl(tag, opts = {}) {
  const el = {
    tagName: tag.toUpperCase(),
    children: [],
    parentElement: null,
    _attrs: {},
    _csProps: Object.assign(
      { position: 'static', display: 'block', opacity: '1', transform: 'none' },
      opts.cs || {}
    ),
    _rect: Object.assign({ top: 0, left: 0, width: 0, height: 0 }, opts.rect || {}),
    setAttribute(k, v) { el._attrs[k] = String(v); },
    getAttribute(k) { return (k in el._attrs) ? el._attrs[k] : null; },
    hasAttribute(k) { return k in el._attrs; },
    get style() {
      if (!el._style) el._style = makeStyle();
      return el._style;
    },
    getBoundingClientRect() {
      const r = el._rect;
      return { top: r.top, left: r.left, width: r.width, height: r.height, bottom: r.top + r.height, right: r.left + r.width };
    },
    appendChild(node) {
      node.parentElement = el;
      el.children.push(node);
      return node;
    },
    remove() {
      if (!el.parentElement) return;
      const sibs = el.parentElement.children;
      const i = sibs.indexOf(el);
      if (i >= 0) sibs.splice(i, 1);
      el.parentElement = null;
    },
    closest(sel) {
      let cur = el;
      const attr = sel.match(/\[([a-z-]+)\]/);
      while (cur) {
        if (attr ? cur._attrs[attr[1]] !== undefined : false) return cur;
        cur = cur.parentElement;
      }
      return null;
    },
    // Selector query over the subtree. Supports '*', '[attr="value"]' and
    // bare '[attr]' — the three forms the pass and the assertions issue.
    querySelectorAll(sel) {
      const exact = sel.match(/^\[([a-z-]+)="(.*)"\]$/);
      const bare = !exact && sel.match(/^\[([a-z-]+)\]$/);
      const out = [];
      const walk = (node) => {
        for (const c of node.children) {
          if (sel === '*') {
            out.push(c);
          } else if (exact) {
            if (c._attrs[exact[1]] === exact[2]) out.push(c);
          } else if (bare) {
            if (c._attrs[bare[1]] !== undefined) out.push(c);
          }
          walk(c);
        }
      };
      walk(el);
      return out;
    },
    // Deep clone: fresh element with copied attrs, computed props, rect and a
    // recursively cloned subtree (mirrors cloneNode(true) for this stub).
    cloneNode(deep) {
      const copy = makeEl(el.tagName, {
        cs: { ...el._csProps },
        rect: { ...el._rect },
      });
      copy._attrs = { ...el._attrs };
      if (deep) {
        for (const c of el.children) {
          copy.appendChild(c.cloneNode(true));
        }
      }
      return copy;
    },
  };
  return el;
}

// Wire `parent` <- `child` and return the child.
function nest(parent, child) {
  parent.appendChild(child);
  return child;
}

// Flatten a subtree depth-first (document.body.querySelectorAll('*')).
function flatten(root) {
  const out = [];
  const walk = (node) => {
    for (const c of node.children) {
      out.push(c);
      walk(c);
    }
  };
  walk(root);
  return out;
}

let savedGlobals;

beforeEach(() => {
  savedGlobals = {
    document: global.document,
    getComputedStyle: global.getComputedStyle,
  };
  global.getComputedStyle = (el) => (el && el._csProps) ? makeCs(el._csProps) : makeCs({});
});

afterEach(() => {
  global.document = savedGlobals.document;
  global.getComputedStyle = savedGlobals.getComputedStyle;
});

// Run the pass against a fake document rooted at `bodyEl`.
function run(bodyEl) {
  global.document = { body: bodyEl };
  return expandStickyScrollytelling();
}

// Build the canonical scrollytelling fixture: a 4545px track, a sticky 766px
// panel, one stack group of `n` cross-fading absolute layers (opacity 1/0/0)
// at a shared parent. Layer rects are 487x292 all pinned at the same top so
// the pairwise overlap check passes.
function makeScrollytelling({ layers = 3, trackHeight = 4545, panelHeight = 766 } = {}) {
  const body = makeEl('body', { rect: { top: 0, left: 0, width: 1400, height: 13401 } });
  const track = nest(body, makeEl('div', {
    cs: { position: 'relative' },
    rect: { top: 2795, left: 0, width: 1400, height: trackHeight },
  }));
  const panel = nest(track, makeEl('div', {
    cs: { position: 'sticky', top: '56px' },
    rect: { top: 2795, left: 168, width: 1064, height: panelHeight },
  }));
  const stack = nest(panel, makeEl('div', {
    rect: { top: 3000, left: 168, width: 519, height: 292 },
  }));
  const stepEls = [];
  for (let i = 0; i < layers; i++) {
    stepEls.push(nest(stack, makeEl('div', {
      cs: { position: 'absolute', opacity: i === 0 ? '1' : '0' },
      rect: { top: 3100, left: 168, width: 487, height: 292 },
    })));
  }
  return { body, track, panel, stack, stepEls };
}

describe('expandStickyScrollytelling', () => {
  test('expands a scrollytelling block into one tiled panel per step layer', () => {
    const { body, track, stepEls } = makeScrollytelling();
    const result = run(body);

    expect(result.blocks).toBe(1);
    expect(result.expanded).toEqual([{ segments: 3 }]);

    // Track now hosts 3 panels, tagged with the segment count.
    expect(track.children.length).toBe(3);
    expect(track._attrs['data-snapshot-sticky-expanded']).toBe('3');

    // Panels are absolutely positioned, evenly tiled and centred in their
    // 1515px segment (segment height = 4545 / 3).
    const segH = 4545 / 3;
    track.children.forEach((panel, i) => {
      expect(panel.style.position).toBe('absolute');
      expect(parseFloat(panel.style.top)).toBeCloseTo(i * segH + (segH - 766) / 2, 1);
    });

    // Segment 1 keeps the original visible layer; segments 2 and 3 keep
    // cloned copies of layers 2 and 3 respectively, forced visible.
    track.children.forEach((panel, i) => {
      const kept = panel.querySelectorAll('[data-snapshot-layer-group]');
      expect(kept.length).toBe(1);
      expect(kept[0]._attrs['data-snapshot-layer-index']).toBe(String(i));
      expect(kept[0].style.opacity).toBe('1');
      expect(kept[0].style.filter).toBe('none');
      expect(kept[0].style.visibility).toBe('visible');
    });

    // The original step elements 2..N were removed from segment 1 (the
    // original panel) — only step 1 survives there.
    expect(stepEls[0]._attrs['data-snapshot-layer-index']).toBe('0');
    expect(stepEls[1].parentElement).toBeNull();
    expect(stepEls[2].parentElement).toBeNull();
  });

  test('freezes the surviving layer transform from its computed value', () => {
    const { body, stepEls } = makeScrollytelling();
    stepEls[0]._csProps.transform = 'matrix(1, 0, 0, 1, 0, -146)';
    run(body);
    // The kept layer in segment 1 keeps its runtime transform instead of
    // snapping back to the CSS base rule.
    expect(stepEls[0].style.transform).toBe('matrix(1, 0, 0, 1, 0, -146)');
  });

  test('aligns multiple layer groups per segment', () => {
    const { body, panel, track } = makeScrollytelling();
    // A second stack group (phone screens) with 3 layers.
    const visual = nest(panel, makeEl('div', {
      rect: { top: 3000, left: 800, width: 520, height: 766 },
    }));
    for (let i = 0; i < 3; i++) {
      nest(visual, makeEl('img', {
        cs: { position: 'absolute', opacity: i === 0 ? '1' : '0' },
        rect: { top: 3100, left: 800, width: 364, height: 749 },
      }));
    }
    run(body);

    track.children.forEach((seg, i) => {
      // One surviving layer per group in every segment, both with index i.
      const kept = seg.querySelectorAll('[data-snapshot-layer-group]');
      expect(kept.length).toBe(2);
      kept.forEach((l) => expect(l._attrs['data-snapshot-layer-index']).toBe(String(i)));
    });
  });

  test('clamps the kept index when groups have different layer counts', () => {
    const { body, panel, track } = makeScrollytelling({ layers: 3 });
    // A 2-layer group against the 3-layer step group.
    const visual = nest(panel, makeEl('div', {
      rect: { top: 3000, left: 800, width: 520, height: 766 },
    }));
    for (let i = 0; i < 2; i++) {
      nest(visual, makeEl('img', {
        cs: { position: 'absolute', opacity: i === 0 ? '1' : '0' },
        rect: { top: 3100, left: 800, width: 364, height: 749 },
      }));
    }
    run(body);

    // Segment 3 (index 2) clamps the short group to its last layer (index 1).
    const seg3 = track.children[2];
    const kept = seg3.querySelectorAll('[data-snapshot-layer-group]');
    const indexes = kept.map((l) => l._attrs['data-snapshot-layer-index']);
    expect(indexes).toEqual(['2', '1']);
  });

  test('ignores a sticky header whose parent is not a tall track', () => {
    const body = makeEl('body', { rect: { top: 0, left: 0, width: 1400, height: 4000 } });
    const flow = nest(body, makeEl('div', {
      rect: { top: 0, left: 0, width: 1400, height: 160 },
    }));
    nest(flow, makeEl('header', {
      cs: { position: 'sticky', top: '0px' },
      rect: { top: 0, left: 0, width: 1400, height: 80 },
    }));
    const result = run(body);
    expect(result.blocks).toBe(0);
  });

  test('ignores stacked layers whose opacities are not mutually exclusive', () => {
    const { body } = makeScrollytelling();
    // Flip every layer fully visible: no cross-fade, no expansion.
    const stack = body.children[0].children[0].children[0];
    stack.children.forEach((l) => { l._csProps.opacity = '1'; });
    const result = run(body);
    expect(result.blocks).toBe(0);
  });

  test('ignores a sticky panel sharing the track with in-flow siblings', () => {
    const { body, track } = makeScrollytelling();
    nest(track, makeEl('p', {
      rect: { top: 3600, left: 0, width: 1400, height: 40 },
    }));
    const result = run(body);
    expect(result.blocks).toBe(0);
  });

  test('pins the track height only when the panel leaving the flow collapses it', () => {
    const { body, track } = makeScrollytelling({ trackHeight: 4545 });
    // Simulate a content-driven track: its measured height collapses to 100px
    // as soon as any child panel has switched to absolute positioning (the
    // moment the pass detaches the sticky panel from the flow).
    const originalRect = { ...track._rect };
    track.getBoundingClientRect = () => {
      const detached = track.children.some((c) => c.style.position === 'absolute');
      const h = detached ? 100 : originalRect.height;
      return {
        top: originalRect.top, left: originalRect.left,
        width: originalRect.width, height: h,
        bottom: originalRect.top + h, right: originalRect.left + originalRect.width,
      };
    };
    run(body);

    // The collapsed track (100px) differs from the recorded pre-collapse
    // height (4545px), so the pass pins the original height inline.
    expect(track.style.height).toBe('4545px');
    expect(track.style.boxSizing).toBe('border-box');
  });

  test('leaves the track height alone when it does not collapse', () => {
    const { body, track } = makeScrollytelling();
    run(body);
    // Track keeps a CSS-driven height: the pass must not write an inline one.
    expect(track.style.height).toBeUndefined();
    expect(track.style.boxSizing).toBeUndefined();
  });

  test('is idempotent: a second run finds nothing left to expand', () => {
    const { body } = makeScrollytelling();
    const first = run(body);
    expect(first.blocks).toBe(1);
    const second = run(body);
    expect(second.blocks).toBe(0);
  });
});
