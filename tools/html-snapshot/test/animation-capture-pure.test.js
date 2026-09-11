'use strict';

const {
  PAGX_ANIM_PREFIX,
  pagxExtractTransform,
  pagxTransformToMatrix6,
  pagxParseFilterChannels,
  pagxParseTranslateXY,
  pagxStopScalarSeries,
  pagxBuildCanonicalAnimation,
  pagxClipFullBoxD,
  pagxResolveClipNoneSamples,
  pagxElementUsesStepTiming,
  pagxClipNormalizeD,
} = require('../dist/lib/animation-capture');

describe('pagxExtractTransform', () => {
  // The compound-string branch resolves the chain via a throwaway element, so it
  // touches `document` / `getComputedStyle`; the node env has neither, so we stub
  // them and restore afterwards.
  const originalDocument = global.document;
  const originalGetComputedStyle = global.getComputedStyle;
  afterEach(() => {
    global.document = originalDocument;
    global.getComputedStyle = originalGetComputedStyle;
  });

  test('an identity-linear matrix folds to a pure translate', () => {
    expect(pagxExtractTransform('matrix(1, 0, 0, 1, 12, 34)')).toBe('translate(12px, 34px)');
  });

  test('a matrix carrying scale is passed through as matrix(...)', () => {
    expect(pagxExtractTransform('matrix(2, 0, 0, 2, 12, 34)')).toBe('matrix(2, 0, 0, 2, 12, 34)');
  });

  test('matrix3d delegates to the translate-only extraction', () => {
    expect(
      pagxExtractTransform('matrix3d(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 7, 8, 0, 1)'),
    ).toBe('translate(7px, 8px)');
  });

  test('a friendly pure-translate string reuses the translate path', () => {
    expect(pagxExtractTransform('translate(8px, 9px)')).toBe('translate(8px, 9px)');
  });

  test('a compound scale/rotate resolves through the createElement probe', () => {
    // The probe appends a throwaway <div>, reads its computed matrix, then removes it.
    global.document = {
      createElement: () => ({ style: {}, remove() {} }),
      body: { appendChild() {} },
    };
    global.getComputedStyle = () => ({ transform: 'matrix(2, 0, 0, 2, 10, 20)' });
    expect(pagxExtractTransform('translate(10px, 20px) scale(2)')).toBe('matrix(2, 0, 0, 2, 10, 20)');
  });

  test('a probe resolving to none falls through to translate-only', () => {
    global.document = {
      createElement: () => ({ style: {}, remove() {} }),
      body: { appendChild() {} },
    };
    global.getComputedStyle = () => ({ transform: 'none' });
    expect(pagxExtractTransform('translate(10px, 20px) scale(2)')).toBe('translate(10px, 20px)');
  });

  test('a throwing probe is caught and falls through to translate-only', () => {
    global.document = { createElement() { throw new Error('no dom'); } };
    expect(pagxExtractTransform('translate(10px, 20px) rotate(45deg)')).toBe('translate(10px, 20px)');
  });
});

describe('pagxTransformToMatrix6', () => {
  test('a matrix(6) splits into its six affine parts', () => {
    expect(pagxTransformToMatrix6('matrix(2, 0, 0, 3, 12, 34)')).toEqual([2, 0, 0, 3, 12, 34]);
  });

  test('a translate folds to [1,0,0,1,tx,ty]', () => {
    expect(pagxTransformToMatrix6('translate(12px, -4px)')).toEqual([1, 0, 0, 1, 12, -4]);
  });

  test('returns null for a value that is neither form', () => {
    expect(pagxTransformToMatrix6('rotate(45deg)')).toBeNull();
    expect(pagxTransformToMatrix6('')).toBeNull();
  });
});

describe('pagxParseFilterChannels', () => {
  test('none / empty yields no shadows and no blur', () => {
    expect(pagxParseFilterChannels('none')).toEqual({ shadows: [], fblur: 0 });
    expect(pagxParseFilterChannels('')).toEqual({ shadows: [], fblur: 0 });
  });

  test('a single drop-shadow captures colour, offsets and blur', () => {
    const f = pagxParseFilterChannels('drop-shadow(rgb(255, 0, 0) 2px 3px 4px)');
    expect(f.fblur).toBe(0);
    expect(f.shadows).toEqual([{ fdx: 2, fdy: 3, fdb: 4, fdr: 255, fdg: 0, fdbl: 0, fda: 1 }]);
  });

  test('a drop-shadow without an explicit colour defaults to opaque black', () => {
    const f = pagxParseFilterChannels('drop-shadow(2px 3px 4px)');
    expect(f.shadows).toEqual([{ fdx: 2, fdy: 3, fdb: 4, fdr: 0, fdg: 0, fdbl: 0, fda: 1 }]);
  });

  test('stacked drop-shadows are kept in author order', () => {
    const f = pagxParseFilterChannels(
      'drop-shadow(rgb(255, 0, 0) 1px 0px 0px) drop-shadow(rgb(0, 0, 255) 2px 0px 0px)');
    expect(f.shadows).toHaveLength(2);
    expect(f.shadows[0].fdx).toBe(1);
    expect([f.shadows[0].fdr, f.shadows[0].fdg, f.shadows[0].fdbl]).toEqual([255, 0, 0]);
    expect(f.shadows[1].fdx).toBe(2);
    expect([f.shadows[1].fdr, f.shadows[1].fdg, f.shadows[1].fdbl]).toEqual([0, 0, 255]);
  });

  test('multiple blur() functions fold to the maximum radius', () => {
    const f = pagxParseFilterChannels('blur(3px) blur(7px)');
    expect(f.fblur).toBe(7);
    expect(f.shadows).toEqual([]);
  });

  test('an unrecognised function (brightness) is ignored', () => {
    expect(pagxParseFilterChannels('brightness(1.5)')).toEqual({ shadows: [], fblur: 0 });
  });
});

describe('pagxParseTranslateXY', () => {
  test('returns null when no translate() matches', () => {
    expect(pagxParseTranslateXY('none')).toBeNull();
    expect(pagxParseTranslateXY('')).toBeNull();
  });

  test('returns null when the x value does not parse', () => {
    expect(pagxParseTranslateXY('translate(abc, 5px)')).toBeNull();
  });

  test('a one-arg translate defaults y to 0', () => {
    expect(pagxParseTranslateXY('translate(8px)')).toEqual([8, 0]);
  });
});

describe('pagxStopScalarSeries', () => {
  test('opacity plus a matrix / translate transform decompose per affine channel', () => {
    const stops = [
      { offset: 0, props: { opacity: '0.25', transform: 'matrix(2, 0, 0, 3, 10, 20)' } },
      { offset: 1, props: { opacity: '0.75', transform: 'translate(30px, 40px)' } },
    ];
    const series = pagxStopScalarSeries(stops);
    expect(series.opacity).toEqual([0.25, 0.75]);
    expect(series.m0).toEqual([2, 1]);
    expect(series.m3).toEqual([3, 1]);
    expect(series.m4).toEqual([10, 30]);
    expect(series.m5).toEqual([20, 40]);
  });

  test('colour / background / fill / stroke / dashoffset each split into their channels', () => {
    const stops = [
      {
        offset: 0,
        props: {
          color: 'rgb(10, 20, 30)',
          'background-color': 'rgb(40, 50, 60)',
          fill: 'rgb(1, 2, 3)',
          stroke: 'rgb(4, 5, 6)',
          'stroke-dashoffset': '100',
        },
      },
      {
        offset: 1,
        props: {
          color: 'rgba(70, 80, 90, 0.5)',
          'background-color': 'rgb(11, 12, 13)',
          fill: 'rgb(14, 15, 16)',
          stroke: 'rgb(17, 18, 19)',
          'stroke-dashoffset': '250',
        },
      },
    ];
    const series = pagxStopScalarSeries(stops);
    expect([series.cr, series.cg, series.cb, series.ca]).toEqual([[10, 70], [20, 80], [30, 90], [1, 0.5]]);
    expect([series.br, series.bg, series.bb, series.ba]).toEqual([[40, 11], [50, 12], [60, 13], [1, 1]]);
    expect([series.flr, series.flg, series.flb, series.fla]).toEqual([[1, 14], [2, 15], [3, 16], [1, 1]]);
    expect([series.skr, series.skg, series.skb, series.ska]).toEqual([[4, 17], [5, 18], [6, 19], [1, 1]]);
    expect(series.sdo).toEqual([100, 250]);
  });

  test('a filter shadow slot reads 0 on the frame where the shadow is absent', () => {
    const stops = [
      { offset: 0, props: { filter: 'drop-shadow(rgb(255, 0, 0) 2px 4px 6px)' } },
      { offset: 1, props: { filter: 'none' } },
    ];
    const series = pagxStopScalarSeries(stops);
    expect(series.fd0x).toEqual([2, 0]);
    expect(series.fd0b).toEqual([6, 0]);
    expect(series.fd0r).toEqual([255, 0]);
    expect(series.fd0a).toEqual([1, 0]);
    expect(series.fblur).toEqual([0, 0]);
  });

  test('a clip-path path() exposes every coordinate as its own scalar channel', () => {
    const stops = [
      { offset: 0, props: { 'clip-path': 'path("M 0 0 L 10 0 L 10 20 L 0 20 Z")' } },
      { offset: 1, props: { 'clip-path': 'path("M 0 0 L 30 0 L 30 40 L 0 40 Z")' } },
    ];
    const series = pagxStopScalarSeries(stops);
    expect(series.cp0).toEqual([0, 0]);
    expect(series.cp2).toEqual([10, 30]);
    expect(series.cp5).toEqual([20, 40]);
  });
});

describe('pagxBuildCanonicalAnimation', () => {
  test('a descriptor hitting every decl branch emits each declaration + infinite', () => {
    const props = {
      opacity: '0',
      transform: 'translate(0px, 0px)',
      color: 'rgb(0, 0, 0)',
      'background-color': 'rgb(1, 1, 1)',
      filter: 'none',
      'clip-path': 'path("M 0 0 L 10 0 L 10 10 Z")',
      fill: 'rgb(2, 2, 2)',
      stroke: 'rgb(3, 3, 3)',
      'stroke-dashoffset': '0',
    };
    const cap = {
      keyframes: [
        { offset: 0, props },
        { offset: 1, props: Object.assign({}, props, { opacity: '1' }) },
      ],
      durationMs: 1000,
      delayMs: 0,
      iterations: Infinity,
      direction: 'normal',
      timing: 'linear',
    };
    const out = pagxBuildCanonicalAnimation(cap, 0, PAGX_ANIM_PREFIX);
    expect(out.name).toBe('pagxAnim0');
    for (const decl of ['opacity:', 'transform:', 'color:', 'background-color:', 'filter:',
      'clip-path:', 'fill:', 'stroke:', 'stroke-dashoffset:']) {
      expect(out.keyframesCss).toContain(decl);
    }
    expect(out.animationShorthand).toBe('pagxAnim0 1s linear 0s infinite normal both');
  });

  test('returns null when no stop yields any tracked declaration', () => {
    const cap = {
      keyframes: [{ offset: 0, props: { width: '5px' } }, { offset: 1, props: { width: '9px' } }],
      durationMs: 1000,
      delayMs: 0,
      iterations: 1,
      direction: 'normal',
      timing: 'linear',
    };
    expect(pagxBuildCanonicalAnimation(cap, 0, PAGX_ANIM_PREFIX)).toBeNull();
  });
});

describe('pagxClipFullBoxD', () => {
  const originalGetComputedStyle = global.getComputedStyle;
  afterEach(() => { global.getComputedStyle = originalGetComputedStyle; });

  test('uses the layout offset box when present', () => {
    const el = { offsetWidth: 200, offsetHeight: 100 };
    expect(pagxClipFullBoxD(el)).toBe('M 0 0 L 200 0 L 200 100 L 0 100 Z');
  });

  test('falls back to the computed used width/height for a node with no offset box', () => {
    global.getComputedStyle = () => ({ width: '150px', height: '75px' });
    const el = { offsetWidth: 0, offsetHeight: 0, getBoundingClientRect: () => ({ width: 0, height: 0 }) };
    expect(pagxClipFullBoxD(el)).toBe('M 0 0 L 150 0 L 150 75 L 0 75 Z');
  });

  test('falls back to getBoundingClientRect when the computed size is unusable', () => {
    global.getComputedStyle = () => ({ width: 'auto', height: 'auto' });
    const el = { offsetWidth: 0, offsetHeight: 0, getBoundingClientRect: () => ({ width: 120, height: 60 }) };
    expect(pagxClipFullBoxD(el)).toBe('M 0 0 L 120 0 L 120 60 L 0 60 Z');
  });

  test('returns empty for a zero-size element', () => {
    global.getComputedStyle = () => ({ width: '0px', height: '0px' });
    const el = { offsetWidth: 0, offsetHeight: 0, getBoundingClientRect: () => ({ width: 0, height: 0 }) };
    expect(pagxClipFullBoxD(el)).toBe('');
  });
});

describe('pagxResolveClipNoneSamples', () => {
  test('rewrites none (\'\') samples to the full box path when every stop is a rect', () => {
    const el = { offsetWidth: 100, offsetHeight: 50 };
    const samples = [
      { 'clip-path': '' },
      { 'clip-path': 'path("M 0 0 L 10 0 L 10 20 L 0 20 Z")' },
    ];
    pagxResolveClipNoneSamples(el, samples);
    expect(samples[0]['clip-path']).toBe('path("M 0 0 L 100 0 L 100 50 L 0 50 Z")');
    expect(samples[1]['clip-path']).toBe('path("M 0 0 L 10 0 L 10 20 L 0 20 Z")');
  });

  test('leaves the column untouched when a geometric stop is not a 4-point rect', () => {
    const el = { offsetWidth: 100, offsetHeight: 50 };
    const samples = [
      { 'clip-path': '' },
      { 'clip-path': 'path("M 0 0 L 10 0 L 10 20 Z")' },
    ];
    pagxResolveClipNoneSamples(el, samples);
    expect(samples[0]['clip-path']).toBe('');
  });

  test('is a no-op when there is no none sample to rewrite', () => {
    const el = { offsetWidth: 100, offsetHeight: 50 };
    const samples = [{ 'clip-path': 'path("M 0 0 L 10 0 L 10 20 L 0 20 Z")' }];
    pagxResolveClipNoneSamples(el, samples);
    expect(samples[0]['clip-path']).toBe('path("M 0 0 L 10 0 L 10 20 L 0 20 Z")');
  });
});

describe('pagxElementUsesStepTiming', () => {
  const originalGetComputedStyle = global.getComputedStyle;
  afterEach(() => { global.getComputedStyle = originalGetComputedStyle; });

  const withTiming = (animationName, animationTimingFunction) => {
    global.getComputedStyle = () => ({ animationName, animationTimingFunction });
    return {};
  };

  test('a pure steps() / step-start / step-end animation is step-timed', () => {
    expect(pagxElementUsesStepTiming(withTiming('glitch', 'steps(4)'))).toBe(true);
    expect(pagxElementUsesStepTiming(withTiming('glitch', 'step-start'))).toBe(true);
    expect(pagxElementUsesStepTiming(withTiming('glitch', 'step-end'))).toBe(true);
  });

  test('mixed easing (one linear track) is not step-timed', () => {
    expect(pagxElementUsesStepTiming(withTiming('a, b', 'steps(4), linear'))).toBe(false);
  });

  test('an element with animation-name none is not step-timed', () => {
    expect(pagxElementUsesStepTiming(withTiming('none', 'steps(4)'))).toBe(false);
  });

  test('a throwing getComputedStyle degrades to false', () => {
    global.getComputedStyle = () => { throw new Error('detached'); };
    expect(pagxElementUsesStepTiming({})).toBe(false);
  });
});

describe('pagxClipNormalizeD (shape branches)', () => {
  // Default all border/padding/margin to 0px so the reference box is the plain border box.
  const origGetComputedStyle = global.getComputedStyle;
  beforeAll(() => {
    global.getComputedStyle = () => ({
      borderLeftWidth: '0px', borderTopWidth: '0px', borderRightWidth: '0px', borderBottomWidth: '0px',
      paddingLeft: '0px', paddingTop: '0px', paddingRight: '0px', paddingBottom: '0px',
      marginLeft: '0px', marginTop: '0px', marginRight: '0px', marginBottom: '0px',
    });
  });
  afterAll(() => { global.getComputedStyle = origGetComputedStyle; });

  // A 100x100 layout box keeps the emitted coordinates easy to verify by hand.
  const el = { offsetWidth: 100, offsetHeight: 100 };

  test('circle() emits a centred kappa-bezier disc', () => {
    expect(pagxClipNormalizeD('circle(50px at 50px 50px)', el)).toBe(
      'M 100 50 C 100 77.614, 77.614 100, 50 100 C 22.386 100, 0 77.614, 0 50' +
      ' C 0 22.386, 22.386 0, 50 0 C 77.614 0, 100 22.386, 100 50 Z');
  });

  test('ellipse() emits distinct rx / ry kappa-beziers', () => {
    expect(pagxClipNormalizeD('ellipse(40px 20px at 50px 50px)', el)).toBe(
      'M 90 50 C 90 61.046, 72.091 70, 50 70 C 27.909 70, 10 61.046, 10 50' +
      ' C 10 38.954, 27.909 30, 50 30 C 72.091 30, 90 38.954, 90 50 Z');
  });

  test('polygon() emits an M/L vertex path resolving percentages against the box', () => {
    expect(pagxClipNormalizeD('polygon(0% 0%, 100% 0%, 50% 100%)', el))
      .toBe('M 0 0 L 100 0 L 50 100 Z');
  });

  test('inset() with a round border-radius drops the rounding and keeps the rect', () => {
    expect(pagxClipNormalizeD('inset(10px 20px 30px 40px round 5px)', el))
      .toBe('M 40 10 L 80 10 L 80 70 L 40 70 Z');
  });
});
