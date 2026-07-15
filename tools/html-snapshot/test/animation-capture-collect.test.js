'use strict';

// Coverage for the in-browser DOM-collector functions. The jest env is `node`
// (no jsdom), so every DOM surface each collector touches is stubbed on the
// globals (`document`, `window`, `getComputedStyle`) with plain-object fakes
// exposing only the fields the function under test reads. All stubs are torn
// down in afterEach.

const {
  pagxCandidateElements,
  pagxDomCheckpoint,
  pagxDomRestore,
  pagxBuildKeyframesIndex,
  pagxCollectWAAPI,
  pagxCollectCSS,
  pagxGsapWindow,
  pagxCollectGsap,
  pagxCollectAnime,
  pagxCaptureAnimeInstances,
  pagxTransitionInstall,
  pagxCollectTransitions,
} = require('../dist/lib/animation-capture');

const savedDocument = global.document;
const savedWindow = global.window;
const savedGetComputedStyle = global.getComputedStyle;

afterEach(() => {
  global.document = savedDocument;
  global.window = savedWindow;
  global.getComputedStyle = savedGetComputedStyle;
});

// A minimal Element fake: attributes live in a backing map so getAttribute /
// setAttribute / removeAttribute behave like the real API (absent attr -> null).
function makeEl(attrs) {
  const map = Object.assign({}, attrs);
  return {
    nodeType: 1,
    isConnected: true,
    parentNode: null,
    getAttribute(name) {
      return Object.prototype.hasOwnProperty.call(map, name) && map[name] != null
        ? map[name]
        : null;
    },
    setAttribute(name, value) {
      map[name] = value;
    },
    removeAttribute(name) {
      delete map[name];
    },
  };
}

describe('pagxCandidateElements', () => {
  test('returns every element document.body.querySelectorAll reports', () => {
    const a = { nodeType: 1 };
    const b = { nodeType: 1 };
    const c = { nodeType: 1 };
    global.document = { body: { querySelectorAll: () => [a, b, c] } };
    expect(pagxCandidateElements(10)).toEqual([a, b, c]);
  });

  test('slices down to maxElements when the page has more', () => {
    const els = [{ nodeType: 1 }, { nodeType: 1 }, { nodeType: 1 }, { nodeType: 1 }];
    global.document = { body: { querySelectorAll: () => els } };
    expect(pagxCandidateElements(2)).toEqual([els[0], els[1]]);
  });

  test('returns [] when querySelectorAll throws', () => {
    global.document = {
      body: {
        querySelectorAll: () => {
          throw new Error('detached body');
        },
      },
    };
    expect(pagxCandidateElements(10)).toEqual([]);
  });
});

describe('pagxDomCheckpoint / pagxDomRestore', () => {
  test('restore reverts a class flip, drops an inline style, and removes an appended node', () => {
    const el1 = makeEl({ class: 'menu' });
    const el2 = makeEl({ class: 'panel' });
    let nodes = [el1, el2];
    global.document = { body: { querySelectorAll: () => nodes.slice() } };

    const cp = pagxDomCheckpoint();
    expect(cp.records).toHaveLength(2);
    expect(cp.set.has(el1)).toBe(true);
    expect(cp.set.has(el2)).toBe(true);

    // Timeline mutations: flip a scene class, write an inline style, append a node.
    el1.setAttribute('class', 'active');
    el1.setAttribute('style', 'opacity: 0');
    const removed = [];
    const parent = {
      removeChild(child) {
        removed.push(child);
        const i = nodes.indexOf(child);
        if (i >= 0) nodes.splice(i, 1);
        child.parentNode = null;
      },
    };
    const el3 = makeEl({ class: 'countdown' });
    el3.parentNode = parent;
    nodes = [el1, el2, el3];

    pagxDomRestore(cp);

    // Class flip reverted, timeline-added inline style removed (checkpoint had none).
    expect(el1.getAttribute('class')).toBe('menu');
    expect(el1.getAttribute('style')).toBeNull();
    // The appended node was dropped via parentNode.removeChild.
    expect(removed).toEqual([el3]);
    expect(nodes).toEqual([el1, el2]);
  });

  test('restore skips elements the timeline detached (isConnected === false)', () => {
    const el = makeEl({ class: 'orig' });
    global.document = { body: { querySelectorAll: () => [el] } };
    const cp = pagxDomCheckpoint();
    el.setAttribute('class', 'changed');
    el.isConnected = false;
    pagxDomRestore(cp);
    // Detached node is left as-is; the class is NOT restored.
    expect(el.getAttribute('class')).toBe('changed');
  });

  test('pagxDomRestore(null) is a no-op', () => {
    expect(() => pagxDomRestore(null)).not.toThrow();
  });
});

describe('pagxBuildKeyframesIndex', () => {
  test('indexes keyframes rules by name and skips cross-origin / rule-less sheets', () => {
    const kfRule = {
      type: 7,
      name: 'spin',
      cssRules: [{ keyText: '0%', style: { transform: 'rotate(0deg)' } }],
    };
    const goodSheet = { cssRules: [kfRule, { type: 1 }] };
    const crossOrigin = {
      get cssRules() {
        throw new Error('cross-origin security error');
      },
    };
    const emptySheet = { cssRules: null };
    global.document = { styleSheets: [goodSheet, crossOrigin, emptySheet] };

    const idx = pagxBuildKeyframesIndex();
    expect(Object.keys(idx)).toEqual(['spin']);
    expect(idx.spin).toBe(kfRule);
  });

  test('returns {} when there are no style sheets', () => {
    global.document = { styleSheets: [] };
    expect(pagxBuildKeyframesIndex()).toEqual({});
  });
});

describe('pagxCollectWAAPI', () => {
  function makeAnim(overrides) {
    const target = {
      nodeType: 1,
      getBoundingClientRect: () => ({ width: 100, height: 50 }),
    };
    const effect = {
      target,
      getKeyframes: () => [
        { opacity: '0', transform: 'translateX(0px)', offset: 0 },
        { opacity: '1', transform: 'translateX(40px)', offset: 1 },
      ],
      getComputedTiming: () => ({ duration: 1000, delay: 0, iterations: 1, direction: 'normal' }),
    };
    const anim = { effect };
    return Object.assign({ anim, effect, target }, overrides);
  }

  test('captures one descriptor and marks the target seen', () => {
    const { anim, target } = makeAnim();
    global.document = { getAnimations: () => [anim] };
    const captured = [];
    const seen = new Set();
    pagxCollectWAAPI(captured, seen);

    expect(captured).toHaveLength(1);
    expect(captured[0].el).toBe(target);
    expect(captured[0].durationMs).toBe(1000);
    expect(captured[0].iterations).toBe(1);
    expect(captured[0].direction).toBe('normal');
    // No animationName + no effect-level easing -> falls back to linear.
    expect(captured[0].timing).toBe('linear');
    expect(captured[0].keyframes[0].props).toEqual({ opacity: '0', transform: 'translate(0px, 0px)' });
    expect(captured[0].keyframes[1].props).toEqual({ opacity: '1', transform: 'translate(40px, 0px)' });
    expect(seen.has(target)).toBe(true);
  });

  test('does nothing when document.getAnimations is absent', () => {
    global.document = {};
    const captured = [];
    const seen = new Set();
    pagxCollectWAAPI(captured, seen);
    expect(captured).toHaveLength(0);
  });

  test('skips an animation whose computed duration is 0', () => {
    const { anim } = makeAnim();
    anim.effect.getComputedTiming = () => ({ duration: 0, delay: 0, iterations: 1, direction: 'normal' });
    global.document = { getAnimations: () => [anim] };
    const captured = [];
    const seen = new Set();
    pagxCollectWAAPI(captured, seen);
    expect(captured).toHaveLength(0);
    expect(seen.size).toBe(0);
  });

  test('skips a target already in seen', () => {
    const { anim, target } = makeAnim();
    global.document = { getAnimations: () => [anim] };
    const captured = [];
    const seen = new Set([target]);
    pagxCollectWAAPI(captured, seen);
    expect(captured).toHaveLength(0);
  });
});

describe('pagxCollectCSS', () => {
  function installSheetAndEl() {
    const kfRule = {
      type: 7,
      name: 'fadein',
      cssRules: [
        { keyText: '0%', style: { opacity: '0' } },
        { keyText: '100%', style: { opacity: '1' } },
      ],
    };
    const el = {
      nodeType: 1,
      getBoundingClientRect: () => ({ width: 100, height: 50 }),
    };
    global.document = {
      styleSheets: [{ cssRules: [kfRule] }],
      body: { querySelectorAll: () => [el] },
    };
    global.getComputedStyle = () => ({
      animationName: 'fadein',
      animationDuration: '2s',
      animationDelay: '0s',
      animationIterationCount: 'infinite',
      animationDirection: 'normal',
      animationTimingFunction: 'linear',
    });
    return el;
  }

  test('captures a descriptor for an element whose animation-name matches the index', () => {
    const el = installSheetAndEl();
    const captured = [];
    const seen = new Set();
    pagxCollectCSS(captured, seen, 100);

    expect(captured).toHaveLength(1);
    expect(captured[0].el).toBe(el);
    expect(captured[0].durationMs).toBe(2000);
    expect(captured[0].iterations).toBe(Infinity);
    expect(captured[0].direction).toBe('normal');
    expect(captured[0].timing).toBe('linear');
    expect(captured[0].keyframes).toEqual([
      { offset: 0, props: { opacity: '0' } },
      { offset: 1, props: { opacity: '1' } },
    ]);
    expect(seen.has(el)).toBe(true);
  });

  test('returns early (captures nothing) when the keyframes index is empty', () => {
    global.document = {
      styleSheets: [],
      body: {
        querySelectorAll: () => {
          throw new Error('should not be reached');
        },
      },
    };
    const captured = [];
    pagxCollectCSS(captured, new Set(), 100);
    expect(captured).toHaveLength(0);
  });
});

describe('pagxGsapWindow', () => {
  function gsapWith(child) {
    return { globalTimeline: { getChildren: () => [child] } };
  }

  test('reconstructs window, iterations and yoyo from a repeating yoyo child', () => {
    const child = {
      startTime: () => 0,
      duration: () => 2,
      repeat: () => 2,
      yoyo: () => true,
    };
    expect(pagxGsapWindow(gsapWith(child))).toEqual({
      windowSec: 2,
      infinite: false,
      yoyo: true,
      iterations: 3,
    });
  });

  test('a negative repeat marks the timeline infinite and folds in startTime', () => {
    const child = {
      startTime: () => 1,
      duration: () => 3,
      repeat: () => -1,
      yoyo: () => false,
    };
    expect(pagxGsapWindow(gsapWith(child))).toEqual({
      windowSec: 4,
      infinite: true,
      yoyo: false,
      iterations: 1,
    });
  });

  test('skips a child with a non-positive duration', () => {
    const child = {
      startTime: () => 0,
      duration: () => 0,
      repeat: () => 0,
      yoyo: () => false,
    };
    expect(pagxGsapWindow(gsapWith(child)).windowSec).toBe(0);
  });

  test('a throwing getChildren yields an empty window', () => {
    const gsap = {
      globalTimeline: {
        getChildren: () => {
          throw new Error('boom');
        },
      },
    };
    expect(pagxGsapWindow(gsap)).toEqual({
      windowSec: 0,
      infinite: false,
      yoyo: false,
      iterations: 1,
    });
  });
});

describe('pagxCollectGsap', () => {
  test('returns early when window.gsap is absent', () => {
    global.window = {};
    const captured = [];
    pagxCollectGsap(captured, new Set(), 4, 50);
    expect(captured).toHaveLength(0);
  });

  test('drives the sampler over a valid gsap window (no candidate elements)', () => {
    const child = {
      startTime: () => 0,
      duration: () => 2,
      repeat: () => 0,
      yoyo: () => false,
    };
    const times = [];
    let pauseCount = 0;
    const tl = {
      getChildren: () => [child],
      pause: () => {
        pauseCount += 1;
      },
      time: (t) => {
        times.push(t);
      },
    };
    global.window = { gsap: { globalTimeline: tl } };
    global.document = { body: { querySelectorAll: () => [], offsetHeight: 0 } };
    global.getComputedStyle = () => ({});

    const captured = [];
    pagxCollectGsap(captured, new Set(), 3, 50);

    // No candidate elements -> nothing captured, but the sampler must have seeked
    // the timeline across the window (3 samples over [0, 2s]) and reset to 0.
    expect(captured).toHaveLength(0);
    expect(times).toEqual([0, 1, 2, 0]);
    expect(pauseCount).toBeGreaterThanOrEqual(2);
  });
});

describe('pagxCollectAnime', () => {
  test('returns early when window.anime is absent', () => {
    global.window = {};
    const captured = [];
    pagxCollectAnime(captured, new Set(), 4, 50);
    expect(captured).toHaveLength(0);
  });

  test('returns early when anime has no running list', () => {
    global.window = { anime: {} };
    const captured = [];
    pagxCollectAnime(captured, new Set(), 4, 50);
    expect(captured).toHaveLength(0);
  });

  test('pauses and seeks each running instance (no candidate elements)', () => {
    const seeks = [];
    let pauseCount = 0;
    const inst = {
      duration: 1000,
      loop: false,
      direction: 'normal',
      pause: () => {
        pauseCount += 1;
      },
      seek: (t) => {
        seeks.push(t);
      },
    };
    global.window = { anime: { running: [inst] } };
    global.document = { body: { querySelectorAll: () => [], offsetHeight: 0 } };
    global.getComputedStyle = () => ({});

    const captured = [];
    pagxCollectAnime(captured, new Set(), 3, 50);

    expect(captured).toHaveLength(0);
    expect(pauseCount).toBe(1);
    // 3 samples over [0, 1000ms] plus the final reset seek to 0.
    expect(seeks).toEqual([0, 500, 1000, 0]);
  });
});

describe('pagxCaptureAnimeInstances', () => {
  test('snapshots a copy of anime.running onto window.__pagxBaselineAnime', () => {
    const running = [{ id: 1 }, { id: 2 }];
    global.window = { anime: { running } };
    pagxCaptureAnimeInstances();
    expect(global.window.__pagxBaselineAnime).toEqual(running);
    // It must be a copy, not the same array reference.
    expect(global.window.__pagxBaselineAnime).not.toBe(running);
  });

  test('records an empty baseline when anime is absent', () => {
    global.window = {};
    pagxCaptureAnimeInstances();
    expect(global.window.__pagxBaselineAnime).toEqual([]);
  });
});

describe('pagxTransitionInstall / pagxCollectTransitions', () => {
  test('installs a single transitionrun listener and is idempotent', () => {
    const listeners = [];
    global.window = {};
    global.document = {
      addEventListener: (type, fn, capture) => listeners.push({ type, fn, capture }),
    };
    pagxTransitionInstall();
    expect(listeners).toHaveLength(1);
    expect(listeners[0].type).toBe('transitionrun');
    expect(listeners[0].capture).toBe(true);
    expect(global.window.__pagxTransitions).toBeDefined();

    // Second install is guarded by the presence of __pagxTransitions.
    pagxTransitionInstall();
    expect(listeners).toHaveLength(1);
  });

  test('records a transitionrun start and pairs it with the settled end value', () => {
    const listeners = [];
    global.window = {};
    global.document = {
      addEventListener: (type, fn) => listeners.push({ type, fn }),
    };
    // A single computed-style report driven by a mutable opacity: the start value
    // at transitionrun time, then the settled resting value at capture time.
    let opacityVal = '0';
    global.getComputedStyle = () => ({
      transitionProperty: 'opacity',
      transitionDuration: '0.6s',
      transitionDelay: '0s',
      transitionTimingFunction: 'ease',
      transform: 'none',
      getPropertyValue: (p) => (p === 'opacity' ? opacityVal : ''),
    });

    pagxTransitionInstall();
    const record = listeners[0].fn;

    const el = {
      nodeType: 1,
      isConnected: true,
      getBoundingClientRect: () => ({ width: 100, height: 50 }),
    };
    record({ target: el, propertyName: 'opacity' });

    // The element settles to opacity 1 by capture time.
    opacityVal = '1';
    const captured = [];
    const seen = new Set();
    pagxCollectTransitions(captured, seen);

    expect(captured).toHaveLength(1);
    expect(captured[0].el).toBe(el);
    expect(captured[0].durationMs).toBe(600);
    expect(captured[0].timing).toBe('ease');
    expect(captured[0].keyframes).toEqual([
      { offset: 0, props: { opacity: '0' } },
      { offset: 1, props: { opacity: '1' } },
    ]);
    expect(seen.has(el)).toBe(true);
  });

  test('pagxCollectTransitions returns early when no recorder store exists', () => {
    global.window = {};
    const captured = [];
    pagxCollectTransitions(captured, new Set());
    expect(captured).toHaveLength(0);
  });
});
