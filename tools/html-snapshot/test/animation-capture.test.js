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
  pagxParseColorChannels,
  pagxParseTranslateXY,
  pagxStopScalarSeries,
  pagxRdpKeep,
  pagxDecimateStops,
  pagxNormalizeTiming,
  pagxResolveWaapiEasing,
  pagxBuildCanonicalAnimation,
  pagxTransitionDescriptorFromBags,
  pagxGlobalSampleCount,
  buildAnimationCapturePayload,
  capturePagxAnimationsOnPage,
  PAGX_TRANSITION_INIT_SCRIPT,
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

  test('keeps the translate component of a matrix carrying rotation/scale', () => {
    expect(pagxExtractTranslate('matrix(2, 0, 0, 2, 12, 34)')).toBe('translate(12px, 34px)');
    expect(pagxExtractTranslate('matrix(0, 1, -1, 0, 5, 6)')).toBe('translate(5px, 6px)');
  });

  test('keeps the translate component of a matrix3d carrying scale', () => {
    expect(
      pagxExtractTranslate('matrix3d(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 7, 8, 0, 1)'),
    ).toBe('translate(7px, 8px)');
  });

  test('drops rotate / scale (no translate component)', () => {
    expect(pagxExtractTranslate('rotate(45deg)')).toBeNull();
    expect(pagxExtractTranslate('scale(1.5)')).toBeNull();
  });

  test('keeps only the translate part of a compound transform', () => {
    expect(pagxExtractTranslate('translate(8px, 9px) rotate(45deg)')).toBe('translate(8px, 9px)');
  });

  test('resolves percent translate against the box dimensions', () => {
    expect(pagxExtractTranslate('translate(-50%, 0px)', { width: 200, height: 80 }))
      .toBe('translate(-100px, 0px)');
    expect(pagxExtractTranslate('translateY(25%)', { width: 100, height: 80 }))
      .toBe('translate(0px, 20px)');
  });

  test('forwards percent translate verbatim when no box is provided', () => {
    expect(pagxExtractTranslate('translate(-50%, 0px)')).toBe('translate(-50%, 0px)');
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

describe('pagxParseColorChannels', () => {
  test('parses rgb and rgba', () => {
    expect(pagxParseColorChannels('rgb(255, 0, 128)')).toEqual([255, 0, 128, 1]);
    expect(pagxParseColorChannels('rgba(10, 20, 30, 0.5)')).toEqual([10, 20, 30, 0.5]);
  });
  test('returns null for non-rgb values', () => {
    expect(pagxParseColorChannels('red')).toBeNull();
    expect(pagxParseColorChannels('#fff')).toBeNull();
    expect(pagxParseColorChannels('')).toBeNull();
  });
});

describe('pagxParseTranslateXY', () => {
  test('parses x and y px', () => {
    expect(pagxParseTranslateXY('translate(12px, -4px)')).toEqual([12, -4]);
  });
  test('defaults missing y to 0', () => {
    expect(pagxParseTranslateXY('translate(8px)')).toEqual([8, 0]);
  });
  test('returns null when no x parses', () => {
    expect(pagxParseTranslateXY('none')).toBeNull();
    expect(pagxParseTranslateXY('')).toBeNull();
  });
});

describe('pagxStopScalarSeries', () => {
  test('decomposes tracked props into per-channel scalar series', () => {
    const stops = [
      { offset: 0, props: { opacity: '0', transform: 'translate(0px, 0px)' } },
      { offset: 1, props: { opacity: '1', transform: 'translate(40px, 10px)' } },
    ];
    const series = pagxStopScalarSeries(stops);
    expect(series.opacity).toEqual([0, 1]);
    // transform decomposes into the six affine components; a pure translate
    // folds to [1,0,0,1,tx,ty], so m4/m5 carry the translation.
    expect(series.m4).toEqual([0, 40]);
    expect(series.m5).toEqual([0, 10]);
  });
});

describe('pagxRdpKeep', () => {
  test('collapses a perfectly linear ramp to its endpoints', () => {
    const offsets = [0, 0.25, 0.5, 0.75, 1];
    const values = [0, 25, 50, 75, 100];
    const keep = pagxRdpKeep(offsets, values, 0.02);
    expect(keep).toEqual([true, false, false, false, true]);
  });

  test('keeps the apex of a sharply curved series', () => {
    // A spike at the middle deviates far from the 0→0 chord.
    const offsets = [0, 0.5, 1];
    const values = [0, 100, 0];
    const keep = pagxRdpKeep(offsets, values, 0.02);
    expect(keep).toEqual([true, true, true]);
  });

  test('a flat channel keeps only endpoints', () => {
    const offsets = [0, 0.5, 1];
    const values = [7, 7, 7];
    expect(pagxRdpKeep(offsets, values, 0.02)).toEqual([true, false, true]);
  });
});

describe('pagxDecimateStops', () => {
  test('collapses a linear opacity ramp to two keyframes', () => {
    const stops = [];
    for (let i = 0; i <= 10; i++) {
      stops.push({ offset: i / 10, props: { opacity: String(i / 10) } });
    }
    const out = pagxDecimateStops(stops);
    expect(out.map((s) => s.offset)).toEqual([0, 1]);
  });

  test('keeps interior points of an eased (non-linear) curve', () => {
    // ease-out-ish: fast then slow → bows away from the chord.
    const stops = [];
    for (let i = 0; i <= 10; i++) {
      const t = i / 10;
      stops.push({ offset: t, props: { opacity: String(Math.sqrt(t)) } });
    }
    const out = pagxDecimateStops(stops);
    expect(out.length).toBeGreaterThan(2);
    expect(out.length).toBeLessThan(stops.length);
    expect(out[0].offset).toBe(0);
    expect(out[out.length - 1].offset).toBe(1);
  });

  test('unions keep-points across independent channels', () => {
    // opacity is linear (collapses) but x has a mid-curve kink → that index survives.
    const stops = [
      { offset: 0, props: { opacity: '0', transform: 'translate(0px, 0px)' } },
      { offset: 0.5, props: { opacity: '0.5', transform: 'translate(80px, 0px)' } },
      { offset: 1, props: { opacity: '1', transform: 'translate(100px, 0px)' } },
    ];
    const out = pagxDecimateStops(stops);
    expect(out.map((s) => s.offset)).toContain(0.5);
  });

  test('returns input unchanged for <= 2 stops', () => {
    const stops = [
      { offset: 0, props: { opacity: '0' } },
      { offset: 1, props: { opacity: '1' } },
    ];
    expect(pagxDecimateStops(stops)).toHaveLength(2);
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
    expect(out.animationShorthand).toBe('pagxAnim0 2s linear 0s infinite normal both');
  });

  test('finite iteration count is preserved verbatim', () => {
    const finite = Object.assign({}, cap, { iterations: 3 });
    const out = pagxBuildCanonicalAnimation(finite, 1, PAGX_ANIM_PREFIX);
    expect(out.animationShorthand).toBe('pagxAnim1 2s linear 0s 3 normal both');
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

describe('pagxTransitionDescriptorFromBags', () => {
  test('builds a 2-stop descriptor for an opacity transition', () => {
    const out = pagxTransitionDescriptorFromBags(
      { opacity: '0' }, { opacity: '1' }, null, 600, 0, 'ease',
    );
    expect(out).not.toBeNull();
    expect(out.durationMs).toBe(600);
    expect(out.timing).toBe('ease');
    expect(out.iterations).toBe(1);
    expect(out.keyframes).toEqual([
      { offset: 0, props: { opacity: '0' } },
      { offset: 1, props: { opacity: '1' } },
    ]);
  });

  test('treats a one-sided transform as translate(0px, 0px) on the missing side', () => {
    const out = pagxTransitionDescriptorFromBags(
      { transform: 'none' }, { transform: 'translate(40px, 0px)' }, null, 300, 0, 'linear',
    );
    expect(out.keyframes[0].props.transform).toBe('translate(0px, 0px)');
    expect(out.keyframes[1].props.transform).toBe('translate(40px, 0px)');
  });

  test('merges multiple changing channels into one descriptor', () => {
    const out = pagxTransitionDescriptorFromBags(
      { opacity: '0', transform: 'translate(0px, 20px)' },
      { opacity: '1', transform: 'translate(0px, 0px)' },
      null, 500, 100, 'ease-out',
    );
    expect(out.delayMs).toBe(100);
    expect(out.keyframes[0].props).toEqual({ opacity: '0', transform: 'translate(0px, 20px)' });
    expect(out.keyframes[1].props).toEqual({ opacity: '1', transform: 'translate(0px, 0px)' });
  });

  test('returns null when no tracked channel changes', () => {
    expect(pagxTransitionDescriptorFromBags(
      { opacity: '1' }, { opacity: '1' }, null, 300, 0, 'linear',
    )).toBeNull();
  });

  test('returns null for a non-positive duration', () => {
    expect(pagxTransitionDescriptorFromBags(
      { opacity: '0' }, { opacity: '1' }, null, 0, 0, 'linear',
    )).toBeNull();
  });

  test('drops a channel defined on only one side (non-transform)', () => {
    // `color` present only in `to` cannot describe a tween (no start value).
    const out = pagxTransitionDescriptorFromBags(
      { opacity: '0' }, { opacity: '1', color: 'rgb(255, 0, 0)' }, null, 300, 0, 'linear',
    );
    expect(out.keyframes[0].props).toEqual({ opacity: '0' });
    expect(out.keyframes[1].props).toEqual({ opacity: '1' });
  });
});

describe('PAGX_TRANSITION_INIT_SCRIPT', () => {
  test('is a self-contained IIFE that installs the transitionrun recorder', () => {
    expect(typeof PAGX_TRANSITION_INIT_SCRIPT).toBe('string');
    expect(PAGX_TRANSITION_INIT_SCRIPT).toContain('function pagxTransitionInstall');
    expect(PAGX_TRANSITION_INIT_SCRIPT).toContain('pagxTransitionInstall();');
    expect(PAGX_TRANSITION_INIT_SCRIPT).toContain('transitionrun');
    // bundles its two helper dependencies
    expect(PAGX_TRANSITION_INIT_SCRIPT).toContain('function pagxSplitTopLevelCommas');
    expect(PAGX_TRANSITION_INIT_SCRIPT).toContain('function pagxParseTimeMs');
  });
});

describe('pagxGlobalSampleCount', () => {
  test('keeps at least the base count for a short timeline', () => {
    // 500ms @ 40ms → 14 wanted, floored back up to the base 24.
    expect(pagxGlobalSampleCount(500, 24)).toBe(24);
    // 1s @ 40ms → 26 (just above the floor); step stays ~40ms.
    expect(pagxGlobalSampleCount(1000, 24)).toBe(26);
  });

  test('scales up for a long timeline so the step stays near stepMs', () => {
    // 28.9s @ 40ms → ~724 samples: the fixed 24 would resolve only ~1.25s,
    // collapsing a 45ms staggered entrance into one step.
    const n = pagxGlobalSampleCount(28900, 24);
    expect(n).toBe(724);
    // resulting temporal resolution is ~40ms, not ~1.25s
    expect(28900 / (n - 1)).toBeCloseTo(40, 0);
  });

  test('caps at maxCount for a pathologically long timeline', () => {
    expect(pagxGlobalSampleCount(10 * 60 * 1000, 24)).toBe(900);
    expect(pagxGlobalSampleCount(1e9, 24, 40, 900)).toBe(900);
  });

  test('honours custom stepMs / maxCount and falls back on a non-positive duration', () => {
    expect(pagxGlobalSampleCount(2000, 24, 20)).toBe(101);
    expect(pagxGlobalSampleCount(0, 24)).toBe(24);
    expect(pagxGlobalSampleCount(-5, 24)).toBe(24);
    expect(pagxGlobalSampleCount(Infinity, 24)).toBe(24);
  });
});

describe('buildAnimationCapturePayload', () => {
  test('produces a self-contained IIFE that calls pagxAnimMain with inlined opts', () => {
    const payload = buildAnimationCapturePayload({ sampleCount: 6, maxElements: 100 });
    expect(payload.startsWith('(() => {')).toBe(true);
    expect(payload).toContain('function pagxAnimMain');
    expect(payload).toContain('function pagxExtractTranslate');
    expect(payload).toContain('function pagxCollectTransitions');
    expect(payload).toContain('function pagxGlobalSampleCount');
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
