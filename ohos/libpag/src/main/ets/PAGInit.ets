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
import { fileIo } from '@kit.CoreFileKit';
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
    // Synchronously scan the system font directory and register the fallback
    // fonts before Init() returns. This is critical for correctness: PAGFile.Load
    // triggers the first text shaping synchronously right after Init(), while the
    // text module API (getSystemFontFullNamesByType) can only be consumed
    // asynchronously. If the fallback list is registered asynchronously, the first
    // shaping runs against an empty fallback list, produces empty glyphs and caches
    // them, leaving the text invisible even after the fonts are registered later.
    if (PAGInit.InitFontFallbackFromSystemDir()) {
      return;
    }

    // Fallback path when the system font directory is inaccessible (rare).
    if (deviceInfo.sdkApiVersion >= 14) {
      // font.getUIFontConfig() may crash on API14+, so use the text module API.
      PAGInit.InitFontFallbackWithTextModule();
      return;
    }
    PAGInit.InitFontFallbackWithFontModule();
  }

  // Synchronously collect fonts from the system font directory and register them.
  // Returns true when at least one font is registered.
  private static InitFontFallbackFromSystemDir(): boolean {
    const systemFontDir = '/system/fonts/';
    // Use two sets to deduplicate. Emoji fonts are appended at the end so that
    // they are not matched before text fonts during shaping.
    let pathSet = new Set<string>();
    let emojiPathSet = new Set<string>();
    try {
      let fileNames = fileIo.listFileSync(systemFontDir);
      for (const fileName of fileNames) {
        let lowerCaseName = fileName.toLowerCase();
        if (!lowerCaseName.endsWith('.ttf') && !lowerCaseName.endsWith('.ttc') &&
          !lowerCaseName.endsWith('.otf')) {
          continue;
        }
        let path = systemFontDir + fileName;
        if (lowerCaseName.includes('emoji')) {
          emojiPathSet.add(path);
        } else {
          pathSet.add(path);
        }
      }
    } catch (e) {
      console.warn('[PAGInit] scan system font directory failed: ' + e);
      return false;
    }
    let paths = Array.from(pathSet).concat(Array.from(emojiPathSet));
    if (paths.length === 0) {
      return false;
    }
    JPAGFont.SetFallbackFontPaths(paths);
    return true;
  }

  // Use the new text module API (API14+). Only used as a fallback when the system
  // font directory is inaccessible.
  private static InitFontFallbackWithTextModule() {
    text.getSystemFontFullNamesByType(text.SystemFontType.ALL).then((fontNames: string[]) => {
      if (!fontNames || fontNames.length === 0) {
        return;
      }
      Promise.all(fontNames.map(fontName =>
        text.getFontDescriptorByFullName(fontName, text.SystemFontType.ALL)
      )).then((descriptors) => {
        let pathSet = new Set<string>();
        let emojiPathSet = new Set<string>();
        for (const descriptor of descriptors) {
          if (!descriptor || !descriptor.path) {
            continue;
          }
          if ((descriptor.fullName || '').toLowerCase().includes('emoji')) {
            emojiPathSet.add(descriptor.path);
          } else {
            pathSet.add(descriptor.path);
          }
        }
        let paths = Array.from(pathSet).concat(Array.from(emojiPathSet));
        if (paths.length > 0) {
          JPAGFont.SetFallbackFontPaths(paths);
        }
      });
    }).catch((e: Object) => {
      console.warn('[PAGInit] InitFontFallbackWithTextModule failed: ' + e);
    });
  }

  // Use the legacy font module API (below API14).
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
