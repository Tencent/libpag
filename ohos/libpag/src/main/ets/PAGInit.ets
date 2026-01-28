/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

import { deviceInfo } from '@kit.BasicServicesKit';
import { text } from '@kit.ArkGraphics2D';
import { font } from '@kit.ArkUI';
import { JPAGDiskCache, JPAGFont } from 'libpag.so';

export class PAGInit {
  static Init() {
    if (PAGInit.hasInit) {
      return;
    }
    PAGInit.InitFontFallback();
    PAGInit.InitCacheDir();
    PAGInit.hasInit = true;
  }

  private static hasInit: boolean = false;

  private static InitFontFallback() {
    // API14+: prefer the new async API, fallback to the legacy API on failure
    if (deviceInfo.sdkApiVersion >= 14) {
      PAGInit.InitFontFallbackWithTextModule().then((success: boolean) => {
        if (!success) {
          PAGInit.InitFontFallbackWithFontModule();
        }
      });
      return;
    }

    // Below API14: use the legacy API
    PAGInit.InitFontFallbackWithFontModule();
  }

  // Use the new text module API (API14+)
  private static async InitFontFallbackWithTextModule(): Promise<boolean> {
    try {
      let fontNames: string[] = await text.getSystemFontFullNamesByType(text.SystemFontType.ALL);
      if (fontNames && fontNames.length > 0) {
        let paths = new Array<string>();
        for (let fontName of fontNames) {
          let fontDescriptor = await text.getFontDescriptorByFullName(fontName, text.SystemFontType.ALL);
          if (fontDescriptor && fontDescriptor.path) {
            paths.push(fontDescriptor.path);
          }
        }
        if (paths.length > 0) {
          JPAGFont.SetFallbackFontPaths(paths);
          return true;
        }
      }
    } catch (e) {
      console.warn('[PAGInit] text.getSystemFontFullNamesByType failed: ' + e);
    }
    return false;
  }

  // Use the legacy font module API
  private static InitFontFallbackWithFontModule() {
    let fontConfig = font.getUIFontConfig();
    if (!fontConfig || !fontConfig.fallbackGroups) {
      console.warn('[PAGInit] fontConfig is not available, skip font fallback initialization');
      return;
    }

    let paths = new Array<string>();
    let emojiPaths = new Array<string>();
    for (let index = 0; index < fontConfig.fallbackGroups.length; index++) {
      const element = fontConfig.fallbackGroups[index];
      if (!element || !element.fallback) {
        continue;
      }
      for (let j = 0; j < element.fallback.length; j++) {
        let fontInfo = font.getFontByName(element.fallback[j].family);
        if (fontInfo == null) {
          fontInfo = font.getFontByName(element.fallback[j].family + " Regular");
        }
        if (fontInfo != null) {
          if (fontInfo.fullName.toLowerCase().indexOf("emoji") > 0) {
            emojiPaths.push(fontInfo.path);
          } else {
            paths.push(fontInfo.path);
          }
        }
      }
    }
    paths = paths.concat(emojiPaths);
    JPAGFont.SetFallbackFontPaths(paths);
  }

  private static InitCacheDir() {
    JPAGDiskCache.SetCacheDir(getContext().cacheDir);
  }
}
