import type { wx } from './interfaces';

declare const wx: wx;

export const isAndroid = (() => wx.getSystemInfoSync().platform === 'android')();

export const MP4_CACHE_PATH = `${wx.env.USER_DATA_PATH}/pag/`;
