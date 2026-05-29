'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { PNG } = require('pngjs');
const { pixelMetrics, countImporterWarnings } = require('../eval/compare');

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
