
/* #if _WECHAT
declare const globalThis: any;
export const ANDROID = false;
export const MOBILE = false;
export const MACOS = false
export const IPHONE = false;
export const isWechatMiniProgram = !!globalThis.wx;
// eslint-disable-next-line
globalThis.isWxWebAssembly = false;
if (typeof WebAssembly !== "object" && isWechatMiniProgram) {
  // eslint-disable-next-line
  globalThis.isWxWebAssembly = true;
  // eslint-disable-next-line
  globalThis.WebAssembly = WXWebAssembly;
}
//#else */
const nav = navigator.userAgent;
export const ANDROID = /android|adr/i.test(nav);
export const MOBILE = /(mobile)/i.test(nav) && ANDROID;
export const MACOS = !(/(mobile)/i.test(nav) || MOBILE) && /Mac OS X/i.test(nav);
export const IPHONE = /(iphone|ipad|ipod)/i.test(nav);
// #endif


