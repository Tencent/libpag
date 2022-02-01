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
}  // namespace pag
