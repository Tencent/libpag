/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "CocoaPlatform.h"
#import <Foundation/Foundation.h>
#include "NativeTextShaper.h"
#include "TraceImage.h"
#include "pag/pag.h"

namespace pag {
bool CocoaPlatform::registerFallbackFonts() const {
#ifdef TGFX_USE_FREETYPE
  // only works for macOS:
  std::vector<std::string> fontPaths = {"/System/Library/Fonts/PingFang.ttc",
                                        "/System/Library/Fonts/AppleSDGothicNeo.ttc",
                                        "/System/Library/Fonts/Helvetica.ttc",
                                        "/System/Library/Fonts/Myanmar Sangam MN.ttc",
                                        "/System/Library/Fonts/Thonburi.ttc",
                                        "/System/Library/Fonts/Mishafi.ttf",
                                        "/System/Library/Fonts/Menlo.ttc",
                                        "/System/Library/Fonts/Kailasa.ttc",
                                        "/System/Library/Fonts/Kefa.ttc",
                                        "/System/Library/Fonts/KohinoorTelugu.ttc",
                                        "/System/Library/Fonts/ヒラギノ丸ゴ ProN W4.ttc",
                                        "/System/Library/Fonts/Apple Color Emoji.ttc"};
  std::vector<int> ttcIndices = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  PAGFont::SetFallbackFontPaths(fontPaths, ttcIndices);
#else
  // for macOS 10.11+ iOS 9.0+
  std::vector<std::string> fallbackList = {"PingFang SC",       "Apple SD Gothic Neo",
                                           "Apple Color Emoji", "Helvetica",
                                           "Myanmar Sangam MN", "Thonburi",
                                           "Mishafi",           "Menlo",
                                           "Kailasa",           "Kefa",
                                           "Kohinoor Telugu",   "Hiragino Maru Gothic ProN"};
  PAGFont::SetFallbackFontNames(fallbackList);
#endif
  return true;
}

void CocoaPlatform::traceImage(const tgfx::ImageInfo& info, const void* pixels,
                               const std::string& tag) const {
  TraceImage(info, pixels, tag);
}

std::string CocoaPlatform::getCacheDir() const {
  NSString* cachePath =
      [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject];
  cachePath = [cachePath stringByAppendingPathComponent:@"libpag"];
  return [cachePath UTF8String];
}

std::vector<ShapedGlyph> CocoaPlatform::shapeText(const std::string& text,
                                                  std::shared_ptr<tgfx::Typeface> typeface) const {
  return NativeTextShaper::Shape(text, std::move(typeface));
}

}  // namespace pag
