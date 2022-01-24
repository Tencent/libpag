import { isWechatMiniProgram } from './ua'
import { wxOffscreenManager } from './offscreen-canvas-manager'
import { WebVideoReader } from '../core/video-reader'
import { WechatVideoReader } from '../core/wechat-video-reader'


declare const WXWebAssembly: any;
declare const wx: any;

// eslint-disable-next-line
globalThis.isWxWebAssembly = false;
if (typeof WebAssembly !== "object" && isWechatMiniProgram) {
  // eslint-disable-next-line
  globalThis.isWxWebAssembly = true;
  // eslint-disable-next-line
  globalThis.WebAssembly = WXWebAssembly;
}

export const getWechatElementById = (id: string): Promise<any> => {
  return new Promise(resolve => {
    const query = wx.createSelectorQuery();
    query.select(`#${id}`).node((res) => {
      resolve(res.node)
    }).exec()
  })
};

export const requestAnimationFrame = (() => {
  if (isWechatMiniProgram) {
    return function(callback, _lastTime = 0) {
      let lastTime = _lastTime;
      const currTime = new Date().getTime();
      const timeToCall = Math.max(0, 16.7 - (currTime - lastTime));
      lastTime = currTime + timeToCall;
      const id = setTimeout(() => {
          callback(currTime + timeToCall, lastTime);
      }, timeToCall);
      return Number(id);
    };
  } else {
    return window.requestAnimationFrame;
  }
})();

export const cancelAnimationFrame = (() => {
  if (isWechatMiniProgram) {
    return function(timer) {
      return clearTimeout(timer)
    };
  } else {
    return window.cancelAnimationFrame;
  }
})();


export const createCanvas = (): HTMLCanvasElement | OffscreenCanvas => {
  if (isWechatMiniProgram) {
    const wxFreeNode = wxOffscreenManager.getFreeCanvas();
    return wxFreeNode.canvas;
  } else {
    try {
      const offscreenCanvas = new OffscreenCanvas(0, 0);
      const context = offscreenCanvas.getContext('2d');
      if (context?.measureText) return offscreenCanvas;
      return document.createElement('canvas');
    } catch (err) {
      return document.createElement('canvas');
    }
  }
};

export const VideoReader = (() => {
  return isWechatMiniProgram ? WechatVideoReader :WebVideoReader;
})();

