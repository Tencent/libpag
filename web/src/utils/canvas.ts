import { CANVAS_POOL_MAX_SIZE } from '../constant';

const canvasPool = new Array<HTMLCanvasElement | OffscreenCanvas>();

export const isOffscreenCanvas = (element: any) =>
  globalThis.OffscreenCanvas && element instanceof globalThis.OffscreenCanvas;
export const isCanvas = (element: any) => isOffscreenCanvas(element) || element instanceof globalThis.HTMLCanvasElement;

export const getCanvas2D = () => {
  return canvasPool.pop() || createCanvas2D();
};

export const releaseCanvas2D = (canvas: HTMLCanvasElement | OffscreenCanvas) => {
  if (canvasPool.length < CANVAS_POOL_MAX_SIZE) {
    canvasPool.push(canvas);
  }
};

const createCanvas2D = () => {
  try {
    const offscreenCanvas = new OffscreenCanvas(0, 0);
    const context = offscreenCanvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    if (typeof context.measureText === 'function') return offscreenCanvas;
    return document.createElement('canvas');
  } catch (err) {
    return document.createElement('canvas');
  }
};

export const calculateDisplaySize = (canvas: HTMLCanvasElement) => {
  const styleDeclaration = globalThis.getComputedStyle(canvas, null);
  const computedSize = {
    width: Number(styleDeclaration.width.replace('px', '')),
    height: Number(styleDeclaration.height.replace('px', '')),
  };
  if (computedSize.width > 0 && computedSize.height > 0) {
    return computedSize;
  } else {
    const styleSize = {
      width: Number(canvas.style.width.replace('px', '')),
      height: Number(canvas.style.height.replace('px', '')),
    };
    if (styleSize.width > 0 && styleSize.height > 0) {
      return styleSize;
    } else {
      return {
        width: canvas.width,
        height: canvas.height,
      };
    }
  }
};
