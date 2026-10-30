/** @jest-environment jsdom */
'use strict';

const {
  pagxSampleTimeline, pagxBuildTextOverlays, pagxBuildUseOverlays,
  pagxAnimMain, pagxSeekAllToTime, pagxMeasureGlobalDurationMs,
  pagxCaptureAnimeInstances, settleAndInlineCanvasesOnPage,
} = require('../dist/lib/animation-capture');
const { computedStyle, setRect } = require('./dom-helpers');

let readStyle;
beforeEach(() => {
  document.head.innerHTML = '';
  document.body.innerHTML = '';
  readStyle = () => ({});
  jest.spyOn(window, 'getComputedStyle').mockImplementation(el => computedStyle(el, {
    color: 'rgb(0, 0, 0)', backgroundColor: 'rgba(0, 0, 0, 0)', fontSize: '16px',
    filter: 'none', clipPath: 'none', boxShadow: 'none',
    animationName: 'none', animationTimingFunction: 'linear', ...readStyle(el),
  }));
});
afterEach(() => {
  jest.restoreAllMocks();
  for (const key of ['gsap', 'anime', '__pagxBaselineAnime', '__pagxClock']) delete window[key];
  delete document.getAnimations;
});

function sample(elements, seek, count = 5) {
  const captured = [], text = [], use = [], seen = new Set();
  pagxSampleTimeline(captured, seen, 1000, seek, 1, count, 100, 'normal', 0, text, use, elements);
  return { captured, text, use, seen };
}

describe('dynamic text and SVG sampling', () => {
  test('uses the most visible text style and keeps disjoint opacity windows for repeated values', () => {
    document.body.innerHTML = '<span style="position:absolute" id="counter"></span>';
    const el = document.querySelector('span');
    let index = 0;
    const values = ['+1', '+1', '', '+2', '+1'];
    const opacity = [0.2, 0.8, 0, 1, 0.5];
    readStyle = () => ({ opacity: String(opacity[index]), fontSize: `${10 + index}px`, fill: 'red' });
    const result = sample([el], p => { index = Math.round(p * 4); el.textContent = values[index]; });
    expect(result.text).toHaveLength(1);
    const segments = result.text[0].segments;
    expect(segments.map(s => [s.text, s.fontSize, s.fill])).toEqual([
      ['+1', '11px', 'red'], ['+2', '13px', 'red'], ['+1', '14px', 'red'],
    ]);
    expect(segments[0].stops).toEqual([
      { offset: 0, props: { opacity: '0.2' } }, { offset: 0.25, props: { opacity: '0.8' } },
      { offset: 0.5, props: { opacity: '0' } }, { offset: 1, props: { opacity: '0' } },
    ]);
    const captured = [];
    pagxBuildTextOverlays(result.text, captured, 1000);
    expect(el.textContent).toBe('');
    expect(document.querySelectorAll('#counter')).toHaveLength(1);
    expect(captured.map(cap => cap.el.textContent)).toEqual(['+1', '+2', '+1']);
    expect(captured[0].el.style.fill).toBe('red');
    expect(captured[0].keyframes).toBe(segments[0].stops);
  });

  test('leaves in-flow scrambled characters and SVG text out of text overlays', () => {
    document.body.innerHTML = '<div><span id="char">a</span><span>next</span></div><svg><text>a</text></svg>';
    const char = document.querySelector('#char');
    const svgText = document.querySelector('text');
    let progress = 0;
    readStyle = () => ({ opacity: String(progress) });
    const result = sample([char, svgText], p => {
      progress = p; char.textContent = svgText.textContent = p < 0.5 ? 'a' : 'b';
    });
    expect(result.text).toEqual([]);
    expect(result.captured.map(cap => cap.el)).toEqual([char, svgText]);
  });

  test.each(['use', 'image'])('captures SVG <%s> href swaps and updates legacy xlink references', tag => {
    document.body.innerHTML = `<svg><${tag} id="slot" style="opacity:0.5"/></svg>`;
    const el = document.querySelector(tag);
    const xlink = 'http://www.w3.org/1999/xlink';
    let index = 0;
    readStyle = () => ({ opacity: String([1, 0.5, 0, 0.75, 1][index]) });
    const result = sample([el], p => {
      index = Math.round(p * 4);
      el.setAttributeNS(xlink, 'xlink:href', ['#a', '#b', '', '#b', '#a'][index]);
    });
    expect(result.text).toEqual([]);
    expect(result.captured).toEqual([]);
    expect(result.seen.has(el)).toBe(true);
    expect(result.use[0].segments.map(s => s.href)).toEqual(['#a', '#b']);
    expect(result.use[0].segments[1].stops).toEqual([
      { offset: 0, props: { opacity: '0' } }, { offset: 0.25, props: { opacity: '0.5' } },
      { offset: 0.5, props: { opacity: '0' } }, { offset: 0.75, props: { opacity: '0.75' } },
      { offset: 1, props: { opacity: '0' } },
    ]);
    const captured = [];
    pagxBuildUseOverlays(result.use, captured, 1000);
    expect(captured.map(cap => cap.el.getAttribute('href'))).toEqual(['#a', '#b']);
    expect(captured.map(cap => cap.el.getAttributeNS(xlink, 'href'))).toEqual(['#a', '#b']);
    expect(captured.every(cap => cap.el.style.opacity === '')).toBe(true);
    expect(document.querySelectorAll('#slot')).toHaveLength(1);
  });

  test.each([
    ['#a', '#a', '#a'], ['', '#a', '#a'],
    Array.from({ length: 13 }, (_, i) => `#${i}`),
  ].map(hrefs => [hrefs]))('does not fan out static, single-reference or excessive glyph sequences: %j', hrefs => {
    document.body.innerHTML = '<svg><use/></svg>';
    const el = document.querySelector('use');
    const result = sample([el], p => el.setAttribute('href', hrefs[Math.round(p * (hrefs.length - 1))]), hrefs.length);
    expect(result.use).toEqual([]);
  });

  test('pins identity transforms and bisects hard steps instead of delaying them to the sample grid', () => {
    document.body.innerHTML = '<div></div>';
    const el = document.querySelector('div');
    let progress = 0;
    readStyle = () => ({
      animationName: 'jump', animationTimingFunction: 'steps(2, end)',
      transform: progress >= 0.3 ? 'translate(20px, 0px)' : 'none',
    });
    const result = sample([el], p => { progress = p; }, 3);
    expect(result.captured).toHaveLength(1);
    const cap = result.captured[0];
    expect(cap.timing).toBe('step-end');
    expect(cap.keyframes[0]).toEqual({ offset: 0, props: { transform: 'translate(0px, 0px)' } });
    expect(cap.keyframes[1].props.transform).toBe('translate(20px, 0px)');
    expect(cap.keyframes[1].offset).toBeGreaterThanOrEqual(0.3);
    expect(cap.keyframes[1].offset).toBeLessThan(0.302);
  });

  test('guards one-sample requests and limits explicit candidates', () => {
    document.body.innerHTML = '<div></div><span></span>';
    const elements = Array.from(document.body.children);
    let progress = 0;
    readStyle = () => ({ opacity: String(progress) });
    const captured = [];
    pagxSampleTimeline(captured, new Set(), 1000, p => { progress = p; }, 1, 1, 1, '', 0, [], [], elements);
    expect(captured).toHaveLength(1);
    expect(captured[0]).toMatchObject({ el: elements[0], direction: 'normal', keyframes: [
      { offset: 0, props: { opacity: '0' } }, { offset: 1, props: { opacity: '1' } },
    ] });
  });

  test('orchestrator restores the initial DOM before emitting text and SVG overlay animations', () => {
    document.body.innerHTML = '<div id="counter" style="position:absolute">1</div><svg><use id="slot" href="#a"/></svg>';
    const counter = document.querySelector('#counter');
    const slot = document.querySelector('#slot');
    document.querySelectorAll('*').forEach(el => setRect(el));
    const animation = { effect: { getComputedTiming: () => ({ duration: 80, delay: 0 }) }, pause() {} };
    document.getAnimations = () => [animation];
    window.__pagxClock = { advanceTo: jest.fn(time => {
      counter.textContent = time < 40 ? '1' : '2';
      counter.className = time < 40 ? 'initial' : 'changed';
      slot.setAttribute('href', time < 40 ? '#a' : '#b');
    }) };
    const result = pagxAnimMain({ sampleCount: 3, maxElements: 100, prefix: 'testAnim', styleId: 'captured' });
    expect(result).toEqual({ count: 4, names: ['testAnim0', 'testAnim1', 'testAnim2', 'testAnim3'] });
    expect(counter.className).toBe('initial');
    expect(counter.textContent).toBe('');
    expect(document.querySelectorAll('#counter')).toHaveLength(1);
    expect(Array.from(document.querySelectorAll('use'), el => el.getAttribute('href'))).toEqual(['#a', '#b']);
    expect(document.querySelector('#captured').textContent).toContain('@keyframes testAnim3');
    expect(window.__pagxClock.advanceTo).toHaveBeenLastCalledWith(0);
    expect(document.head.querySelectorAll('style')).toHaveLength(1);
  });
});

describe('library clocks', () => {
  test.each([
    [undefined, 'normal', 250, 100], [3, 'normal', 150, 50],
    [3, 'alternate', 125, 75], [3, 'reverse', 125, 75],
    [2, 'alternate', 500, 0], [true, 'normal', 250, 50],
    [true, 'alternate', 325, 75], [true, 'normal', 100, 0],
    [true, 'reverse', 250, 50], [true, 'alternate', 200, 0],
  ])('seeks loop=%s direction=%s at %sms to %sms', (loop, direction, time, expected) => {
    const instance = { duration: 100, loop, direction, pause: jest.fn(), seek: jest.fn() };
    window.anime = { running: [instance] };
    pagxCaptureAnimeInstances();
    window.anime.running = [];
    pagxSeekAllToTime(time);
    expect(instance.pause).toHaveBeenCalledTimes(1);
    expect(instance.seek).toHaveBeenCalledWith(expected);
  });

  test('advances the virtual clock before library seeks and tolerates one broken instance', () => {
    const calls = [];
    window.__pagxClock = { advanceTo: time => calls.push(['clock', time]) };
    window.gsap = { globalTimeline: { pause: () => {}, time: (...args) => calls.push(['gsap', ...args]) } };
    const good = { seek: time => calls.push(['anime', time]) };
    window.anime = { running: [
      { duration: Infinity, seek: jest.fn() }, { duration: 0, seek: jest.fn() },
      { duration: 100, pause: () => { throw new Error('detached'); } }, good,
    ] };
    pagxSeekAllToTime(300);
    expect(calls).toEqual([['clock', 300], ['gsap', 0.3, true], ['anime', 300]]);
  });

  test('measures the maximum valid WAAPI, GSAP and anime duration despite inaccessible entries', () => {
    document.getAnimations = () => [
      { effect: { getComputedTiming: () => { throw new Error('detached'); } } },
      { effect: { getComputedTiming: () => ({ duration: Infinity }) } },
      { effect: { getComputedTiming: () => ({ duration: 100, delay: 20 }) } },
    ];
    window.gsap = { globalTimeline: { getChildren: () => [
      { duration: () => 0.5, startTime: () => 0.2 },
      { duration: () => Infinity }, { duration: () => 0.1, startTime: () => NaN },
      { duration: () => { throw new Error('bad duration'); } },
    ] } };
    window.anime = { running: [null, { duration: 'invalid' }, { duration: 900 }] };
    expect(pagxMeasureGlobalDurationMs()).toBe(900);
    window.anime.running = [];
    expect(pagxMeasureGlobalDurationMs()).toBe(700);
  });

  test('a missing animation API or broken library does not prevent seeking the remaining animations', () => {
    document.getAnimations = () => { throw new Error('unsupported'); };
    window.gsap = { globalTimeline: { getChildren: () => { throw new Error('disposed'); } } };
    Object.defineProperty(window, 'anime', {
      configurable: true, get: () => { throw new Error('unavailable library'); },
    });
    expect(pagxMeasureGlobalDurationMs()).toBe(0);
    expect(() => pagxSeekAllToTime(150)).not.toThrow();
    const animation = {
      pause: jest.fn(), get __pagxBirth() { throw new Error('missing birth'); }, currentTime: 0,
    };
    document.getAnimations = () => [animation];
    pagxSeekAllToTime(150);
    expect(animation.currentTime).toBe(150);
  });
});

describe('overlay failure recovery', () => {
  test.each([pagxBuildTextOverlays, pagxBuildUseOverlays])('%p skips failed clones without losing other overlays', build => {
    document.body.innerHTML = '<div id="bad"></div><div id="good"></div>';
    const bad = document.querySelector('#bad'), good = document.querySelector('#good');
    jest.spyOn(bad, 'cloneNode').mockImplementation(() => { throw new Error('detached clone'); });
    const stops = [{ offset: 0, props: { opacity: '0' } }, { offset: 1, props: { opacity: '1' } }];
    const segments = [{ href: '#a', text: 'a', stops }, { href: '#b', text: 'b', stops }];
    const captured = [];
    build([{ el: bad, segments }, { el: good, segments }], captured, 1000);
    const successful = captured.filter(cap => cap.el !== bad);
    expect(successful).toHaveLength(2);
    expect(successful.every(cap => cap.el.isConnected)).toBe(true);
    expect(document.querySelectorAll('#good')).toHaveLength(1);
  });

  test.each([pagxBuildTextOverlays, pagxBuildUseOverlays])('%p recovers when a parent refuses overlay insertion', build => {
    document.body.innerHTML = '<div><span id="bad"></span></div><section><span id="good"></span></section>';
    const bad = document.querySelector('#bad'), good = document.querySelector('#good');
    jest.spyOn(bad.parentNode, 'insertBefore').mockImplementation(() => { throw new Error('parent removed'); });
    const segments = [{ text: 'a', href: '#a', stops: [] }, { text: 'b', href: '#b', stops: [] }];
    const captured = [];
    build([{ el: bad, segments }, { el: good, segments }], captured, 1000);
    expect(captured.filter(cap => cap.el !== bad)).toHaveLength(2);
  });
});

describe('canvas settle wrapper', () => {
  test('forwards the budget and reports successful capture', async () => {
    const result = { count: 2 };
    const page = { evaluate: jest.fn().mockResolvedValue(result) };
    const logger = jest.fn();
    expect(await settleAndInlineCanvasesOnPage(page, { settleBudgetMs: 250, logger })).toBe(result);
    expect(page.evaluate.mock.calls[0][0]).toContain('pagxSettleAndInlineCanvases(250)');
    expect(logger).toHaveBeenCalledWith('settled + inlined 2 canvas bitmap(s)');
  });

  test('falls back to a static capture for null results and evaluation failures', async () => {
    const page = { evaluate: jest.fn().mockResolvedValue(null) };
    expect(await settleAndInlineCanvasesOnPage(page)).toEqual({ count: 0 });
    expect(page.evaluate.mock.calls[0][0]).toContain('pagxSettleAndInlineCanvases(4000)');
    const logger = jest.fn();
    page.evaluate.mockRejectedValue(new Error('page closed'));
    expect(await settleAndInlineCanvasesOnPage(page, { logger })).toEqual({ count: 0 });
    expect(logger).toHaveBeenCalledWith('canvas settle failed: page closed');
    expect(await settleAndInlineCanvasesOnPage(page)).toEqual({ count: 0 });
  });
});
