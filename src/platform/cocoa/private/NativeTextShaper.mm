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

#include "NativeTextShaper.h"
#include <CoreText/CoreText.h>
#include "tgfx/core/UTF.h"
#include "tgfx/platform/apple/CTTypeface.h"

namespace pag {
std::vector<ShapedGlyph> NativeTextShaper::Shape(const std::string& text,
                                                 std::shared_ptr<tgfx::Typeface> typeface) {
  auto mainFont = tgfx::CTTypeface::GetCTFont(typeface.get());
  std::vector<uint32_t> clusters = {};
  const char* textStart = text.data();
  const char* textStop = textStart + text.size();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    auto uni = tgfx::UTF::NextUTF8(&textStart, textStop);
    auto cluster = oldPosition - text.data();
    clusters.emplace_back(static_cast<uint32_t>(cluster));
    if (0x10000 <= uni && uni <= 0x10FFFF) {
      clusters.emplace_back(static_cast<uint32_t>(cluster));
    }
  }
  auto str = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(), kCFStringEncodingUTF8);
  auto attr = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks);
  if (mainFont != nullptr) {
    CFDictionaryAddValue(attr, kCTFontAttributeName, mainFont);
  }
  auto attrString = CFAttributedStringCreate(kCFAllocatorDefault, str, attr);
  auto line = CTLineCreateWithAttributedString(attrString);
  auto runs = CTLineGetGlyphRuns(line);
  std::vector<ShapedGlyph> glyphs = {};
  for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
    auto run = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs, i));
    auto attrs = CTRunGetAttributes(run);
    auto font = (CTFontRef)CFDictionaryGetValue(attrs, kCTFontAttributeName);
    std::shared_ptr<tgfx::Typeface> face;
    if (font == mainFont) {
      face = typeface;
    } else {
      face = tgfx::CTTypeface::MakeFromCTFont(font);
      if (face == nullptr) {
        continue;
      }
    }
    auto count = CTRunGetGlyphCount(run);
    std::vector<CGGlyph> glyphIDs(count);
    CTRunGetGlyphs(run, CFRangeMake(0, count), glyphIDs.data());
    std::vector<CFIndex> indices(count);
    CTRunGetStringIndices(run, CFRangeMake(0, count), indices.data());
    std::vector<CGPoint> positions(count);
    CTRunGetPositions(run, CFRangeMake(0, count), positions.data());
    bool hasColor = face->hasColor();
    for (size_t j = 0; j < glyphIDs.size(); j++) {
      if (hasColor && j > 0 && positions[j].x == positions[j - 1].x) {
        glyphs.back().glyphIDs.emplace_back(static_cast<tgfx::GlyphID>(glyphIDs[j]));
        continue;
      }
      glyphs.emplace_back(face, static_cast<tgfx::GlyphID>(glyphIDs[j]), clusters[indices[j]]);
    }
  }
  CFRelease(line);
  CFRelease(attrString);
  CFRelease(attr);
  CFRelease(str);
  return glyphs;
}
}  // namespace pag
