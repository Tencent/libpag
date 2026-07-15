'use strict';

// Coverage for the shared-clock sampler / global-capture pipeline. The test env
// is `node` (no jsdom), so DOM entry points are exercised by stubbing the
// globals each function reads (`getComputedStyle`, `document`, `window`) and
// passing plain-object fakes carrying only the fields the code touches. Every
// stub is restored in afterEach. Expected values are derived from the real
// function output (characterization), never weakened to trivial checks.

const {
  PAGX_ANIM_PREFIX,
  PAGX_ANIM_STYLE_ID,
  pagxSampleTimesMs,
  pagxMeasureGlobalDurationMs,
  pagxSeekAllToTime,
  pagxCollectGlobalSampled,
  pagxSampleTimeline,
  pagxBuildTextOverlays,
  pagxBuildUseOverlays,
  pagxEmitCaptured,
  pagxAnimMain,
} = require('../dist/lib/animation-capture');

// A computed-style bag with the channels pagxReadAnimChannels reads. `animationName`
// is 'none' so pagxElementUsesStepTiming reports non-stepped (linear), keeping the
// sampler off the step-boundary bisection path. `transform` is 'none' so
// pagxExtractTransform returns null without needing a probe element.
function makeCS(opacityVal) {
  return {
    opacity: String(opacityVal),
    transform: 'none',
    color: 'rgb(0, 0, 0)',
    backgroundColor: 'rgba(0, 0, 0, 0)',
    filter: 'none',
    clipPath: 'none',
    boxShadow: 'none',
    fontSize: '16px',
    animationName: 'none',
    animationTimingFunction: 'linear',
  };
}

// A DOM node that tracks parentNode across appendChild / removeChild, matching
// what the injected-<style> paths (txBlocker, keyframes style) rely on.
function makeContainer() {
  const node = { childNodes: [] };
  node.appendChild = (child) => {
    child.parentNode = node;
    node.childNodes.push(child);
    return child;
  };
  node.removeChild = (child) => {
    child.parentNode = null;
    const i = node.childNodes.indexOf(child);
    if (i >= 0) node.childNodes.splice(i, 1);
    return child;
  };
  return node;
}

function makeStyleNode() {
  return { tagName: 'STYLE', textContent: '', id: '', parentNode: null };
}

describe('pagxSampleTimesMs', () => {
  test('emits N points including both endpoints, evenly spaced', () => {
    expect(pagxSampleTimesMs(1000, 5)).toEqual([0, 250, 500, 750, 1000]);
    expect(pagxSampleTimesMs(2000, 2)).toEqual([0, 2000]);
  });

  test('degenerate inputs collapse to a single [0] sample', () => {
    expect(pagxSampleTimesMs(0, 5)).toEqual([0]);
    expect(pagxSampleTimesMs(-10, 5)).toEqual([0]);
    expect(pagxSampleTimesMs(1000, 1)).toEqual([0]);
    expect(pagxSampleTimesMs(1000, 0)).toEqual([0]);
  });
});

describe('pagxMeasureGlobalDurationMs', () => {
  const origDocument = global.document;
  const origWindow = global.window;
  afterEach(() => {
    global.document = origDocument;
    global.window = origWindow;
  });

  test('no animations and no libraries yields 0 (the default/zero branch)', () => {
    global.window = {};
    global.document = { getAnimations: () => [] };
    expect(pagxMeasureGlobalDurationMs()).toBe(0);
  });

  test('WAAPI: takes the max delay+duration across animations', () => {
    global.window = {};
    global.document = {
      getAnimations: () => [
        { effect: { getComputedTiming: () => ({ duration: 1000, delay: 200 }) } },
        { effect: { getComputedTiming: () => ({ duration: 500, delay: 0 }) } },
      ],
    };
    // max(200 + 1000, 0 + 500) = 1200
    expect(pagxMeasureGlobalDurationMs()).toBe(1200);
  });

  test('anime.js: a longer running instance raises the window', () => {
    global.document = {
      getAnimations: () => [
        { effect: { getComputedTiming: () => ({ duration: 800, delay: 0 }) } },
      ],
    };
    global.window = { anime: { running: [{ duration: 3000 }, { duration: 400 }] } };
    expect(pagxMeasureGlobalDurationMs()).toBe(3000);
  });

  test('GSAP: reconstructs the loop period from top-level children (ms)', () => {
    global.document = { getAnimations: () => [] };
    global.window = {
      gsap: {
        globalTimeline: {
          getChildren: () => [
            { startTime: () => 0, duration: () => 1.5 },
            { startTime: () => 1, duration: () => 0.5 },
          ],
        },
      },
    };
    // max(start + iterDur) = max(1.5, 1.5) = 1.5s -> 1500ms
    expect(pagxMeasureGlobalDurationMs()).toBe(1500);
  });
});

describe('pagxSeekAllToTime', () => {
  const origDocument = global.document;
  const origWindow = global.window;
  afterEach(() => {
    global.document = origDocument;
    global.window = origWindow;
  });

  // A WAAPI-style fake: pause() flips a flag, currentTime is a real accessor so
  // the seek is observable. `birth` optionally sets __pagxBirth.
  function makeAnim(birth) {
    let ct = null;
    const a = { paused: false, pause() { this.paused = true; } };
    if (birth !== undefined) a.__pagxBirth = birth;
    Object.defineProperty(a, 'currentTime', {
      get() { return ct; },
      set(v) { ct = v; },
    });
    return a;
  }

  function docWith(anims) {
    return { getAnimations: () => anims, body: { offsetHeight: 0 } };
  }

  test('pauses and pins currentTime to the absolute time (birth 0)', () => {
    const a = makeAnim();
    global.window = {};
    global.document = docWith([a]);
    pagxSeekAllToTime(500);
    expect(a.paused).toBe(true);
    expect(a.currentTime).toBe(500);
  });

  test('subtracts a mid-timeline birth so a late-born tween plays its local phase', () => {
    const a = makeAnim(100);
    global.window = {};
    global.document = docWith([a]);
    pagxSeekAllToTime(500);
    // local = t - birth = 500 - 100
    expect(a.currentTime).toBe(400);
  });

  test('clamps a pre-birth seek to 0 (tween has not started yet)', () => {
    const a = makeAnim(600);
    global.window = {};
    global.document = docWith([a]);
    pagxSeekAllToTime(500);
    expect(a.currentTime).toBe(0);
  });

  test('seeks every animation in one pass', () => {
    const a = makeAnim();
    const b = makeAnim();
    global.window = {};
    global.document = docWith([a, b]);
    pagxSeekAllToTime(250);
    expect(a.currentTime).toBe(250);
    expect(b.currentTime).toBe(250);
  });

  test('no-libraries branch: empty getAnimations does not throw', () => {
    global.window = {};
    global.document = docWith([]);
    expect(() => pagxSeekAllToTime(300)).not.toThrow();
  });
});

describe('pagxSampleTimeline', () => {
  const origDocument = global.document;
  const origGetComputedStyle = global.getComputedStyle;
  afterEach(() => {
    global.document = origDocument;
    global.getComputedStyle = origGetComputedStyle;
  });

  test('captures elements whose tracked channels vary, skips static ones', () => {
    const mover = { __mover: true };
    const staticEl = {};
    let progress = 0;
    global.document = { body: { offsetHeight: 0, querySelectorAll: () => [mover, staticEl] } };
    // seekFn advances the shared clock; getComputedStyle reads it so the mover's
    // opacity walks 0 -> 1 across the window while the static element holds.
    const seekFn = (p) => { progress = p; };
    global.getComputedStyle = (el) => makeCS(el === mover ? progress : 1);

    const captured = [];
    const seen = new Set();
    pagxSampleTimeline(captured, seen, 1000, seekFn, 1, 5, 100, 'normal', 0);

    expect(captured).toHaveLength(1);
    const cap = captured[0];
    expect(cap.el).toBe(mover);
    // A perfectly linear opacity ramp decimates to its two endpoints.
    expect(cap.keyframes.map((k) => k.offset)).toEqual([0, 1]);
    expect(cap.keyframes.map((k) => k.props.opacity)).toEqual(['0', '1']);
    expect(cap.durationMs).toBe(1000);
    expect(cap.delayMs).toBe(0);
    expect(cap.iterations).toBe(1);
    expect(cap.direction).toBe('normal');
    expect(cap.timing).toBe('linear');
    expect(seen.has(mover)).toBe(true);
  });

  test('forwards the direction and iterations onto the descriptor', () => {
    const mover = { __mover: true };
    let progress = 0;
    global.document = { body: { offsetHeight: 0, querySelectorAll: () => [mover] } };
    global.getComputedStyle = () => makeCS(progress);
    const seekFn = (p) => { progress = p; };

    const captured = [];
    pagxSampleTimeline(captured, new Set(), 2000, seekFn, 3, 5, 100, 'alternate', 50);
    expect(captured).toHaveLength(1);
    expect(captured[0].direction).toBe('alternate');
    expect(captured[0].iterations).toBe(3);
    expect(captured[0].delayMs).toBe(50);
  });

  test('an already-seen element is not double-captured', () => {
    const mover = { __mover: true };
    let progress = 0;
    global.document = { body: { offsetHeight: 0, querySelectorAll: () => [mover] } };
    global.getComputedStyle = () => makeCS(progress);
    const seekFn = (p) => { progress = p; };

    const captured = [];
    const seen = new Set([mover]);
    pagxSampleTimeline(captured, seen, 1000, seekFn, 1, 5, 100, 'normal', 0);
    expect(captured).toHaveLength(0);
  });
});

describe('pagxCollectGlobalSampled', () => {
  const origDocument = global.document;
  const origWindow = global.window;
  const origGetComputedStyle = global.getComputedStyle;
  afterEach(() => {
    global.document = origDocument;
    global.window = origWindow;
    global.getComputedStyle = origGetComputedStyle;
  });

  // Wire the WAAPI seek to a shared clock: the sole animation's currentTime
  // setter records the absolute seek time; getComputedStyle reads it back so the
  // mover's opacity tracks the timeline position. globalMs comes from the same
  // animation's computed timing, so measure + seek + sample all agree.
  function setup(globalMs) {
    const state = { clock: 0 };
    const anim = {
      effect: { getComputedTiming: () => ({ duration: globalMs, delay: 0 }) },
      pause() {},
    };
    Object.defineProperty(anim, 'currentTime', {
      get() { return state.clock; },
      set(v) { state.clock = v; },
    });
    const mover = { __mover: true };
    const staticEl = {};
    const head = makeContainer();
    global.window = {};
    global.document = {
      head,
      documentElement: head,
      getAnimations: () => [anim],
      createElement: () => makeStyleNode(),
      getElementById: () => null,
      body: { offsetHeight: 0, querySelectorAll: () => [mover, staticEl] },
    };
    global.getComputedStyle = (el) => makeCS(el === mover ? state.clock / globalMs : 1);
    return { mover, staticEl };
  }

  test('samples the whole page on the shared clock and captures the varying element', () => {
    const globalMs = 200;
    const { mover } = setup(globalMs);
    const captured = [];
    const seen = new Set();
    pagxCollectGlobalSampled(captured, seen, 3, 100);

    expect(captured).toHaveLength(1);
    const cap = captured[0];
    expect(cap.el).toBe(mover);
    expect(cap.durationMs).toBe(globalMs);
    expect(cap.iterations).toBe(1);
    expect(cap.direction).toBe('normal');
    expect(cap.timing).toBe('linear');
    // opacity = seekTime/globalMs == progress -> linear ramp -> endpoints only.
    expect(cap.keyframes.map((k) => k.offset)).toEqual([0, 1]);
    expect(cap.keyframes.map((k) => k.props.opacity)).toEqual(['0', '1']);
  });

  test('returns without capturing when the global duration is 0', () => {
    global.window = {};
    global.document = {
      head: makeContainer(),
      getAnimations: () => [],
      createElement: () => makeStyleNode(),
      body: { offsetHeight: 0, querySelectorAll: () => [{}] },
    };
    global.getComputedStyle = () => makeCS(1);
    const captured = [];
    pagxCollectGlobalSampled(captured, new Set(), 3, 100);
    expect(captured).toHaveLength(0);
  });
});

describe('pagxBuildTextOverlays', () => {
  // A cloneable text leaf. Each cloneNode(false) returns a fresh fake carrying a
  // style bag (setProperty stashes fill under _fill) so the emitted overlay's
  // per-value glyph / size / color can be asserted.
  function makeClone() {
    return {
      textContent: '',
      style: { fontSize: '', color: '', setProperty(k, v) { this['_' + k] = v; } },
      removeAttribute() {},
    };
  }

  function makeLeaf() {
    const inserted = [];
    const el = {
      isConnected: true,
      childNodes: [],
      nextSibling: null,
      parentNode: { insertBefore(node) { inserted.push(node); } },
      cloneNode() { return makeClone(); },
    };
    return { el, inserted };
  }

  test('materialises one overlay descriptor per segment with its snapshotted style', () => {
    const { el, inserted } = makeLeaf();
    const stops1 = [{ offset: 0, props: { opacity: '0' } }, { offset: 1, props: { opacity: '1' } }];
    const stops2 = [{ offset: 0, props: { opacity: '1' } }, { offset: 1, props: { opacity: '0' } }];
    const textDynamics = [{
      el,
      segments: [
        { text: '+1', fontSize: '20px', color: 'rgb(1, 2, 3)', fill: 'rgb(9, 9, 9)', stops: stops1 },
        { text: '+2', fontSize: '30px', color: 'rgb(4, 5, 6)', fill: 'none', stops: stops2 },
      ],
    }];
    const captured = [];
    pagxBuildTextOverlays(textDynamics, captured, 1500);

    expect(captured).toHaveLength(2);
    expect(inserted).toHaveLength(2);
    // Each overlay wraps the clone that was inserted, in segment order.
    expect(captured[0].el).toBe(inserted[0]);
    expect(captured[1].el).toBe(inserted[1]);
    expect(inserted[0].textContent).toBe('+1');
    expect(inserted[0].style.fontSize).toBe('20px');
    expect(inserted[0].style.color).toBe('rgb(1, 2, 3)');
    // A plain color fill is pinned; a `none` fill is skipped.
    expect(inserted[0].style._fill).toBe('rgb(9, 9, 9)');
    expect(inserted[1].style._fill).toBeUndefined();
    // The overlay carries the segment's opacity stops on the global window.
    expect(captured[0].keyframes).toBe(stops1);
    expect(captured[0].durationMs).toBe(1500);
    expect(captured[0].iterations).toBe(1);
    expect(captured[0].direction).toBe('normal');
    expect(captured[0].timing).toBe('linear');
  });

  test('no-op for an empty list or a detached element', () => {
    const captured = [];
    pagxBuildTextOverlays([], captured, 1000);
    expect(captured).toHaveLength(0);

    const detached = { el: { isConnected: true, parentNode: null }, segments: [{ text: 'x', stops: [] }] };
    pagxBuildTextOverlays([detached], captured, 1000);
    expect(captured).toHaveLength(0);
  });
});

describe('pagxBuildUseOverlays', () => {
  function makeUseClone() {
    return {
      attrs: {},
      setAttribute(k, v) { this.attrs[k] = v; },
      getAttributeNS() { return null; },
      removeAttribute() {},
      style: { removeProperty() {} },
    };
  }

  function makeUse() {
    const inserted = [];
    const el = {
      isConnected: true,
      nextSibling: null,
      attrs: {},
      setAttribute(k, v) { this.attrs[k] = v; },
      getAttributeNS() { return null; },
      removeAttribute() {},
      style: { removeProperty() {} },
      parentNode: { insertBefore(node) { inserted.push(node); } },
      cloneNode() { return makeUseClone(); },
    };
    return { el, inserted };
  }

  test('reuses the base for glyph 0 and clones the rest, each pointed at its href', () => {
    const { el, inserted } = makeUse();
    const stopsA = [{ offset: 0, props: { opacity: '1' } }, { offset: 1, props: { opacity: '0' } }];
    const stopsB = [{ offset: 0, props: { opacity: '0' } }, { offset: 1, props: { opacity: '1' } }];
    const useDynamics = [{
      el,
      segments: [{ href: '#a', stops: stopsA }, { href: '#b', stops: stopsB }],
    }];
    const captured = [];
    pagxBuildUseOverlays(useDynamics, captured, 1200);

    expect(captured).toHaveLength(2);
    // First glyph reuses the base element; second is a cloned sibling.
    expect(captured[0].el).toBe(el);
    expect(el.attrs.href).toBe('#a');
    expect(inserted).toHaveLength(1);
    expect(captured[1].el).toBe(inserted[0]);
    expect(inserted[0].attrs.href).toBe('#b');
    expect(captured[0].keyframes).toBe(stopsA);
    expect(captured[1].keyframes).toBe(stopsB);
    expect(captured[0].durationMs).toBe(1200);
    expect(captured[1].direction).toBe('normal');
  });

  test('no-op for an empty list', () => {
    const captured = [];
    pagxBuildUseOverlays([], captured, 1000);
    expect(captured).toHaveLength(0);
  });
});

describe('pagxEmitCaptured', () => {
  const origDocument = global.document;
  const origGetComputedStyle = global.getComputedStyle;
  afterEach(() => {
    global.document = origDocument;
    global.getComputedStyle = origGetComputedStyle;
  });

  test('injects @keyframes CSS and installs the animation shorthand', () => {
    const el = {
      isConnected: true,
      style: {},
      getBoundingClientRect: () => ({ width: 100, height: 50, left: 0, top: 0 }),
    };
    const captured = [{
      el,
      keyframes: [
        { offset: 0, props: { opacity: '0' } },
        { offset: 1, props: { opacity: '1' } },
      ],
      durationMs: 2000,
      delayMs: 0,
      iterations: Infinity,
      direction: 'normal',
      timing: 'linear',
    }];

    const styles = [];
    const head = makeContainer();
    global.document = {
      head,
      documentElement: head,
      createElement: () => { const n = makeStyleNode(); styles.push(n); return n; },
      getElementById: () => null,
      body: { offsetHeight: 0 },
    };
    // Base transform read under the animation-blocking <style>: report `none`.
    global.getComputedStyle = () => ({ transform: 'none' });

    const out = pagxEmitCaptured(captured, PAGX_ANIM_PREFIX, PAGX_ANIM_STYLE_ID);

    expect(out).toEqual({ count: 1, names: ['pagxAnim0'] });
    // The injected keyframes style carries the @keyframes rule + the opacity ramp.
    const kf = styles.find((s) => s.textContent.indexOf('@keyframes') !== -1);
    expect(kf).toBeTruthy();
    expect(kf.id).toBe(PAGX_ANIM_STYLE_ID);
    expect(kf.textContent).toContain('@keyframes pagxAnim0');
    expect(kf.textContent).toContain('0% { opacity: 0; }');
    expect(kf.textContent).toContain('100% { opacity: 1; }');
    // The element gets the canonical shorthand and a neutral base transform.
    expect(el.style.animation).toBe('pagxAnim0 2s linear 0s infinite normal both');
    expect(el.style.transform).toBe('none');
  });

  test('skips a detached element and reports zero', () => {
    const el = { isConnected: false, style: {}, getBoundingClientRect: () => ({ width: 0, height: 0 }) };
    const captured = [{
      el,
      keyframes: [{ offset: 0, props: { opacity: '0' } }, { offset: 1, props: { opacity: '1' } }],
      durationMs: 1000, delayMs: 0, iterations: 1, direction: 'normal', timing: 'linear',
    }];
    const head = makeContainer();
    global.document = {
      head, documentElement: head,
      createElement: () => makeStyleNode(),
      getElementById: () => null,
      body: { offsetHeight: 0 },
    };
    global.getComputedStyle = () => ({ transform: 'none' });
    expect(pagxEmitCaptured(captured, PAGX_ANIM_PREFIX, PAGX_ANIM_STYLE_ID))
      .toEqual({ count: 0, names: [] });
  });
});

describe('pagxAnimMain', () => {
  const origDocument = global.document;
  const origWindow = global.window;
  const origGetComputedStyle = global.getComputedStyle;
  afterEach(() => {
    global.document = origDocument;
    global.window = origWindow;
    global.getComputedStyle = origGetComputedStyle;
  });

  const OPTS = { sampleCount: 6, maxElements: 100, prefix: PAGX_ANIM_PREFIX, styleId: PAGX_ANIM_STYLE_ID };

  test('empty page (no animations) returns the zero result shape', () => {
    const head = makeContainer();
    global.window = {};
    global.document = {
      head,
      documentElement: head,
      getAnimations: () => [],
      createElement: () => makeStyleNode(),
      getElementById: () => null,
      body: { offsetHeight: 0, querySelectorAll: () => [] },
    };
    global.getComputedStyle = () => makeCS(1);
    expect(pagxAnimMain(OPTS)).toEqual({ count: 0, names: [] });
  });

  test('one varying element runs end-to-end to {count:1, names:[pagxAnim0]}', () => {
    const globalMs = 200;
    const state = { clock: 0 };
    const anim = {
      effect: { getComputedTiming: () => ({ duration: globalMs, delay: 0 }) },
      pause() {},
    };
    Object.defineProperty(anim, 'currentTime', {
      get() { return state.clock; },
      set(v) { state.clock = v; },
    });
    // A real-ish element: checkpoint reads its class/style attrs, restore clears
    // them, emit measures its box + writes the shorthand onto its inline style.
    const mover = {
      __mover: true,
      isConnected: true,
      style: {},
      // The global sampler also records text/href leaves; give it an empty,
      // non-SVG leaf so those paths see a static (skipped) node.
      children: [],
      namespaceURI: 'http://www.w3.org/1999/xhtml',
      textContent: '',
      getAttribute: () => null,
      setAttribute() {},
      removeAttribute() {},
      getBoundingClientRect: () => ({ width: 100, height: 50, left: 0, top: 0 }),
    };
    const head = makeContainer();
    global.window = {};
    global.document = {
      head,
      documentElement: head,
      getAnimations: () => [anim],
      createElement: () => makeStyleNode(),
      getElementById: () => null,
      body: { offsetHeight: 0, querySelectorAll: () => [mover] },
    };
    global.getComputedStyle = (el) => {
      const cs = makeCS(el === mover ? state.clock / globalMs : 1);
      return cs;
    };

    const out = pagxAnimMain(OPTS);
    expect(out).toEqual({ count: 1, names: ['pagxAnim0'] });
    expect(mover.style.animation).toBe('pagxAnim0 0.2s linear 0s 1 normal both');
  });
});
