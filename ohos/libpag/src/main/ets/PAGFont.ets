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

import { resourceManager } from '@kit.LocalizationKit';
import { JPAGFont } from 'libpag.so';
import { PAGUtils } from './private/PAGUtils';

export class PAGFont {
  static RegisterFontFromPath(fontPath: string, ttcIndex?: number,
    fontFamily?: string, fontStyle?: string): PAGFont {
    let jFont = JPAGFont.RegisterFontFromPath(fontPath, ttcIndex, fontFamily, fontStyle);
    return PAGUtils.ToPAGFont(jFont);
  }

  static RegisterFontFromAsset(manager: resourceManager.ResourceManager, fileName: string, ttcIndex?: number,
    fontFamily?: string, fontStyle?: string): PAGFont {
    let jFont = JPAGFont.RegisterFontFromAsset(manager, fileName, ttcIndex, fontFamily, fontStyle);
    return PAGUtils.ToPAGFont(jFont);
  }

  static UnregisterFont(font: PAGFont): void {
    JPAGFont.UnregisterFont(new JPAGFont(font.fontFamily, font.fontStyle));
  }

  constructor(fontFamily?: string, fontStyle?: string) {
    this.fontFamily = fontFamily ? fontFamily : "";
    this.fontStyle = fontStyle ? fontStyle : "";
  }

  /**
   * A string with the name of the font family.
   **/
  fontFamily: string;
  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  fontStyle: string;
}
