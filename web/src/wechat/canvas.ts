import { CANVAS_POOL_MAX_SIZE } from '../constant';

import type { wx } from './interfaces';

declare const wx: wx;

const canvasPool = new Array<OffscreenCanvas>();

export const getCanvas2D = (width: number, height: number) => {
  let canvas = canvasPool.pop() || createCanvas2D();
  if (canvas !== null) {
    canvas.width = width;
    canvas.height = height;
  }
  return canvas;
};

export const releaseCanvas2D = (canvas: OffscreenCanvas) => {
  if (canvasPool.length < CANVAS_POOL_MAX_SIZE) {
    canvasPool.push(canvas);
  }
};

const createCanvas2D = () => {
  return wx.createOffscreenCanvas({ type: '2d' });
};
