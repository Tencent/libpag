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

#include "CGTypeface.h"
#include "CGScalerContext.h"
#include "core/utils/UniqueID.h"
#include "tgfx/core/UTF.h"

namespace tgfx {
std::string StringFromCFString(CFStringRef src) {
  static const CFIndex kCStringSize = 128;
  char temporaryCString[kCStringSize];
  bzero(temporaryCString, kCStringSize);
  CFStringGetCString(src, temporaryCString, kCStringSize, kCFStringEncodingUTF8);
  return {temporaryCString};
}

std::shared_ptr<Typeface> Typeface::MakeFromName(const std::string& fontFamily,
                                                 const std::string& fontStyle) {
  CFMutableDictionaryRef cfAttributes = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  if (!fontFamily.empty()) {
    auto cfFontName =
        CFStringCreateWithCString(kCFAllocatorDefault, fontFamily.c_str(), kCFStringEncodingUTF8);
    if (cfFontName) {
      CFDictionaryAddValue(cfAttributes, kCTFontFamilyNameAttribute, cfFontName);
      CFRelease(cfFontName);
    }
  }
  if (!fontStyle.empty()) {
    auto cfStyleName =
        CFStringCreateWithCString(kCFAllocatorDefault, fontStyle.c_str(), kCFStringEncodingUTF8);
    if (cfStyleName) {
      CFDictionaryAddValue(cfAttributes, kCTFontStyleNameAttribute, cfStyleName);
      CFRelease(cfStyleName);
    }
  }
  std::shared_ptr<CGTypeface> typeface;
  auto cfDesc = CTFontDescriptorCreateWithAttributes(cfAttributes);
  if (cfDesc) {
    auto ctFont = CTFontCreateWithFontDescriptor(cfDesc, 0, nullptr);
    if (ctFont) {
      typeface = CGTypeface::Make(ctFont);
      CFRelease(ctFont);
    }
    CFRelease(cfDesc);
  }
  CFRelease(cfAttributes);
  return typeface;
}

std::shared_ptr<Typeface> Typeface::MakeFromPath(const std::string& fontPath, int) {
  CGDataProviderRef cgDataProvider = CGDataProviderCreateWithFilename(fontPath.c_str());
  if (cgDataProvider == nullptr) {
    return nullptr;
  }
  std::shared_ptr<CGTypeface> typeface;
  auto cgFont = CGFontCreateWithDataProvider(cgDataProvider);
  if (cgFont) {
    auto ctFont = CTFontCreateWithGraphicsFont(cgFont, 0, nullptr, nullptr);
    if (ctFont) {
      typeface = CGTypeface::Make(ctFont);
      CFRelease(ctFont);
    }
    CFRelease(cgFont);
  }
  CGDataProviderRelease(cgDataProvider);
  return typeface;
}

std::shared_ptr<Typeface> Typeface::MakeFromBytes(const void* data, size_t length, int) {
  auto cfData =
      CFDataCreate(kCFAllocatorNull, static_cast<const UInt8*>(data), static_cast<CFIndex>(length));
  if (cfData == nullptr) {
    return nullptr;
  }
  std::shared_ptr<CGTypeface> typeface;
  auto cfDesc = CTFontManagerCreateFontDescriptorFromData(cfData);
  if (cfDesc) {
    auto ctFont = CTFontCreateWithFontDescriptor(cfDesc, 0, nullptr);
    if (ctFont) {
      typeface = CGTypeface::Make(ctFont);
      CFRelease(ctFont);
    }
    CFRelease(cfDesc);
  }
  CFRelease(cfData);
  return typeface;
}

std::shared_ptr<Typeface> Typeface::MakeDefault() {
  static auto typeface = Typeface::MakeFromName("Lucida Sans", "");
  return typeface;
}

std::shared_ptr<CGTypeface> CGTypeface::Make(CTFontRef ctFont) {
  if (ctFont == nullptr) {
    return nullptr;
  }
  auto typeface = std::shared_ptr<CGTypeface>(new CGTypeface(ctFont));
  typeface->weakThis = typeface;
  return typeface;
}

CGTypeface::CGTypeface(CTFontRef ctFont) : _uniqueID(UniqueID::Next()), ctFont(ctFont) {
  CFRetain(ctFont);
}

CGTypeface::~CGTypeface() {
  if (ctFont) {
    CFRelease(ctFont);
  }
}

std::string CGTypeface::fontFamily() const {
  std::string familyName;
  auto ctFamilyName = CTFontCopyName(ctFont, kCTFontFamilyNameKey);
  if (ctFamilyName != nullptr) {
    familyName = StringFromCFString(ctFamilyName);
    CFRelease(ctFamilyName);
  }
  return familyName;
}

std::string CGTypeface::fontStyle() const {
  std::string styleName;
  auto ctStyleName = CTFontCopyName(ctFont, kCTFontStyleNameKey);
  if (ctStyleName != nullptr) {
    styleName = StringFromCFString(ctStyleName);
    CFRelease(ctStyleName);
  }
  return styleName;
}

int CGTypeface::unitsPerEm() const {
  auto cgFont = CTFontCopyGraphicsFont(ctFont, nullptr);
  auto ret = CGFontGetUnitsPerEm(cgFont);
  CGFontRelease(cgFont);
  return ret;
}

bool CGTypeface::hasColor() const {
  CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(ctFont);
  return static_cast<bool>(traits & kCTFontTraitColorGlyphs);
}

static size_t ToUTF16(int32_t uni, uint16_t utf16[2]) {
  if ((uint32_t)uni > 0x10FFFF) {
    return 0;
  }
  int extra = (uni > 0xFFFF);
  if (utf16) {
    if (extra) {
      utf16[0] = (uint16_t)((0xD800 - 64) + (uni >> 10));
      utf16[1] = (uint16_t)(0xDC00 | (uni & 0x3FF));
    } else {
      utf16[0] = (uint16_t)uni;
    }
  }
  return 1 + extra;
}

GlyphID CGTypeface::getGlyphID(const std::string& name) const {
  const char* start = &(name[0]);
  auto uni = UTF::NextUTF8(&start, start + name.size());
  UniChar utf16[2] = {0, 0};
  auto srcCount = ToUTF16(uni, utf16);
  GlyphID macGlyphs[2] = {0, 0};
  CTFontGetGlyphsForCharacters(ctFont, utf16, macGlyphs, static_cast<CFIndex>(srcCount));
  return macGlyphs[0];
}

FontMetrics CGTypeface::getMetrics(float size) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size);
  if (scalerContext == nullptr) {
    return {};
  }
  return scalerContext->generateFontMetrics();
}

Rect CGTypeface::getGlyphBounds(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return Rect::MakeEmpty();
  }
  auto metrics = scalerContext->generateGlyphMetrics(glyphID);
  auto bounds = Rect::MakeXYWH(metrics.left, metrics.top, metrics.width, metrics.height);
  auto advance = metrics.advanceX;
  if (bounds.isEmpty() && advance > 0) {
    auto fontMetrics = scalerContext->generateFontMetrics();
    bounds.setLTRB(0, fontMetrics.ascent, advance, fontMetrics.descent);
  }
  return bounds;
}

float CGTypeface::getGlyphAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                                  bool verticalText) const {
  auto scalerContext =
      CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic, verticalText);
  if (scalerContext == nullptr) {
    return 0;
  }
  auto glyphMetrics = scalerContext->generateGlyphMetrics(glyphID);
  return verticalText ? glyphMetrics.advanceY : glyphMetrics.advanceX;
}

bool CGTypeface::getGlyphPath(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                              Path* path) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return false;
  }
  return scalerContext->generatePath(glyphID, path);
}

std::shared_ptr<TextureBuffer> CGTypeface::getGlyphImage(GlyphID glyphID, float size, bool fauxBold,
                                                         bool fauxItalic, Matrix* matrix) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return nullptr;
  }
  return scalerContext->generateImage(glyphID, matrix);
}

Point CGTypeface::getGlyphVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                                         bool fauxItalic) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return Point::Zero();
  }
  return scalerContext->getGlyphVerticalOffset(glyphID);
}
}  // namespace tgfx
