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
    // system font APIs (text module on API14+, font module below) can only be
    // consumed asynchronously or may crash. If the fallback list is registered
    // asynchronously, the first shaping runs against an empty fallback list,
    // produces empty glyphs and caches them, leaving the text invisible even
    // after the fonts are registered later.
    let registeredFromDir = PAGInit.InitFontFallbackFromSystemDir();

    // The directory scan uses file-system ordering and registers every font file,
    // losing the language-priority ordering and the curated subset that the system
    // font config provides. Asynchronously re-register the fallback list from the
    // system API to restore the correct ordering for subsequent shaping. When the
    // synchronous scan failed (rare), this is also the only source of fallback fonts.
    if (deviceInfo.sdkApiVersion >= 14) {
      // font.getUIFontConfig() may crash on API14+, so use the text module API.
      PAGInit.InitFontFallbackWithTextModule();
    } else if (!registeredFromDir) {
      // Below API14 the async text module is unavailable. Only touch the crash-prone
      // font module when the synchronous scan produced nothing.
      PAGInit.InitFontFallbackWithFontModule();
    }
  }

  // Concatenate the two sets (emoji fonts last so they are not matched before text
  // fonts during shaping) and register them. Returns true when at least one font is
  // registered. The native call is guarded so a failure cannot escape Init().
  private static RegisterFallbackPaths(pathSet: Set<string>, emojiPathSet: Set<string>): boolean {
    let paths = Array.from(pathSet).concat(Array.from(emojiPathSet));
    if (paths.length === 0) {
      return false;
    }
    try {
      JPAGFont.SetFallbackFontPaths(paths);
    } catch (e) {
      console.warn('[PAGInit] SetFallbackFontPaths failed: ' + e);
      return false;
    }
    return true;
  }

  // Synchronously collect fonts from the system font directory and register them.
  // Returns true when at least one font is registered.
  private static InitFontFallbackFromSystemDir(): boolean {
    const systemFontDir = '/system/fonts/';
    let pathSet = new Set<string>();
    let emojiPathSet = new Set<string>();
    try {
      let fileNames = fileIo.listFileSync(systemFontDir);
      for (const fileName of fileNames) {
        let lowerCaseName = fileName.toLowerCase();
        // Skip .ttc: the native SetFallbackFontPaths hardcodes ttcIndex to 0, so only
        // the first subface of a collection would ever be reachable as a fallback.
        if (!lowerCaseName.endsWith('.ttf') && !lowerCaseName.endsWith('.otf')) {
          continue;
        }
        let path = systemFontDir + fileName;
        // listFileSync also returns subdirectory names. Guard statSync per file:
        // some protected entries under /system/fonts throw "Permission denied",
        // which must not abort scanning the whole directory. Only skip confirmed
        // directories; when stat is unavailable, keep the path since the ArkTS
        // stat permission differs from the native file read permission, and let
        // the native loader safely reject anything it cannot open.
        try {
          if (fileIo.statSync(path).isDirectory()) {
            continue;
          }
        } catch (e) {
          // Keep the path; native Typeface loading will skip unreadable files.
        }
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
    return PAGInit.RegisterFallbackPaths(pathSet, emojiPathSet);
  }

  // Use the new text module API (API14+) to register the curated, ordered fallback list.
  private static InitFontFallbackWithTextModule() {
    try {
      // Wrap the entry call: the text module object can occasionally be undefined at
      // runtime, and getSystemFontFullNamesByType then throws a synchronous TypeError
      // that the promise .catch below cannot intercept and would escape Init().
      text.getSystemFontFullNamesByType(text.SystemFontType.ALL).then((fontNames: string[]) => {
        if (!fontNames || fontNames.length === 0) {
          return;
        }
        // Return the inner promise so a rejection propagates to the outer catch instead
        // of becoming an unhandled promise rejection.
        return Promise.all(fontNames.map(fontName =>
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
          PAGInit.RegisterFallbackPaths(pathSet, emojiPathSet);
        });
      }).catch((e: Object) => {
        console.warn('[PAGInit] InitFontFallbackWithTextModule failed: ' + e);
      });
    } catch (e) {
      console.warn('[PAGInit] text module is not available: ' + e);
    }
  }

  // Use the legacy font module API (below API14).
  private static InitFontFallbackWithFontModule() {
    try {
      // font.getUIFontConfig() throws when the font module object is undefined at
      // runtime (the root cause of the historical #3232 crash), so guard the call
      // to keep the exception from escaping Init().
      let fontConfig = font.getUIFontConfig();
      if (!fontConfig || !fontConfig.fallbackGroups) {
        console.warn('[PAGInit] fontConfig is not available, skip font fallback initialization');
        return;
      }

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
      PAGInit.RegisterFallbackPaths(pathSet, emojiPathSet);
    } catch (e) {
      console.warn('[PAGInit] font module is not available: ' + e);
    }
  }

  private static InitCacheDir() {
    JPAGDiskCache.SetCacheDir(getContext().cacheDir);
  }
}
