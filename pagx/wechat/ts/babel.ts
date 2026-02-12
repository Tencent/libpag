/* global globalThis */
// Polyfills for WeChat Mini Program environment: registers WXWebAssembly as WebAssembly
// and ensures globalThis/window are available for Emscripten module initialization.
import type { wx } from './interfaces';

declare const WXWebAssembly: typeof WebAssembly;
declare const wx: wx;
declare const globalThis: any;

globalThis.WebAssembly = WXWebAssembly;
globalThis.isWxWebAssembly = true;
// eslint-disable-next-line no-global-assign
window = globalThis;
