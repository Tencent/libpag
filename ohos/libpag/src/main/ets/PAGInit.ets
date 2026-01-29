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
        let descriptors = await Promise.all(fontNames.map(fontName =>
          text.getFontDescriptorByFullName(fontName, text.SystemFontType.ALL)
        ));
        let paths = Array.from(new Set(descriptors.filter(d => d && d.path).map(d => d.path)));
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

    // Use two sets to deduplicate, emoji fonts will be appended at the end
    let pathSet = new Set<string>();
    let emojiPathSet = new Set<string>();
    for (const element of fontConfig.fallbackGroups) {
      if (!element || !element.fallback) {
        continue;
      }
      for (const fallback of element.fallback) {
        let fontInfo = font.getFontByName(fallback.family);
        if (fontInfo == null) {
          fontInfo = font.getFontByName(fallback.family + " Regular");
        }
        if (fontInfo != null) {
          if (fontInfo.fullName.toLowerCase().includes("emoji")) {
            emojiPathSet.add(fontInfo.path);
          } else {
            pathSet.add(fontInfo.path);
          }
        }
      }
    }
    JPAGFont.SetFallbackFontPaths(Array.from(pathSet).concat(Array.from(emojiPathSet)));
  }

  private static InitCacheDir() {
    JPAGDiskCache.SetCacheDir(getContext().cacheDir);
  }
}
