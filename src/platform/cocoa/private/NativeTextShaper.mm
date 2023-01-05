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

#include "NativeTextShaper.h"
#include <CoreText/CoreText.h>
#include "tgfx/core/UTF.h"
#include "tgfx/platform/apple/Typeface.h"

namespace pag {
std::optional<PositionedGlyphs> NativeTextShaper::Shape(
    const std::string& text, const std::shared_ptr<tgfx::Typeface>& typeface) {
  auto ctFont = tgfx::TypefaceGetCTFontRef(typeface.get());
  if (ctFont == nullptr) {
    return std::nullopt;
  }
  std::vector<uint32_t> clusters;
  const char* textStart = &(text[0]);
  const char* textStop = textStart + text.size();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    auto uni = tgfx::UTF::NextUTF8(&textStart, textStop);
    auto cluster = oldPosition - &(text[0]);
    clusters.emplace_back(static_cast<uint32_t>(cluster));
    if (0x10000 <= uni && uni <= 0x10FFFF) {
      clusters.emplace_back(static_cast<uint32_t>(cluster));
    }
  }
  auto str = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(), kCFStringEncodingUTF8);
  auto attr = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks);
  CFDictionaryAddValue(attr, kCTFontAttributeName, ctFont);
  auto attrString = CFAttributedStringCreate(kCFAllocatorDefault, str, attr);
  auto line = CTLineCreateWithAttributedString(attrString);
  auto runs = CTLineGetGlyphRuns(line);
  std::vector<std::tuple<std::shared_ptr<tgfx::Typeface>, tgfx::GlyphID, uint32_t>> glyphIDs;
  for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
    auto run = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs, i));
    auto attrs = CTRunGetAttributes(run);
    auto font = (CTFontRef)CFDictionaryGetValue(attrs, kCTFontAttributeName);
    std::shared_ptr<tgfx::Typeface> face;
    if (font == ctFont) {
      face = typeface;
    } else {
      face = tgfx::MakeTypefaceFromCTFont(font);
      if (face == nullptr) {
        continue;
      }
    }
    auto count = CTRunGetGlyphCount(run);
    std::vector<CGGlyph> glyphs(count);
    CTRunGetGlyphs(run, CFRangeMake(0, count), &(glyphs[0]));
    std::vector<CFIndex> indices(count);
    CTRunGetStringIndices(run, CFRangeMake(0, count), &(indices[0]));
    for (size_t j = 0; j < glyphs.size(); j++) {
      glyphIDs.emplace_back(face, static_cast<tgfx::GlyphID>(glyphs[j]), clusters[indices[j]]);
    }
  }
  CFRelease(line);
  CFRelease(attrString);
  CFRelease(attr);
  CFRelease(str);
  return PositionedGlyphs(glyphIDs);
}
}  // namespace pag
