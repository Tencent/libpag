import { CANVAS_POOL_MAX_SIZE } from '../constant';

const canvasPool = new Array<HTMLCanvasElement | OffscreenCanvas>();

export const isOffscreenCanvas = (element: any) => window.OffscreenCanvas && element instanceof window.OffscreenCanvas;
export const isCanvas = (element: any) => isOffscreenCanvas(element) || element instanceof HTMLCanvasElement;

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
