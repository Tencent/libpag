'use strict';

const { pagxClipNormalizeD } = require('../dist/lib/animation-capture');
const original = global.getComputedStyle;
let css;
beforeEach(() => {
  css = {};
  global.getComputedStyle = () => css;
});
afterEach(() => { global.getComputedStyle = original; });
const el = { offsetWidth: 200, offsetHeight: 100 };

test.each([
  ['padding-box', 'M 2 3 L 196 3 L 196 95 L 2 95 Z'],
  ['content-box', 'M 8 10 L 188 10 L 188 86 L 8 86 Z'],
  ['margin-box', 'M -10 -20 L 230 -20 L 230 140 L -10 140 Z'],
  ['border-box', 'M 0 0 L 200 0 L 200 100 L 0 100 Z'],
  ['fill-box', 'M 0 0 L 200 0 L 200 100 L 0 100 Z'],
])('resolves %s in local border-box coordinates', (box, expected) => {
  css = {
    borderLeftWidth: '2px', borderTopWidth: '3px', borderRightWidth: '4px', borderBottomWidth: '5px',
    paddingLeft: '6px', paddingTop: '7px', paddingRight: '8px', paddingBottom: '9px',
    marginLeft: '10px', marginTop: '20px', marginRight: '30px', marginBottom: '40px',
  };
  expect(pagxClipNormalizeD(`inset(0) ${box}`, el)).toBe(expected);
});

test('falls back to rendered bounds for SVG without a used CSS size and rejects empty geometry', () => {
  const svg = { getBoundingClientRect: () => ({ width: 90, height: 45 }) };
  expect(pagxClipNormalizeD('inset(0)', svg)).toBe('M 0 0 L 90 0 L 90 45 L 0 45 Z');
  expect(pagxClipNormalizeD('inset(0)', { getBoundingClientRect: () => ({ width: 0, height: 0 }) })).toBe('');
  css = { paddingLeft: '300px' };
  expect(pagxClipNormalizeD('inset(0) content-box', el)).toBe('');
});

test('prefers SVG used CSS dimensions over transformed screen bounds', () => {
  css = { width: '120px', height: '60px' };
  const svg = { getBoundingClientRect: jest.fn(() => ({ width: 240, height: 120 })) };
  expect(pagxClipNormalizeD('inset(0)', svg)).toBe('M 0 0 L 120 0 L 120 60 L 0 60 Z');
  expect(svg.getBoundingClientRect).not.toHaveBeenCalled();
});

test.each([
  ['inset(calc(20% + 2px * 3) calc(50% / 2 - 5px))', 'M 45 26 L 155 26 L 155 74 L 45 74 Z'],
  ['polygon(evenodd, 0 0, calc(100% - 10px) 0, 50% 100%)', 'M 0 0 L 190 0 L 100 100 Z'],
  ['inset(80px)', 'M 80 80 L 120 80 L 120 80 L 80 80 Z'],
  ['inset()', 'M 0 0 L 200 0 L 200 100 L 0 100 Z'],
  ['path(evenodd, "M 1 2 L 3 4 Z")', 'M 1 2 L 3 4 Z'],
  ["path('M 0 0 Z')", 'M 0 0 Z'],
  ['path(unquoted)', ''], ['path("")', ''], ['polygon(0 0, 10px 10px)', ''],
  ['inset(20px', ''], ['unknown(10px)', ''], ['', ''], ['none', ''],
])('normalizes shape %s', (raw, expected) => {
  expect(pagxClipNormalizeD(raw, el)).toBe(expected);
});

// Endpoints of the four cubic arcs determine the expected circle/ellipse
// extents independently of its control-point approximation.
function arcEndpoints(path) {
  const numbers = path.match(/-?\d+(?:\.\d+)?/g).map(Number);
  return [numbers.slice(0, 2), ...Array.from({ length: 4 }, (_, i) => numbers.slice(6 + i * 6, 8 + i * 6))];
}
test.each([
  ['circle()', [150, 50, 100, 100, 50, 50, 100, 0, 150, 50]],
  ['circle(closest-side at 20px 30px)', [40, 30, 20, 50, 0, 30, 20, 10, 40, 30]],
  ['circle(farthest-side at left top)', [200, 0, 0, 200, -200, 0, 0, -200, 200, 0]],
  ['circle(closest-corner at 30px 40px)', [80, 40, 30, 90, -20, 40, 30, -10, 80, 40]],
  ['circle(farthest-corner at 170px 60px)', [350.278, 60, 170, 240.278, -10.278, 60, 170, -120.278, 350.278, 60]],
  ['ellipse()', [200, 50, 100, 100, 0, 50, 100, 0, 200, 50]],
  ['ellipse(farthest-side closest-side at 25% 25%)', [200, 25, 50, 50, -100, 25, 50, 0, 200, 25]],
  ['ellipse(closest-side farthest-side at 25% 25%)', [100, 25, 50, 100, 0, 25, 50, -50, 100, 25]],
  ['ellipse(10px 20px)', [110, 50, 100, 70, 90, 50, 100, 30, 110, 50]],
])('preserves arc endpoints for %s', (shape, expected) => {
  expect(arcEndpoints(pagxClipNormalizeD(shape, el)).flat()).toEqual(expected);
});
