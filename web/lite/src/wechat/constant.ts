import type { wx } from './types';

declare const wx: wx;

export const isAndroid = (() => wx.getSystemInfoSync().platform === 'android')();
