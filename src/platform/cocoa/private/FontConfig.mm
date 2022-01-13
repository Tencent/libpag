/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "FontConfig.h"
#include <CoreText/CTFont.h>
#include <Foundation/Foundation.h>

namespace pag {
bool FontConfig::RegisterFallbackFonts() {
#ifdef TGFX_USE_FREETYPE
  // only works for macOS:
  std::vector<std::string> fontPaths = {"/System/Library/Fonts/PingFang.ttc",
                                        "/System/Library/Fonts/Apple Color Emoji.ttc"};
  std::vector<int> ttcIndices = {1, 0};
  pag::PAGFont::SetFallbackFontPaths(fontPaths, ttcIndices);
#else
  // for macOS 10.11+ iOS 9.0+
  std::vector<std::string> fallbackList = {"PingFang SC",       "Apple SD Gothic Neo",
                                           "Apple Color Emoji", "Helvetica",
                                           "Myanmar Sangam MN", "Thonburi",
                                           "Mishafi",           "Menlo",
                                           "Kailasa",           "Kefa",
                                           "Kohinoor Telugu",   "Hiragino Maru Gothic ProN"};
  pag::PAGFont::SetFallbackFontNames(fallbackList);
#endif
  return true;
}

PAGFont ParseFont(CGDataProviderRef fontData) {
  if (fontData == nullptr) {
    return {"", ""};
  }
  auto cgFont = CGFontCreateWithDataProvider(fontData);
  if (cgFont == nullptr) {
    return {"", ""};
  }
  auto ctFont = CTFontCreateWithGraphicsFont(cgFont, 0, nullptr, nullptr);
  if (ctFont == nullptr) {
    CGFontRelease(cgFont);
    return {"", ""};
  }
  std::string familyName = "";
  std::string styleName = "";

  auto ctFamilyName = CTFontCopyName(ctFont, kCTFontFamilyNameKey);
  if (ctFamilyName != NULL) {
    familyName = ((NSString*)ctFamilyName).UTF8String;
    CFRelease(ctFamilyName);
  }
  auto ctStyleName = CTFontCopyName(ctFont, kCTFontStyleNameKey);
  if (ctStyleName != NULL) {
    styleName = ((NSString*)ctStyleName).UTF8String;
    CFRelease(ctStyleName);
  }

  CFRelease(ctFont);
  CGFontRelease(cgFont);
  return {familyName, styleName};
}

PAGFont FontConfig::Parse(const void* data, size_t length, int) {
  if (length == 0) {
    return {"", ""};
  }
  CGDataProviderRef fontData = CGDataProviderCreateWithData(nullptr, data, length, nullptr);
  auto font = ParseFont(fontData);
  CGDataProviderRelease(fontData);
  return font;
}

PAGFont FontConfig::Parse(const std::string& fontPath, int) {
  if (fontPath.empty()) {
    return {"", ""};
  }
  CGDataProviderRef fontData = CGDataProviderCreateWithFilename(fontPath.c_str());
  auto font = ParseFont(fontData);
  CGDataProviderRelease(fontData);
  return font;
}
}  // namespace pag
