'use strict';

const {
  splitProjectiveTransform,
  liftBoxOntoCameraWrapper,
  rewritePerspectiveKeyframes,
  transformOriginDepth,
  perspectiveOriginXY,
  recordCameraSkip,
  pagxCameraSkips,
  staticCameraFor,
  withPerspectiveCamera,
} = require('../dist/lib/browser-snapshot');

// `perspective(600px) rotateY(-45deg)`: row 3 = (0, 0, -cos45/600, 1), row 1..2 carry ±sin45.
const ROTATE_Y_45_PROJECTED =
  'matrix3d(0.707107, 0, 0.707107, -0.00117851, 0, 1, 0, 0, -0.707107, 0, 0.707107, -0.00117851, 0, 0, 0, 1)';

describe('splitProjectiveTransform', () => {
  test('pulls the camera out of a projected matrix3d', () => {
    const split = splitProjectiveTransform(ROTATE_Y_45_PROJECTED);
    expect(split).not.toBeNull();
    // Recovered from a float matrix, so only the magnitude is asserted.
    expect(split.distance).toBeCloseTo(600, 2);
    // The affine part is the bare rotation: the projection row is gone.
    const values = split.affine
      .replace(/^matrix3d\(|\)$/g, '')
      .split(',')
      .map((v) => parseFloat(v.trim()));
    expect(values).toHaveLength(16);
    expect(values[3]).toBeCloseTo(0, 6);
    expect(values[7]).toBeCloseTo(0, 6);
    expect(values[11]).toBeCloseTo(0, 6);
    expect(values[0]).toBeCloseTo(0.707107, 4);
    expect(values[8]).toBeCloseTo(-0.707107, 4);
  });

  test('leaves a plane transform alone', () => {
    expect(splitProjectiveTransform('matrix(2, 0, 0, 2, 10, 20)')).toBeNull();
    // A flat matrix3d (scale only) carries no projection row either.
    expect(
      splitProjectiveTransform('matrix3d(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 10, 20, 0, 1)'),
    ).toBeNull();
  });
});

describe('liftBoxOntoCameraWrapper', () => {
  const html = '<div style="position: absolute; left: 60px; top: 40px; width: 120px; height: 80px; background-color: #38BDF8">x</div>';

  test('moves only the offset onto the camera layer', () => {
    const wrapped = liftBoxOntoCameraWrapper(html, 600, '0px 40px');
    expect(wrapped).not.toBeNull();
    expect(wrapped).toContain('perspective: 600px');
    expect(wrapped).toContain('perspective-origin: 0px 40px');
    // Wrapper keeps the layout slot; the child is pinned to its origin.
    expect(wrapped.startsWith('<div style="position: absolute; left: 60px; top: 40px;')).toBe(true);
    expect(wrapped).toContain('left: 0px; top: 0px;');
    // The element box keeps its absolute size: `width: 100%` would make `box.widthPx` NaN and break
    // percentage-derived properties such as `border-radius: 50%` and `background-size`.
    expect(wrapped).toContain('width: 120px');
    expect(wrapped).toContain('height: 80px');
    expect(wrapped).not.toContain('width: 100%');
    // Balanced tags: one wrapper div plus the original element div.
    expect((wrapped.match(/<div /g) || []).length).toBe(2);
    expect((wrapped.match(/<\/div>/g) || []).length).toBe(2);
  });

  test('declines when the box has no absolute size', () => {
    const relative =
      '<div style="position: absolute; left: 0px; top: 0px; width: 100%; height: 100%">x</div>';
    expect(liftBoxOntoCameraWrapper(relative, 600, '0px 40px')).toBeNull();
    const auto =
      '<div style="position: absolute; left: 0px; top: 0px; width: auto; height: 80px">x</div>';
    expect(liftBoxOntoCameraWrapper(auto, 600, '0px 40px')).toBeNull();
  });

  test('keeps an authored attribute on the element box, not on the camera layer', () => {
    // `applyLayerName` names the element before the wrapper is added, so the wrapper must not
    // swallow the name: `Layer.name` has to stay on the layer that carries content and animation.
    const named = '<div name="card" style="position: absolute; left: 60px; top: 40px; width: 120px; height: 80px; background-color: rgb(56, 189, 248)" id="x">x</div>';
    const wrapped = liftBoxOntoCameraWrapper(named, 600, '0px 40px');
    expect(wrapped).not.toBeNull();
    expect(wrapped.startsWith('<div style="position: absolute; left: 60px; top: 40px;')).toBe(true);
    expect(wrapped).not.toContain('<div name="card" style="position: absolute; left: 60px');
    expect(wrapped).toContain('<div name="card" style="position: absolute; left: 0px;');
    expect((wrapped.match(/<div /g) || []).length).toBe(2);
    expect((wrapped.match(/<\/div>/g) || []).length).toBe(2);
  });

  test('declines when the element did not render as a positioned div', () => {
    expect(liftBoxOntoCameraWrapper('<span style="left: 0px">x</span>', 600, '50% 50%')).toBeNull();
    expect(liftBoxOntoCameraWrapper('<div style="position: relative">x</div>', 600, '50% 50%')).toBeNull();
  });
});

describe('rewritePerspectiveKeyframes', () => {
  beforeEach(() => {
    // The camera map lives on globalThis so it survives the browser payload boundary.
    delete globalThis.__pagxAnimCameras;
  });

  test('strips the projection from every stop and records the distance', () => {
    const css =
      '@keyframes pagxAnim0 { 0% { transform: ' + ROTATE_Y_45_PROJECTED + '; } ' +
      '100% { transform: matrix3d(0.5, 0, -0.866025, 0.001443, 0, 1, 0, 0, 0.866025, 0, 0.5, -0.000833, 0, 0, 0, 1); } }';
    const rewritten = rewritePerspectiveKeyframes(css);
    expect(rewritten).not.toContain('-0.00117851');
    expect(rewritten).not.toContain('-0.000833');
    expect(rewritten).toContain('@keyframes pagxAnim0');
    expect(globalThis.__pagxAnimCameras['pagxAnim0']).toBeCloseTo(600, 2);
  });

  test('keeps the block untouched when the stops disagree on the distance', () => {
    const css =
      '@keyframes pagxAnim1 { 0% { transform: ' + ROTATE_Y_45_PROJECTED +
      '; } 100% { transform: matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1); } }';
    const rewritten = rewritePerspectiveKeyframes(css);
    expect(rewritten).toBe(css);
    expect(globalThis.__pagxAnimCameras['pagxAnim1']).toBeUndefined();
  });

  test('leaves flights without a projection alone', () => {
    const css = '@keyframes pagxAnim2 { 0% { transform: matrix(1, 0, 0, 1, 10, 0); } }';
    expect(rewritePerspectiveKeyframes(css)).toBe(css);
  });
});

describe('camera wrapper guards', () => {
  test('a pivot with depth disables the rewrite', () => {
    // `perspective-origin` has no depth component, so a z pivot cannot be carried over; the element
    // keeps its function form instead of being re-placed.
    expect(transformOriginDepth({ transformOrigin: '0px 80px 30px' })).toBeCloseTo(30, 6);
    expect(transformOriginDepth({ transformOrigin: '0px 80px' })).toBe(0);
    expect(transformOriginDepth({ transformOrigin: '50% 50% 0px' })).toBe(0);
  });

  test('perspective-origin is emitted with two components', () => {
    // A three-value declaration is invalid CSS and browsers drop it, which would move the vanishing
    // point to the wrapper centre.
    expect(perspectiveOriginXY({ transformOrigin: '0px 80px 30px' })).toBe('0px 80px');
    expect(perspectiveOriginXY({ transformOrigin: '10px 20px' })).toBe('10px 20px');
    expect(perspectiveOriginXY({})).toBe('50% 50%');
  });
});

describe('camera skip accounting', () => {
  beforeEach(() => {
    delete globalThis.__pagxCameraSkips;
  });

  test('counts one entry per skipped element and reason', () => {
    recordCameraSkip('depth-pivot');
    recordCameraSkip('depth-pivot');
    recordCameraSkip('not-a-sized-div');
    expect(pagxCameraSkips()).toEqual({ 'depth-pivot': 2, 'not-a-sized-div': 1 });
  });

  test('reports an empty map before anything was skipped', () => {
    expect(pagxCameraSkips()).toEqual({});
  });
});

describe('static camera placement', () => {
  beforeEach(() => {
    delete globalThis.__pagxCameraSkips;
  });

  const named = '<div name="card" style="position: absolute; left: 20px; top: 20px; width: 120px; height: 80px">x</div>';

  test('reads the camera from the pre-strip computed transform', () => {
    // `computed.transform` is the matrix3d the browser reports for `perspective(600px) rotateY(20deg)`.
    const projected = {
      transform:
        'matrix3d(0.939693, 0, -0.34202, 0.00057, 0, 1, 0, 0, 0.34202, 0, 0.939693, -0.001566, 0, 0, 0, 1)',
      transformOrigin: '0px 40px',
    };
    // The fixture is a 6-decimal matrix, so the recovered distance is 600.06 rather than exactly 600.
    expect(staticCameraFor(projected).distance).toBeCloseTo(600, 0);
    // A flat matrix3d carries no projection, so nothing to wrap.
    expect(
      staticCameraFor({ transform: 'matrix3d(2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 10, 20, 0, 1)' }),
    ).toBeNull();
    expect(staticCameraFor({ transform: 'matrix(1, 0, 0, 1, 5, 5)' })).toBeNull();
  });

  test('the stripped re-entry never adds a second camera layer', () => {
    // `renderElementBox` re-renders the same element with its transform cleared; wrapping there would
    // put the camera above the author `name` (it is applied by the outermost call).
    const style = { transform: 'none', transformOrigin: '0px 40px' };
    const html = '<div style="position: absolute; left: 0px; top: 0px; width: 120px; height: 80px">x</div>';
    expect(withPerspectiveCamera(html, { _strippedTransform: true }, style, { distance: 600 })).toBe(html);
    // The outermost call does wrap, after naming.
    const wrapped = withPerspectiveCamera(named, {}, style, { distance: 600 });
    expect(wrapped).toContain('perspective: 600px');
    expect(wrapped).toContain('<div name="card" style="position: absolute; left: 0px;');
  });
});
