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

  test('closes a newly created page when navigation fails without masking its cause', async () => {
    const error = new Error('net::ERR_NAME_NOT_RESOLVED');
    const page = { on: jest.fn(), goto: jest.fn().mockRejectedValue(error),
      close: jest.fn().mockRejectedValue(new Error('cleanup error')) };
    browserEngine.newPage.mockResolvedValue(page);
    await expect(openAndSettlePage({ browser: {}, engine: 'puppeteer' }, 'https://invalid.example'))
      .rejects.toBe(error);
    expect(page.close).toHaveBeenCalledTimes(1);
  });

  test('does not capture about:blank when a main document never commits', async () => {
    const error = new Error('Navigation timeout');
    const page = { on: jest.fn(), goto: jest.fn().mockRejectedValue(error), url: () => 'about:blank',
      evaluate: jest.fn().mockResolvedValue(true), close: jest.fn().mockResolvedValue(undefined) };
    browserEngine.newPage.mockResolvedValue(page);
    await expect(openAndSettlePage({ browser: {}, engine: 'puppeteer' }, 'https://pending.example'))
      .rejects.toBe(error);
    expect(page.evaluate).not.toHaveBeenCalled();
    expect(page.close).toHaveBeenCalledTimes(1);
  });
});
