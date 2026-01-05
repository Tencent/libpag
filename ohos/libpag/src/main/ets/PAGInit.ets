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
    let fontConfig = font.getUIFontConfig();
    let paths = new Array<string>();
    let emojiPaths = new Array<string>();
    for (let index = 0; index < fontConfig.fallbackGroups.length; index++) {
      const element = fontConfig.fallbackGroups[index];
      for (let j = 0; j < element.fallback.length; j++) {
        let fontInfo = font.getFontByName(element.fallback[j].family)
        if (fontInfo == null) {
          fontInfo = font.getFontByName(element.fallback[j].family + " Regular")
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
    paths.concat(emojiPaths);
    JPAGFont.SetFallbackFontPaths(paths);
  }

  private static InitCacheDir() {
    JPAGDiskCache.SetCacheDir(getContext().cacheDir);
  }
}
