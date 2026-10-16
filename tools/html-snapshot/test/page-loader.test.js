'use strict';

jest.mock('../dist/lib/browser-engine', () => ({
  unwrap: jest.fn(),
  newPage: jest.fn(),
  emulateReducedMotion: jest.fn(),
  mapWaitUntil: jest.fn(),
  addCookies: jest.fn(),
  addInitScript: jest.fn(),
  waitForNetworkIdle: jest.fn(),
}));

const vm = require('vm');
const browserEngine = require('../dist/lib/browser-engine');
const { openAndSettlePage } = require('../dist/lib/page-loader');

describe('openAndSettlePage — reduced motion', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    browserEngine.unwrap.mockReturnValue({ browser: {}, engine: 'puppeteer' });
    browserEngine.mapWaitUntil.mockReturnValue('load');
    browserEngine.waitForNetworkIdle.mockResolvedValue(undefined);
    browserEngine.addInitScript.mockResolvedValue(undefined);
  });

  test('lazy scrolling preserves authored opacity and filters', async () => {
    const hidden = ['0s', '1s'].map((transitionDuration) => {
      const values = { opacity: '0', filter: 'blur(2px)' };
      return {
        values,
        style: { setProperty: (name, value) => { values[name] = value; } },
        computed: {
          display: 'block', visibility: 'visible', opacity: '0',
          transitionProperty: 'all', transitionDuration, filter: 'blur(2px)',
        },
        textContent: 'Hidden content',
        getBoundingClientRect: () => ({ width: 200, height: 30 }),
      };
    });
    const root = {
      style: { setProperty() {} }, scrollHeight: 100,
      querySelectorAll: () => hidden,
    };
    const context = vm.createContext({
      window: {
        innerHeight: 900, scrollY: 0, scrollTo() {}, dispatchEvent() {},
        __pagxRealSetTimeout: (resolve) => resolve(),
      },
      document: {
        body: root, documentElement: root, scrollingElement: root,
        images: [], getAnimations: () => [],
      },
      getComputedStyle: (el) => el.computed,
      Event: function Event() {},
    });
    const page = {
      goto: jest.fn().mockResolvedValue(undefined),
      on: jest.fn(),
      waitForFunction: jest.fn().mockResolvedValue(undefined),
      evaluate: jest.fn((expression) => vm.runInContext(expression, context)),
    };
    browserEngine.newPage.mockResolvedValue(page);

    await openAndSettlePage(
      { browser: {}, engine: 'puppeteer' },
      'https://example.com/page',
      { waitMs: 0, autoScroll: true },
    );

    expect(page.evaluate).toHaveBeenCalled();
    await Promise.all(page.evaluate.mock.results.map(({ value }) => value));
    expect(hidden.map((el) => el.values)).toEqual([
      { opacity: '0', filter: 'blur(2px)' },
      { opacity: '0', filter: 'blur(2px)' },
    ]);
  });

  test('applies the default reduced-motion preference before navigation', async () => {
    const order = [];
    const page = {
      goto: jest.fn(async () => { order.push('goto'); }),
      on: jest.fn(),
      waitForFunction: jest.fn().mockResolvedValue(undefined),
    };
    browserEngine.newPage.mockResolvedValue(page);
    browserEngine.emulateReducedMotion.mockImplementation(async (_page, _engine, reduce) => {
      order.push(`reduced-motion:${reduce}`);
    });

    const result = await openAndSettlePage(
      { browser: {}, engine: 'puppeteer' },
      'https://example.com/page',
      { waitMs: 0, autoScroll: false },
    );

    expect(result).toBe(page);
    expect(browserEngine.emulateReducedMotion)
      .toHaveBeenCalledWith(page, 'puppeteer', true);
    expect(order).toEqual(['reduced-motion:true', 'goto']);
  });

  test('pins the root scroller to zero before and after lazy-content walking', async () => {
    const expressions = [];
    const page = {
      goto: jest.fn().mockResolvedValue(undefined),
      on: jest.fn(),
      waitForFunction: jest.fn().mockResolvedValue(undefined),
      evaluate: jest.fn(async (expression) => {
        expressions.push(expression);
        // The sweep returns the stable document height. Three identical rounds
        // satisfy settleLazyContent's two-stable-round exit condition.
        if (typeof expression === 'string' && expression.includes('return docHeight();')) {
          return 1200;
        }
        return undefined;
      }),
    };
    browserEngine.newPage.mockResolvedValue(page);

    await openAndSettlePage(
      { browser: {}, engine: 'puppeteer' },
      'https://example.com/page',
      { waitMs: 0, autoScroll: true },
    );

    const resetExpressions = expressions.filter(
      (expression) => typeof expression === 'string' &&
        expression.includes('function pagxResetRootScroll'),
    );
    expect(resetExpressions.length).toBeGreaterThanOrEqual(2);
    for (const expression of resetExpressions) {
      expect(expression).toContain('scrollLeft = 0');
      expect(expression).toContain('scrollTop = 0');
    }
  });
});
