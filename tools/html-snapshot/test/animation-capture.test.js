'use strict';

const {
  PAGX_ANIM_PREFIX,
  PAGX_ANIM_STYLE_ID,
  pagxExtractTranslate,
  pagxPickProp,
  pagxNormalizeProps,
  pagxFillOffsets,
  pagxOffsetFromKeyText,
  pagxParseTimeMs,
  pagxWhichVary,
  pagxReduceKeyframes,
  pagxNormalizeTiming,
  pagxResolveWaapiEasing,
  pagxBuildCanonicalAnimation,
  buildAnimationCapturePayload,
  capturePagxAnimationsOnPage,
} = require('../dist/lib/animation-capture');

describe('pagxExtractTranslate', () => {
  test('returns null for none / empty', () => {
    expect(pagxExtractTranslate('none')).toBeNull();
    expect(pagxExtractTranslate('')).toBeNull();
  });

  test('normalises translateX / translateY to translate(x, y)', () => {
    expect(pagxExtractTranslate('translateX(40px)')).toBe('translate(40px, 0px)');
    expect(pagxExtractTranslate('translateY(20px)')).toBe('translate(0px, 20px)');
  });

  test('passes through a two-arg translate', () => {
    expect(pagxExtractTranslate('translate(10px, 5px)')).toBe('translate(10px, 5px)');
  });

  test('decomposes a pure-translation 2D matrix', () => {
    expect(pagxExtractTranslate('matrix(1, 0, 0, 1, 12, 34)')).toBe('translate(12px, 34px)');
  });

  test('drops a matrix carrying rotation/scale', () => {
    expect(pagxExtractTranslate('matrix(2, 0, 0, 2, 12, 34)')).toBeNull();
  });

  test('drops rotate / scale (no translate component)', () => {
    expect(pagxExtractTranslate('rotate(45deg)')).toBeNull();
    expect(pagxExtractTranslate('scale(1.5)')).toBeNull();
  });

  test('keeps only the translate part of a compound transform', () => {
    expect(pagxExtractTranslate('translate(8px, 9px) rotate(45deg)')).toBe('translate(8px, 9px)');
  });
});

describe('pagxPickProp', () => {
  test('prefers kebab, falls back to camel', () => {
    expect(pagxPickProp({ 'background-color': 'red' }, 'background-color', 'backgroundColor'))
      .toBe('red');
    expect(pagxPickProp({ backgroundColor: 'blue' }, 'background-color', 'backgroundColor'))
      .toBe('blue');
  });

  test('returns null when neither key is present or non-empty', () => {
    expect(pagxPickProp({}, 'opacity', 'opacity')).toBeNull();
    expect(pagxPickProp({ opacity: '' }, 'opacity', 'opacity')).toBeNull();
  });
});

describe('pagxNormalizeProps', () => {
  test('keeps the runtime-playable subset only', () => {
    const out = pagxNormalizeProps({
      opacity: '0.5',
      transform: 'translateX(5px)',
      color: 'rgb(1, 2, 3)',
      backgroundColor: 'rgb(4, 5, 6)',
      width: '100px',
    });
    expect(out).toEqual({
      opacity: '0.5',
      transform: 'translate(5px, 0px)',
      color: 'rgb(1, 2, 3)',
      'background-color': 'rgb(4, 5, 6)',
    });
  });

  test('drops a non-translate transform but keeps other channels', () => {
    const out = pagxNormalizeProps({ opacity: '1', transform: 'rotate(10deg)' });
    expect(out).toEqual({ opacity: '1' });
  });
});

describe('pagxFillOffsets', () => {
  test('spaces missing offsets evenly across the range', () => {
    const norm = [
      { offset: null, props: {} },
      { offset: null, props: {} },
      { offset: null, props: {} },
    ];
    pagxFillOffsets(norm);
    expect(norm.map((k) => k.offset)).toEqual([0, 0.5, 1]);
  });

  test('a single missing offset becomes 0', () => {
    const norm = [{ offset: null, props: {} }];
    pagxFillOffsets(norm);
    expect(norm[0].offset).toBe(0);
  });
});

describe('pagxOffsetFromKeyText', () => {
  test('maps from / to / percentages', () => {
    expect(pagxOffsetFromKeyText('from')).toBe(0);
    expect(pagxOffsetFromKeyText('to')).toBe(1);
    expect(pagxOffsetFromKeyText('50%')).toBe(0.5);
  });

  test('returns null for unparsable text', () => {
    expect(pagxOffsetFromKeyText('middle')).toBeNull();
  });
});

describe('pagxParseTimeMs', () => {
  test('parses s and ms', () => {
    expect(pagxParseTimeMs('2s')).toBe(2000);
    expect(pagxParseTimeMs('500ms')).toBe(500);
  });

  test('returns 0 for unitless / empty', () => {
    expect(pagxParseTimeMs('5')).toBe(0);
    expect(pagxParseTimeMs('')).toBe(0);
  });
});

describe('pagxWhichVary', () => {
  test('returns only the properties that change across samples', () => {
    const samples = [
      { opacity: '0', transform: 'translate(0px, 0px)', color: 'red', 'background-color': 'blue' },
      { opacity: '1', transform: 'translate(0px, 0px)', color: 'red', 'background-color': 'blue' },
    ];
    expect(pagxWhichVary(samples)).toEqual(['opacity']);
  });
});

describe('pagxReduceKeyframes', () => {
  test('drops interior stops identical to both neighbours', () => {
    const norm = [
      { offset: 0, props: { opacity: '0' } },
      { offset: 0.5, props: { opacity: '0' } },
      { offset: 1, props: { opacity: '1' } },
    ];
    const out = pagxReduceKeyframes(norm);
    expect(out.map((k) => k.offset)).toEqual([0, 0.5, 1]);
  });

  test('keeps a varying interior stop', () => {
    const norm = [
      { offset: 0, props: { opacity: '0' } },
      { offset: 0.5, props: { opacity: '0.5' } },
      { offset: 1, props: { opacity: '1' } },
    ];
    expect(pagxReduceKeyframes(norm)).toHaveLength(3);
  });

  test('a flat middle run collapses', () => {
    const norm = [
      { offset: 0, props: { opacity: '0' } },
      { offset: 0.33, props: { opacity: '0' } },
      { offset: 0.66, props: { opacity: '0' } },
      { offset: 1, props: { opacity: '1' } },
    ];
    const out = pagxReduceKeyframes(norm);
    // first kept, the two identical interior collapse to one (the boundary), last kept.
    expect(out.length).toBeLessThan(4);
    expect(out[0].offset).toBe(0);
    expect(out[out.length - 1].offset).toBe(1);
  });
});

describe('pagxNormalizeTiming', () => {
  test('defaults empty to linear, passes others through', () => {
    expect(pagxNormalizeTiming('')).toBe('linear');
    expect(pagxNormalizeTiming('ease-in-out')).toBe('ease-in-out');
  });
});

describe('pagxResolveWaapiEasing', () => {
  // The function calls `getComputedStyle` in the page; in jest's node env we
  // stub it so the CSS-name branch is exercised end-to-end.
  const originalGetComputedStyle = global.getComputedStyle;
  afterEach(() => {
    global.getComputedStyle = originalGetComputedStyle;
  });

  test('CSSAnimation: reads animation-timing-function from computed style', () => {
    global.getComputedStyle = () => ({
      animationName: 'runEase',
      animationTimingFunction: 'ease-in-out',
    });
    const anim = { animationName: 'runEase' };
    const effect = { getTiming: () => ({ easing: 'linear' }) };
    const ct = { easing: 'linear' };
    const target = {};
    expect(pagxResolveWaapiEasing(anim, effect, ct, target)).toBe('ease-in-out');
  });

  test('CSSAnimation: picks the timing matching the animation-name index', () => {
    global.getComputedStyle = () => ({
      animationName: 'foo, runEase, bar',
      animationTimingFunction: 'linear, cubic-bezier(0.42, 0, 0.58, 1), ease-out',
    });
    const anim = { animationName: 'runEase' };
    const out = pagxResolveWaapiEasing(anim, {}, {}, {});
    expect(out).toBe('cubic-bezier(0.42, 0, 0.58, 1)');
  });

  test('WAAPI animation (no animationName): uses effect-level easing', () => {
    global.getComputedStyle = () => ({ animationName: '', animationTimingFunction: 'linear' });
    const anim = {};
    const effect = { getTiming: () => ({ easing: 'cubic-bezier(0.1, 0.2, 0.3, 0.4)' }) };
    const ct = { easing: 'linear' };
    expect(pagxResolveWaapiEasing(anim, effect, ct, {})).toBe('cubic-bezier(0.1, 0.2, 0.3, 0.4)');
  });

  test('falls back to ct.easing when effect-level easing is empty', () => {
    const anim = {};
    const effect = { getTiming: () => ({ easing: '' }) };
    const ct = { easing: 'ease-in' };
    expect(pagxResolveWaapiEasing(anim, effect, ct, {})).toBe('ease-in');
  });

  test('returns linear on any thrown lookup', () => {
    const anim = {};
    const effect = { getTiming: () => { throw new Error('boom'); } };
    const ct = {};
    expect(pagxResolveWaapiEasing(anim, effect, ct, {})).toBe('linear');
  });
});

describe('pagxBuildCanonicalAnimation', () => {
  const cap = {
    keyframes: [
      { offset: 0, props: { opacity: '0', transform: 'translate(0px, 0px)' } },
      { offset: 1, props: { opacity: '1', transform: 'translate(40px, 0px)' } },
    ],
    durationMs: 2000,
    delayMs: 0,
    iterations: Infinity,
    direction: 'normal',
    timing: 'linear',
  };

  test('emits @keyframes + animation shorthand for an infinite animation', () => {
    const out = pagxBuildCanonicalAnimation(cap, 0, PAGX_ANIM_PREFIX);
    expect(out.name).toBe('pagxAnim0');
    expect(out.keyframesCss).toContain('@keyframes pagxAnim0');
    expect(out.keyframesCss).toContain('0% { opacity: 0; transform: translate(0px, 0px); }');
    expect(out.keyframesCss).toContain('100% { opacity: 1; transform: translate(40px, 0px); }');
    expect(out.animationShorthand).toBe('pagxAnim0 2s linear 0s infinite normal');
  });

  test('finite iteration count is preserved verbatim', () => {
    const finite = Object.assign({}, cap, { iterations: 3 });
    const out = pagxBuildCanonicalAnimation(finite, 1, PAGX_ANIM_PREFIX);
    expect(out.animationShorthand).toBe('pagxAnim1 2s linear 0s 3 normal');
  });

  test('returns null when no subset declarations survive', () => {
    const empty = Object.assign({}, cap, {
      keyframes: [{ offset: 0, props: { width: '10px' } }],
    });
    expect(pagxBuildCanonicalAnimation(empty, 0, PAGX_ANIM_PREFIX)).toBeNull();
  });

  test('rounds offsets to one decimal percent', () => {
    const odd = Object.assign({}, cap, {
      keyframes: [
        { offset: 0, props: { opacity: '0' } },
        { offset: 1 / 3, props: { opacity: '0.5' } },
        { offset: 1, props: { opacity: '1' } },
      ],
    });
    const out = pagxBuildCanonicalAnimation(odd, 0, PAGX_ANIM_PREFIX);
    expect(out.keyframesCss).toContain('33.3% {');
  });
});

describe('buildAnimationCapturePayload', () => {
  test('produces a self-contained IIFE that calls pagxAnimMain with inlined opts', () => {
    const payload = buildAnimationCapturePayload({ sampleCount: 6, maxElements: 100 });
    expect(payload.startsWith('(() => {')).toBe(true);
    expect(payload).toContain('function pagxAnimMain');
    expect(payload).toContain('function pagxExtractTranslate');
    expect(payload).toContain(`"prefix":"${PAGX_ANIM_PREFIX}"`);
    expect(payload).toContain(`"styleId":"${PAGX_ANIM_STYLE_ID}"`);
    expect(payload).toContain('"sampleCount":6');
    expect(payload).toContain('"maxElements":100');
  });
});

describe('capturePagxAnimationsOnPage', () => {
  test('evaluates the payload and logs when animations are captured', async () => {
    const logs = [];
    const page = {
      evaluate: async () => ({ count: 2, names: ['pagxAnim0', 'pagxAnim1'] }),
    };
    const out = await capturePagxAnimationsOnPage(page, { logger: (m) => logs.push(m) });
    expect(out).toEqual({ count: 2, names: ['pagxAnim0', 'pagxAnim1'] });
    expect(logs.join('\n')).toMatch(/captured 2 animation\(s\): pagxAnim0, pagxAnim1/);
  });

  test('passes a string payload to page.evaluate', async () => {
    let received = null;
    const page = {
      evaluate: async (arg) => {
        received = arg;
        return { count: 0, names: [] };
      },
    };
    await capturePagxAnimationsOnPage(page);
    expect(typeof received).toBe('string');
    expect(received).toContain('pagxAnimMain');
  });

  test('degrades to a zero result and logs on evaluate failure', async () => {
    const logs = [];
    const page = {
      evaluate: async () => { throw new Error('boom'); },
    };
    const out = await capturePagxAnimationsOnPage(page, { logger: (m) => logs.push(m) });
    expect(out).toEqual({ count: 0, names: [] });
    expect(logs.join('\n')).toMatch(/animation capture failed: boom/);
  });
});
