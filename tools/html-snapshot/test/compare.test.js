'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { PNG } = require('pngjs');
const { pixelMetrics, countImporterWarnings } = require('../eval/compare');

// `countFlexInRenderedHtml` drives a real browser through
// `openAndSettlePage`. We mock that collaborator so the flex-counting logic
// (including the in-page `getComputedStyle` walk) runs in node without
// Chromium: the stub page `evaluate` executes the supplied function against a
// fake `document` / `getComputedStyle`, and `close` is asserted to run.
describe('countFlexInRenderedHtml', () => {
  let saved;
  beforeEach(() => {
    saved = { document: global.document, getComputedStyle: global.getComputedStyle };
    jest.resetModules();
  });
  afterEach(() => {
    global.document = saved.document;
    global.getComputedStyle = saved.getComputedStyle;
    jest.dontMock('../dist/lib/page-loader');
    jest.resetModules();
  });

  // Stand up a fake DOM where each element's display is read via getComputedStyle.
  function installFakeDom(displays) {
    const elements = displays.map((d) => ({ __display: d }));
    global.document = { body: { querySelectorAll: () => elements } };
    global.getComputedStyle = (el) => ({ display: el.__display });
  }

  // Load compare.js with a mocked page-loader whose page.evaluate runs the
  // passed function in-process (so the in-page walk is actually executed and
  // counted). `closeSpy` records that the page was closed.
  function loadCompareWithPage({ closeSpy, openImpl } = {}) {
    jest.doMock('../dist/lib/page-loader', () => ({
      openAndSettlePage: openImpl || (async () => ({
        evaluate: async (fn) => fn(),
        close: async () => { if (closeSpy) closeSpy(); },
      })),
    }));
    return require('../eval/compare');
  }

  test('counts only flex / inline-flex descendants and closes the page', async () => {
    installFakeDom(['block', 'flex', 'inline-flex', 'grid', 'flex']);
    let closed = 0;
    const { countFlexInRenderedHtml } = loadCompareWithPage({ closeSpy: () => { closed++; } });
    const count = await countFlexInRenderedHtml({ browser: {}, engine: 'puppeteer' }, '/tmp/page.html');
    expect(count).toBe(3);
    expect(closed).toBe(1);
  });

  test('forwards viewport / wait options to openAndSettlePage', async () => {
    installFakeDom(['flex']);
    let seenUrl = null;
    let seenOpts = null;
    const { countFlexInRenderedHtml } = loadCompareWithPage({
      openImpl: async (browser, url, opts) => {
        seenUrl = url;
        seenOpts = opts;
        return { evaluate: async (fn) => fn(), close: async () => {} };
      },
    });
    await countFlexInRenderedHtml({}, '/abs/path.html', { viewportWidth: 320, viewportHeight: 480, waitMs: 25 });
    expect(seenUrl).toBe('file:///abs/path.html');
    expect(seenOpts).toEqual({ viewportWidth: 320, viewportHeight: 480, waitMs: 25 });
  });

  test('closes the page even when the in-page evaluate rejects', async () => {
    let closed = 0;
    jest.doMock('../dist/lib/page-loader', () => ({
      openAndSettlePage: async () => ({
        evaluate: async () => { throw new Error('evaluate boom'); },
        close: async () => { closed++; },
      }),
    }));
    const { countFlexInRenderedHtml } = require('../eval/compare');
    await expect(countFlexInRenderedHtml({}, '/tmp/x.html')).rejects.toThrow(/evaluate boom/);
    expect(closed).toBe(1);
  });
});

let dir;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), 'compare-test-'));
});
afterEach(() => {
  fs.rmSync(dir, { recursive: true, force: true });
});

// Write a solid-colour PNG of the given size to `name` inside the temp dir.
function writePng(name, width, height, [r, g, b]) {
  const png = new PNG({ width, height });
  for (let i = 0; i < width * height; i++) {
    const j = i * 4;
    png.data[j] = r;
    png.data[j + 1] = g;
    png.data[j + 2] = b;
    png.data[j + 3] = 255;
  }
  const file = path.join(dir, name);
  fs.writeFileSync(file, PNG.sync.write(png));
  return file;
}

describe('pixelMetrics', () => {
  test('identical images: zero diff, perfect structural match', () => {
    const a = writePng('a.png', 20, 20, [255, 255, 255]);
    const b = writePng('b.png', 20, 20, [255, 255, 255]);
    const m = pixelMetrics(a, b);
    expect(m.width).toBe(20);
    expect(m.height).toBe(20);
    expect(m.pixelDiffRatio).toBe(0);
    expect(m.meanRgbDelta).toBe(0);
    expect(m.ssim).toBeCloseTo(1, 5);
  });

  test('fully different solid colours: high diff ratio and RGB delta', () => {
    const a = writePng('a.png', 16, 16, [0, 0, 0]);
    const b = writePng('b.png', 16, 16, [255, 255, 255]);
    const m = pixelMetrics(a, b);
    expect(m.pixelDiffRatio).toBe(1);
    expect(m.meanRgbDelta).toBeCloseTo(255, 5);
  });

  test('pads mismatched sizes to the larger bounds', () => {
    const a = writePng('a.png', 10, 10, [255, 255, 255]);
    const b = writePng('b.png', 20, 30, [255, 255, 255]);
    const m = pixelMetrics(a, b);
    expect(m.width).toBe(20);
    expect(m.height).toBe(30);
  });

  test('writes a diff PNG when a path is provided', () => {
    const a = writePng('a.png', 8, 8, [0, 0, 0]);
    const b = writePng('b.png', 8, 8, [255, 255, 255]);
    const diffPath = path.join(dir, 'diff.png');
    pixelMetrics(a, b, diffPath);
    expect(fs.existsSync(diffPath)).toBe(true);
    const diff = PNG.sync.read(fs.readFileSync(diffPath));
    expect(diff.width).toBe(8);
    expect(diff.height).toBe(8);
  });
});

describe('countImporterWarnings', () => {
  test('returns zeros when the stderr file is missing', () => {
    expect(countImporterWarnings(path.join(dir, 'nope.txt')))
      .toEqual({ total: 0, flexInferred: 0, flexSkipped: 0 });
  });

  test('counts warnings and the flex-inference subcategories', () => {
    const file = path.join(dir, 'stderr.txt');
    fs.writeFileSync(file, [
      'warning: html: something odd',
      '[subset:flex-inferred] recovered a flex container',
      '[subset:flex-inference-skipped] gave up on one',
      '   ',
      'info: not a warning line',
    ].join('\n'));
    const out = countImporterWarnings(file);
    expect(out.total).toBe(3);
    expect(out.flexInferred).toBe(1);
    expect(out.flexSkipped).toBe(1);
  });

  test('ignores blank lines and non-warning content', () => {
    const file = path.join(dir, 'empty.txt');
    fs.writeFileSync(file, '\n\n   \nall good here\n');
    expect(countImporterWarnings(file)).toEqual({ total: 0, flexInferred: 0, flexSkipped: 0 });
  });
});
