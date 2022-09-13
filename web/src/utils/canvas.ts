const POOL_MAX_SIZE = 10;
const canvasPool = new Array<HTMLCanvasElement | OffscreenCanvas>();

export const isOffscreenCanvas = (element: any) => window.OffscreenCanvas && element instanceof window.OffscreenCanvas;
export const isCanvas = (element: any) => isOffscreenCanvas(element) || element instanceof HTMLCanvasElement;

export const getCanvas2D = () => {
  return canvasPool.pop() || createCanvas2D();
};

export const createCanvas2D = () => {
  try {
    const offscreenCanvas = new OffscreenCanvas(0, 0);
    const context = offscreenCanvas.getContext('2d');
    if (context?.measureText) return offscreenCanvas;
    return document.createElement('canvas');
  } catch (err) {
    return document.createElement('canvas');
  }
};

export const releaseCanvas2D = (canvas: HTMLCanvasElement | OffscreenCanvas) => {
  if (canvasPool.length < POOL_MAX_SIZE) {
    canvasPool.push(canvas);
  }
};
