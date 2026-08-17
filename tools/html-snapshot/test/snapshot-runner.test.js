'use strict';

jest.mock('../dist/lib/page-loader', () => ({
  openAndSettlePage: jest.fn(),
}));

const { TAKE_SNAPSHOT_EXPR, MEASURE_CANVAS_EXPR } = require('../dist/lib/browser-snapshot');
const { openAndSettlePage } = require('../dist/lib/page-loader');
const { runSnapshot } = require('../dist/lib/snapshot-runner');

describe('runSnapshot — reduced motion', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('forwards its default reducedMotion value to openAndSettlePage', async () => {
    const snapshot = { html: '<!DOCTYPE html><html></html>', width: 320, height: 180 };
    const page = {
      evaluate: jest.fn(async (expression) => {
        if (expression === MEASURE_CANVAS_EXPR) return null;
        if (expression === TAKE_SNAPSHOT_EXPR) return snapshot;
        return undefined;
      }),
      close: jest.fn().mockResolvedValue(undefined),
    };
    openAndSettlePage.mockResolvedValue(page);

    const result = await runSnapshot(
      { browser: {}, engine: 'puppeteer' },
      'https://example.com/page',
      { inlineIconFonts: false },
    );

    expect(openAndSettlePage).toHaveBeenCalledWith(
      expect.anything(),
      'https://example.com/page',
      expect.objectContaining({ reducedMotion: true }),
    );
    expect(result).toEqual({ ...snapshot, fonts: [], images: [] });
    expect(page.close).toHaveBeenCalledTimes(1);
  });

  test('animation capture defaults to no-preference motion', async () => {
    const snapshot = { html: '<!DOCTYPE html><html></html>', width: 320, height: 180 };
    const page = {
      evaluate: jest.fn(async (expression) => {
        if (expression === MEASURE_CANVAS_EXPR) return null;
        if (expression === TAKE_SNAPSHOT_EXPR) return snapshot;
        return undefined;
      }),
      close: jest.fn().mockResolvedValue(undefined),
    };
    openAndSettlePage.mockResolvedValue(page);

    await runSnapshot(
      { browser: {}, engine: 'puppeteer' },
      'https://example.com/page',
      { inlineIconFonts: false, captureAnimations: true },
    );

    expect(openAndSettlePage).toHaveBeenCalledWith(
      expect.anything(),
      'https://example.com/page',
      expect.objectContaining({ reducedMotion: false }),
    );
  });
});
