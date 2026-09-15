'use strict';

const { MAX_CAPTURE_HEIGHT_PX } = require('../dist/lib/common');
const { applyCanvasViewport } = require('../dist/lib/canvas-viewport');

describe('applyCanvasViewport', () => {
  let timeoutSpy;

  beforeEach(() => {
    // The production delay lets browser reflow settle. These unit tests use a
    // deterministic evaluate() result, so execute the callback immediately.
    timeoutSpy = jest.spyOn(global, 'setTimeout').mockImplementation((callback) => {
      callback();
      return 0;
    });
  });

  afterEach(() => {
    timeoutSpy.mockRestore();
  });

  function puppeteerPage(before, after) {
    return {
      evaluate: jest.fn()
        .mockResolvedValueOnce(before)
        .mockResolvedValueOnce(after),
      setViewport: jest.fn().mockResolvedValue(undefined),
    };
  }

  test('keeps the canvas viewport when visible content and layout stay stable', async () => {
    const page = puppeteerPage(
      { score: 100, height: 900 },
      { score: 85, height: 1022 },
    );
    const log = jest.fn();

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1080, height: 1000 },
      { width: 1400, height: 900 },
      log,
    )).resolves.toEqual({ reverted: false });

    expect(page.evaluate).toHaveBeenCalledTimes(2);
    expect(page.setViewport).toHaveBeenCalledTimes(1);
    expect(page.setViewport).toHaveBeenCalledWith({
      width: 1080,
      height: 1000,
      deviceScaleFactor: 1,
    });
    expect(log).not.toHaveBeenCalled();
  });

  test('reverts and logs when the resize hides more than 15% of visible boxes', async () => {
    const page = puppeteerPage(
      { score: 100, height: 900 },
      { score: 84, height: 1000 },
    );
    const log = jest.fn();

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1080, height: 1000 },
      { width: 1400, height: 900 },
      log,
    )).resolves.toEqual({ reverted: true });

    expect(page.setViewport).toHaveBeenNthCalledWith(1, {
      width: 1080,
      height: 1000,
      deviceScaleFactor: 1,
    });
    expect(page.setViewport).toHaveBeenNthCalledWith(2, {
      width: 1400,
      height: 900,
      deviceScaleFactor: 1,
    });
    expect(log).toHaveBeenCalledWith(expect.stringContaining(
      'hid content (visible boxes 100→84)',
    ));
    expect(log).toHaveBeenCalledWith(expect.stringContaining(
      'capturing at 1400x900',
    ));
  });

  test('reverts a ballooned layout through the Playwright viewport API', async () => {
    const page = {
      evaluate: jest.fn()
        .mockResolvedValueOnce({ score: 0, height: 900 })
        .mockResolvedValueOnce({ score: 0, height: 1300 }),
      setViewportSize: jest.fn().mockResolvedValue(undefined),
    };
    const log = jest.fn();

    await expect(applyCanvasViewport(
      page,
      'playwright',
      { width: 1080, height: 1000 },
      { width: 1400, height: 900 },
      log,
    )).resolves.toEqual({ reverted: true });

    expect(page.setViewportSize).toHaveBeenNthCalledWith(1, {
      width: 1080,
      height: 1000,
    });
    expect(page.setViewportSize).toHaveBeenNthCalledWith(2, {
      width: 1400,
      height: 900,
    });
    expect(log).toHaveBeenCalledWith(expect.stringContaining(
      'ballooned layout (height 1000→1300px)',
    ));
  });

  test('keeps a single-screen fit-to-window stage that reflows within the viewport increase', async () => {
    // A fixed stage (canvas ≈ one viewport) rescaled by a `fit()` resize handler
    // grows the body by up to the viewport-height increase (990 → up to ~1082
    // for a 90px viewport grow). That is a benign fit reflow, not a balloon, so
    // the resize is kept.
    const page = puppeteerPage(
      { score: 50, height: 990 },
      { score: 50, height: 1080 },
    );

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1660, height: 990 },
      { width: 1400, height: 900 },
    )).resolves.toEqual({ reverted: false });

    expect(page.setViewport).toHaveBeenCalledTimes(1);
  });

  test('reverts a long page whose viewport-height hero balloons the canvas', async () => {
    // Regression: a long scrolling page (canvas many viewports tall) with a
    // single `100vh` hero. Resizing to the tall canvas inflates the hero and
    // grows the body (6695 → 9417) — LESS than the huge `viewportGrew` (5795),
    // so the fit-to-window relaxation would wrongly keep it and leave a big empty
    // gap. Because the canvas is multi-screen the strict bound applies and it
    // reverts to the settle viewport instead. (codebuddy.cn)
    const page = puppeteerPage(
      { score: 100, height: 6695 },
      { score: 100, height: 9417 },
    );
    const log = jest.fn();

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1400, height: 6695 },
      { width: 1400, height: 900 },
      log,
    )).resolves.toEqual({ reverted: true });

    expect(page.setViewport).toHaveBeenNthCalledWith(2, {
      width: 1400,
      height: 900,
      deviceScaleFactor: 1,
    });
    expect(log).toHaveBeenCalledWith(expect.stringContaining(
      'ballooned layout (height 6695→9417px)',
    ));
  });

  test('caps the measured height before checking for layout ballooning', async () => {
    const page = puppeteerPage(
      { score: 1, height: MAX_CAPTURE_HEIGHT_PX },
      { score: 1, height: MAX_CAPTURE_HEIGHT_PX * 2 },
    );

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1080, height: MAX_CAPTURE_HEIGHT_PX },
      { width: 1400, height: 900 },
    )).resolves.toEqual({ reverted: false });

    expect(page.setViewport).toHaveBeenCalledTimes(1);
  });

  test('reverts safely when no logger is provided', async () => {
    const page = puppeteerPage(
      { score: 10, height: 900 },
      { score: 0, height: 1000 },
    );

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1080, height: 1000 },
      { width: 1400, height: 900 },
    )).resolves.toEqual({ reverted: true });

    expect(page.setViewport).toHaveBeenCalledTimes(2);
  });

  test('treats missing or malformed fingerprint fields as zero/default values', async () => {
    const page = puppeteerPage(null, { score: 'unknown', height: 'unknown' });

    await expect(applyCanvasViewport(
      page,
      'puppeteer',
      { width: 1080, height: 1000 },
      { width: 1400, height: 900 },
      null,
    )).resolves.toEqual({ reverted: false });

    expect(page.setViewport).toHaveBeenCalledTimes(1);
  });
});
