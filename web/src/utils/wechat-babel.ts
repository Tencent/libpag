/* #if _WECHAT
import { isWechatMiniProgram } from './ua';
//#else */
// #endif

declare const WXWebAssembly: any;
declare const wx: any;

export const getWechatElementById = (id: string): Promise<any> => {
  return new Promise((resolve) => {
    const query = wx.createSelectorQuery();
    query
      .select(`#${id}`)
      .node((res) => {
        resolve(res.node);
      })
      .exec();
  });
};

export const requestAnimationFrame = (() => {
  return function (callback, _lastTime = 0) {
    let lastTime = _lastTime;
    const currTime = new Date().getTime();
    const timeToCall = Math.max(0, 16.7 - (currTime - lastTime));
    lastTime = currTime + timeToCall;
    const id = setTimeout(() => {
      callback(currTime + timeToCall, lastTime);
    }, timeToCall);
    return Number(id);
  };
})();

export const cancelAnimationFrame = (() => {
  return function (timer) {
    return clearTimeout(timer);
  };
})();
