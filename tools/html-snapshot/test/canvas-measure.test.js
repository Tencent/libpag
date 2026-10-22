'use strict';

const { measureBodyCanvas } = require('../dist/lib/canvas-measure');

describe('body canvas boundaries', () => {
  const previous = global.document;
  const previousStyle = global.getComputedStyle;
  afterEach(() => { global.document = previous; global.getComputedStyle = previousStyle; });

  function bodyCanvas(rect, scroll = { x: 0, y: 0 }, position = '') {
    let currentPosition = position;
    const body = {
      style: {
        getPropertyValue: () => currentPosition,
        getPropertyPriority: () => '',
        setProperty: (_key, value) => { currentPosition = value; },
      },
      scrollWidth: Math.round(rect.width), scrollHeight: Math.round(rect.height),
      getBoundingClientRect: () => rect,
      querySelectorAll: () => [],
    };
    global.document = { body, scrollingElement: { scrollLeft: scroll.x, scrollTop: scroll.y } };
    return body;
  }

  test('encloses fractional right and bottom edges without viewport inflation', () => {
    bodyCanvas({ left: 0, top: 0, width: 375.125, height: 812.125 });
    expect(measureBodyCanvas()).toEqual({ width: 376, height: 813 });
  });

  test('includes leading collapsed space in document coordinates and tolerates residual scroll', () => {
    bodyCanvas({ left: -7, top: -80, width: 200.25, height: 100.25 }, { x: 7, y: 100 });
    expect(measureBodyCanvas()).toEqual({ width: 201, height: 121 });
  });

  test('measures authored position and restores the temporary snapshot containing block', () => {
    const body = bodyCanvas({ left: 0, top: 0, width: 200, height: 100 }, undefined, 'relative');
    body.__pagxAuthoredPosition = { value: 'absolute', priority: 'important' };
    body.getBoundingClientRect = () => {
      expect(body.style.getPropertyValue('position')).toBe('absolute');
      return { left: 0, top: 0, width: 200, height: 100 };
    };
    expect(measureBodyCanvas()).toEqual({ width: 200, height: 100 });
    expect(body.style.getPropertyValue('position')).toBe('relative');
  });

  test('captures out-of-flow content when body has collapsed, and applies the height cap', () => {
    bodyCanvas({ left: 0, top: 0, width: 0, height: 0 });
    expect(measureBodyCanvas(500, () => ({ right: 672.25, bottom: 700.25 })))
      .toEqual({ width: 673, height: 500 });
  });

  test('returns an empty canvas before a body is available', () => {
    global.document = {};
    expect(measureBodyCanvas()).toEqual({ width: 0, height: 0 });
  });

  test('uses only visible non-empty descendants for a collapsed body', () => {
    const body = bodyCanvas({ left: 0, top: 0, width: 0, height: 0 });
    delete document.scrollingElement;
    document.documentElement = { scrollLeft: 3, scrollTop: 7 };
    const elements = [
      { display: 'none' }, { visibility: 'hidden' }, { opacity: '0' },
      { width: 0 }, { height: 0 }, { right: 10.25, bottom: 30.25 },
      { right: 20.25, bottom: 10.25 },
    ].map(props => ({
      ...props,
      getBoundingClientRect: () => ({ width: 5, height: 5, right: 999, bottom: 999, ...props }),
    }));
    body.querySelectorAll = () => elements;
    global.getComputedStyle = el => ({ display: 'block', visibility: 'visible', opacity: '1', ...el });
    expect(measureBodyCanvas()).toEqual({ width: 24, height: 38 });
  });

  test('does not inflate a noncollapsed axis and tolerates a missing scrolling root', () => {
    bodyCanvas({ left: 0, top: 0, width: 100, height: 0 });
    delete document.scrollingElement;
    expect(measureBodyCanvas(Infinity, () => ({ right: 1000, bottom: 20 })))
      .toEqual({ width: 100, height: 20 });
  });

  test('restores the authored position override even when measurement throws', () => {
    const body = bodyCanvas({ left: 0, top: 0, width: 0, height: 0 }, undefined, 'relative');
    body.__pagxAuthoredPosition = { value: 'absolute', priority: 'important' };
    body.getBoundingClientRect = () => { throw new Error('detached'); };
    expect(() => measureBodyCanvas()).toThrow('detached');
    expect(body.style.getPropertyValue('position')).toBe('relative');
  });
});
