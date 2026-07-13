/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * WeChat MiniProgram system info utility.
 * Adapts to different base library versions for deprecated APIs:
 * - wx.getSystemInfoSync is deprecated since base library 2.20.1.
 * - wx.getDeviceInfo().benchmarkLevel and modelLevel are deprecated since
 *   3.4.5; use wx.getDeviceBenchmarkInfo() instead.
 * - wx.getDeviceBenchmarkInfo is async (callback-based), the rest are sync.
 */

declare const wx: any;

const MODEL_LEVEL_UNKNOWN = 0;
const MODEL_LEVEL_HIGH = 1;
const MODEL_LEVEL_MID = 2;
const MODEL_LEVEL_LOW = 3;

function compareVersion(v1: string, v2: string): number {
  const parts1 = v1.split('.').map(Number);
  const parts2 = v2.split('.').map(Number);
  const len = Math.max(parts1.length, parts2.length);
  for (let i = 0; i < len; i++) {
    const n1 = parts1[i] || 0;
    const n2 = parts2[i] || 0;
    if (n1 > n2) return 1;
    if (n1 < n2) return -1;
  }
  return 0;
}

function gte(v1: string, v2: string): boolean {
  return compareVersion(v1, v2) >= 0;
}

function getSDKVersion(): string {
  try {
    const appBaseInfo = wx.getAppBaseInfo();
    if (appBaseInfo && appBaseInfo.SDKVersion) {
      return appBaseInfo.SDKVersion;
    }
  } catch (_) {
    // ignore
  }
  try {
    const info = wx.getSystemInfoSync();
    return info.SDKVersion || '';
  } catch (_) {
    return '';
  }
}

function calculateModelLevel(benchmarkLevel: number, platform: string): number {
  if (benchmarkLevel < 0) {
    return MODEL_LEVEL_UNKNOWN;
  }
  if (platform === 'android' || platform === 'Android') {
    if (benchmarkLevel >= 30) return MODEL_LEVEL_HIGH;
    if (benchmarkLevel >= 23) return MODEL_LEVEL_MID;
    return MODEL_LEVEL_LOW;
  }
  if (platform === 'ios' || platform === 'iOS') {
    if (benchmarkLevel >= 36) return MODEL_LEVEL_HIGH;
    if (benchmarkLevel >= 30) return MODEL_LEVEL_MID;
    return MODEL_LEVEL_LOW;
  }
  return MODEL_LEVEL_UNKNOWN;
}

function getDeviceBenchmarkInfoNativeAsync(): Promise<{
  benchmarkLevel: number;
  modelLevel: number;
} | null> {
  return new Promise((resolve) => {
    try {
      wx.getDeviceBenchmarkInfo({
        success(res: any) {
          resolve({
            benchmarkLevel: res.benchmarkLevel,
            modelLevel: res.modelLevel,
          });
        },
        fail() {
          resolve(null);
        },
      });
    } catch (_) {
      resolve(null);
    }
  });
}

async function getDeviceBenchmarkInfo(
  sdkVersion: string,
  platform: string
): Promise<{ benchmarkLevel: number; modelLevel: number }> {
  if (gte(sdkVersion, '3.4.5') && typeof wx.getDeviceBenchmarkInfo === 'function') {
    const result = await getDeviceBenchmarkInfoNativeAsync();
    if (result) {
      return result;
    }
  }
  let benchmarkLevel = -1;
  try {
    const deviceInfo = wx.getDeviceInfo();
    if (deviceInfo && deviceInfo.benchmarkLevel !== undefined) {
      benchmarkLevel = deviceInfo.benchmarkLevel;
    }
  } catch (_) {
    // ignore and fall through
  }
  if (benchmarkLevel < 0) {
    try {
      const info = wx.getSystemInfoSync();
      if (info.benchmarkLevel !== undefined) {
        benchmarkLevel = info.benchmarkLevel;
      }
    } catch (_) {
      // ignore
    }
  }
  return {
    benchmarkLevel,
    modelLevel: calculateModelLevel(benchmarkLevel, platform),
  };
}

async function getSystemInfoNew(sdkVersion: string) {
  const appBaseInfo = wx.getAppBaseInfo();
  const deviceInfo = wx.getDeviceInfo();
  const windowInfo = wx.getWindowInfo();
  const systemSetting = wx.getSystemSetting();

  const platform = deviceInfo.platform || '';
  let system = platform;
  if (platform === 'ios' || platform === 'android') {
    system = platform.charAt(0).toUpperCase() + platform.slice(1);
  }

  const benchInfo = await getDeviceBenchmarkInfo(sdkVersion, platform);

  return {
    system,
    platform,
    SDKVersion: sdkVersion,
    pixelRatio: windowInfo.pixelRatio || deviceInfo.pixelRatio || 1,
    screenWidth: windowInfo.screenWidth || deviceInfo.screenWidth || 0,
    screenHeight: windowInfo.screenHeight || deviceInfo.screenHeight || 0,
    windowWidth: windowInfo.windowWidth || 0,
    windowHeight: windowInfo.windowHeight || 0,
    statusBarHeight: windowInfo.statusBarHeight || 0,
    safeArea: windowInfo.safeArea,
    language: appBaseInfo.language || systemSetting.language || 'zh_CN',
    version: appBaseInfo.version || '',
    benchmarkLevel: benchInfo.benchmarkLevel,
    modelLevel: benchInfo.modelLevel,
    brand: deviceInfo.brand || '',
    model: deviceInfo.model || '',
  };
}

async function getSystemInfoLegacy() {
  const info = wx.getSystemInfoSync();
  const benchInfo = await getDeviceBenchmarkInfo(info.SDKVersion || '', info.platform || '');
  return {
    system: info.system || '',
    platform: info.platform || '',
    SDKVersion: info.SDKVersion || '',
    pixelRatio: info.pixelRatio || 1,
    screenWidth: info.screenWidth || 0,
    screenHeight: info.screenHeight || 0,
    windowWidth: info.windowWidth || 0,
    windowHeight: info.windowHeight || 0,
    statusBarHeight: info.statusBarHeight || 0,
    safeArea: info.safeArea,
    language: info.language || 'zh_CN',
    version: info.version || '',
    benchmarkLevel: benchInfo.benchmarkLevel,
    modelLevel: benchInfo.modelLevel,
    brand: info.brand || '',
    model: info.model || '',
  };
}

export async function getSystemInfo() {
  const sdkVersion = getSDKVersion();
  if (sdkVersion && gte(sdkVersion, '2.20.1')) {
    return getSystemInfoNew(sdkVersion);
  }
  return getSystemInfoLegacy();
}

export function getSDKInfo(): string {
  return getSDKVersion();
}
