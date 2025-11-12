import type { wx } from '../interfaces';
declare const wx: wx;
const deviceInfo = wx.getDeviceInfo();
export const ANDROIDWECHAT = deviceInfo.platform === "android";