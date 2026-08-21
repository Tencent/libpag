'use strict';

const {
  readRootScrollOffset,
  resetRootScroll,
  canvasRootRect,
} = require('../dist/lib/browser-snapshot');

describe('root-scroll geometry normalisation', () => {
  const previousWindow = global.window;
  const previousDocument = global.document;

  afterEach(() => {
    if (previousWindow === undefined) delete global.window;
    else global.window = previousWindow;
    if (previousDocument === undefined) delete global.document;
    else global.document = previousDocument;
  });

  function rootElement(left, top) {
    return {
      scrollLeft: left,
      scrollTop: top,
      style: { setProperty: jest.fn() },
    };
  }

  test('directly resets the scrolling element when window.scrollTo is a no-op', () => {
    const html = rootElement(31, 11547);
    const body = rootElement(31, 11547);
    global.document = {
      documentElement: html,
      body,
      scrollingElement: html,
    };
    global.window = {
      // Simulate an authored override. The DOM property assignment must still win.
      scrollTo: jest.fn(),
      scrollX: 31,
      scrollY: 11547,
      pageXOffset: 31,
      pageYOffset: 11547,
    };

    expect(resetRootScroll()).toEqual({ left: 0, top: 0 });
    expect(readRootScrollOffset()).toEqual({ left: 0, top: 0 });
    expect(html.scrollLeft).toBe(0);
    expect(html.scrollTop).toBe(0);
    expect(body.scrollLeft).toBe(0);
    expect(body.scrollTop).toBe(0);
    expect(global.window.scrollTo).toHaveBeenCalledWith(0, 0);
    expect(html.style.setProperty)
      .toHaveBeenCalledWith('scroll-behavior', 'auto', 'important');
  });

  test('uses residual root scroll as the canvas viewport origin', () => {
    const html = { style: { setProperty: jest.fn() } };
    Object.defineProperties(html, {
      scrollLeft: { configurable: true, get: () => 9, set: () => {} },
      scrollTop: { configurable: true, get: () => 11547, set: () => {} },
    });
    const body = rootElement(0, 0);
    global.document = {
      documentElement: html,
      body,
      scrollingElement: html,
    };
    global.window = {
      scrollTo: jest.fn(),
      scrollX: 9,
      scrollY: 11547,
      pageXOffset: 9,
      pageYOffset: 11547,
    };

    expect(canvasRootRect(1920, 12627)).toEqual({
      left: -9,
      top: -11547,
      right: 1911,
      bottom: 1080,
      width: 1920,
      height: 12627,
      x: -9,
      y: -11547,
    });
  });
});
