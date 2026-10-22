/** @jest-environment jsdom */
'use strict';

const {
  installControlPaints, beginUntransformedLayout, endUntransformedLayout, captureControlPaints,
} = require('../dist/lib/control-capture');
const { computedStyle, setRect, nodePath } = require('./dom-helpers');

let computed;
beforeEach(() => {
  document.head.innerHTML = '';
  document.body.innerHTML = '';
  document.documentElement.removeAttribute('style');
  document.body.removeAttribute('style');
  computed = new WeakMap();
  jest.spyOn(window, 'getComputedStyle').mockImplementation(el => computedStyle(el, computed.get(el)));
});
afterEach(() => {
  endUntransformedLayout();
  jest.restoreAllMocks();
});

function pageFixture() {
  return {
    evaluate: jest.fn(async (fn, arg) => fn(arg)),
    screenshot: jest.fn().mockResolvedValue(new Uint8Array([137, 80, 78, 71])),
  };
}

describe('control providers and transform measurement', () => {
  test('installs paints only at valid element paths and replaces old providers', () => {
    document.body.innerHTML = '<input>text';
    const input = document.querySelector('input');
    const paint = { path: nodePath(input), src: 'data:image/png;base64,AAAA' };
    installControlPaints([paint, { path: [999, 0] }, { path: nodePath(input.nextSibling) }]);
    expect(window.__pagxControlPaints.get(input)).toBe(paint);
    expect(window.__pagxControlPaints.has(input.nextSibling)).toBe(false);
    installControlPaints([]);
    expect(window.__pagxControlPaints.has(input)).toBe(false);
  });

  test('keeps containing blocks while neutralizing transforms, then restores exact inline styles', () => {
    document.body.innerHTML = '<div style="transform:scale(2)!important;rotate:15deg;scale:2;translate:4px;color:red"></div><p></p><span></span>';
    const div = document.querySelector('div');
    const p = document.querySelector('p');
    const span = document.querySelector('span');
    computed.set(p, { transform: 'rotate(20deg)' }); // Stylesheet-only transform.
    Object.defineProperty(span, 'style', { value: undefined, configurable: true });
    const original = div.getAttribute('style');
    beginUntransformedLayout();
    expect(div.style.transform).toBe('matrix(1,0,0,1,0,0)');
    expect(div.style.rotate).toBe('0deg');
    expect(div.style.scale).toBe('1');
    expect(div.style.translate).toBe('0px');
    expect(div.style.getPropertyPriority('transform')).toBe('important');
    expect(p.style.transform).toBe('matrix(1,0,0,1,0,0)');
    endUntransformedLayout();
    expect(div.getAttribute('style')).toBe(original);
    expect(p.hasAttribute('style')).toBe(false);
    expect(window.__pagxCaptureTransforms).toBeUndefined();
    expect(() => endUntransformedLayout()).not.toThrow();
  });
});

describe('captureControlPaints', () => {
  test('skips invisible, empty, text-only and custom-appearance controls without hiding the page', async () => {
    document.body.innerHTML = '<input type="text"><input type="password"><input type="hidden">' +
      '<input type="checkbox" style="appearance:none"><input type="radio" style="appearance:none">' +
      '<select style="appearance:none"></select><progress style="display:none"></progress>' +
      '<meter style="visibility:hidden"></meter><fieldset></fieldset><input type="range">';
    document.querySelectorAll('*').forEach(el => setRect(el));
    setRect(document.querySelector('fieldset'), { width: 0 });
    setRect(document.querySelector('[type=range]'), { height: 0 });
    const original = document.body.innerHTML;
    const page = pageFixture();
    expect(await captureControlPaints(page, 'puppeteer')).toEqual([]);
    expect(page.screenshot).not.toHaveBeenCalled();
    expect(document.body.innerHTML).toBe(original);
    expect(document.body.hasAttribute('style')).toBe(false);
  });

  test.each(['puppeteer', 'playwright'])('%s isolates native paint, rounds the clip outward and restores the page', async engine => {
    document.body.innerHTML = '<div style="opacity:0.5;filter:blur(2px);overflow:hidden">' +
      '<input type="checkbox" style="width:100.25px;height:20.25px;transform:rotate(5deg);outline:2px solid red;outline-offset:1px">' +
      '<b style="visibility:visible!important">overlap</b></div>';
    const control = document.querySelector('input');
    computed.set(control, { outlineStyle: 'solid', outlineWidth: '2px', outlineOffset: '1px' });
    setRect(control, { left: 10.25, top: 20.25, width: 100.25, height: 20.25 });
    jest.replaceProperty(window, 'devicePixelRatio', 2);
    const original = document.documentElement.outerHTML;
    const path = nodePath(control);
    const page = pageFixture();
    page.screenshot.mockImplementation(async options => {
      expect(options).toEqual({
        type: 'png', omitBackground: true,
        clip: { x: 7, y: 17, width: 106.5, height: 26.5 },
        ...(engine === 'puppeteer' ? { captureBeyondViewport: true } : {}),
      });
      expect(control.style.visibility).toBe('visible');
      expect(control.parentElement.style.opacity).toBe('1');
      expect(control.parentElement.style.overflow).toBe('visible');
      expect(document.querySelector('b').style.visibility).toBe('hidden');
      expect(document.querySelector('b').style.getPropertyPriority('visibility')).toBe('important');
      expect(document.body.style.background).toBe('transparent');
      expect(document.documentElement.style.background).toBe('transparent');
      expect(control.style.boxShadow).toBe('none');
      expect(document.head.textContent).toContain('*::before,*::after { visibility: hidden !important; }');
      return new Uint8Array([137, 80, 78, 71]);
    });
    expect(await captureControlPaints(page, engine)).toEqual([{
      path, src: 'data:image/png;base64,iVBORw==', x: -3.25, y: -3.25, width: 106.5, height: 26.5,
    }]);
    expect(document.documentElement.outerHTML).toBe(original);
    expect(window.__pagxIsolatedControl).toBeUndefined();
    expect(window.__pagxCaptureTransforms).toBeUndefined();
  });

  test('captures multiple native controls without leaking the previous control visibility', async () => {
    document.body.innerHTML = '<input type="range"><select></select><progress></progress><meter></meter><fieldset></fieldset>';
    const controls = Array.from(document.body.children);
    controls.forEach(el => setRect(el, { left: 4, top: 5 }));
    const original = document.body.innerHTML;
    const page = pageFixture();
    page.screenshot.mockImplementation(async () => {
      const active = controls[page.screenshot.mock.calls.length - 1];
      for (const control of controls) expect(control.style.visibility).toBe(control === active ? 'visible' : 'hidden');
      return Buffer.from('png');
    });
    const paints = await captureControlPaints(page, 'puppeteer');
    expect(paints.map(paint => paint.path)).toEqual(controls.map(nodePath));
    expect(page.screenshot).toHaveBeenCalledTimes(5);
    expect(document.body.innerHTML).toBe(original);
  });

  test('moves negative coordinates on-canvas and preserves local offsets at fallback DPR', async () => {
    document.body.innerHTML = '<input type="radio">';
    const control = document.querySelector('input');
    jest.replaceProperty(window, 'devicePixelRatio', 0);
    control.getBoundingClientRect = () => {
      const moved = control.style.transform === 'translate(11px,6px)';
      const left = moved ? 0.75 : -10.25;
      const top = moved ? 0.75 : -5.25;
      return { left, top, right: left + 100, bottom: top + 20, width: 100, height: 20 };
    };
    const page = pageFixture();
    const [paint] = await captureControlPaints(page, 'puppeteer');
    expect(paint).toMatchObject({ x: -0.75, y: -0.75, width: 101, height: 21 });
    expect(page.screenshot).toHaveBeenCalledWith(expect.objectContaining({ clip: { x: 0, y: 0, width: 101, height: 21 } }));
    expect(control.hasAttribute('style')).toBe(false);
  });

  test.each([{ left: -20 }, { top: -20 }])('skips a clip that remains off-canvas: %j', async rect => {
    document.body.innerHTML = '<input type="radio">';
    setRect(document.querySelector('input'), rect);
    const page = pageFixture();
    const original = document.documentElement.outerHTML;
    expect(await captureControlPaints(page, 'puppeteer')).toEqual([]);
    expect(page.screenshot).not.toHaveBeenCalled();
    expect(document.documentElement.outerHTML).toBe(original);
  });

  test('restores all styles and removes the isolation stylesheet after a screenshot failure', async () => {
    document.body.innerHTML = '<div style="opacity:0.4"><input type="color"></div>';
    setRect(document.querySelector('input'));
    const original = document.documentElement.outerHTML;
    const page = pageFixture();
    const error = new Error('screenshot failed');
    page.screenshot.mockRejectedValue(error);
    await expect(captureControlPaints(page, 'playwright')).rejects.toBe(error);
    expect(document.documentElement.outerHTML).toBe(original);
    expect(window.__pagxIsolatedControl).toBeUndefined();
    expect(window.__pagxCaptureTransforms).toBeUndefined();
  });

  test('restores hidden page styles if bounds evaluation fails before isolation starts', async () => {
    document.body.innerHTML = '<input type="color">';
    setRect(document.querySelector('input'));
    const original = document.documentElement.outerHTML;
    const page = pageFixture();
    const error = new Error('detached control');
    const evaluate = page.evaluate.getMockImplementation();
    page.evaluate.mockImplementation(async (fn, arg) => {
      if (arg) throw error;
      return evaluate(fn, arg);
    });
    await expect(captureControlPaints(page, 'puppeteer')).rejects.toBe(error);
    expect(document.documentElement.outerHTML).toBe(original);
    expect(page.screenshot).not.toHaveBeenCalled();
  });
});
