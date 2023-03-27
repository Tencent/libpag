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
#include "tgfx/utils/UTF.h"
#include "utils/UniqueID.h"

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
  auto copyData = Data::MakeWithCopy(data, length);
  if (copyData->size() == 0) {
    return nullptr;
  }
  auto cfData =
      CFDataCreateWithBytesNoCopy(kCFAllocatorNull, static_cast<const UInt8*>(copyData->data()),
                                  static_cast<CFIndex>(copyData->size()), kCFAllocatorNull);
  if (cfData == nullptr) {
    return nullptr;
  }
  std::shared_ptr<CGTypeface> typeface;
  auto cfDesc = CTFontManagerCreateFontDescriptorFromData(cfData);
  if (cfDesc) {
    auto ctFont = CTFontCreateWithFontDescriptor(cfDesc, 0, nullptr);
    if (ctFont) {
      typeface = CGTypeface::Make(ctFont, std::move(copyData));
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

std::shared_ptr<CGTypeface> CGTypeface::Make(CTFontRef ctFont, std::shared_ptr<Data> data) {
  if (ctFont == nullptr) {
    return nullptr;
  }
  auto typeface = std::shared_ptr<CGTypeface>(new CGTypeface(ctFont, std::move(data)));
  typeface->weakThis = typeface;
  return typeface;
}

CGTypeface::CGTypeface(CTFontRef ctFont, std::shared_ptr<Data> data)
    : _uniqueID(UniqueID::Next()), ctFont(ctFont), data(std::move(data)) {
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

std::shared_ptr<Data> CGTypeface::getBytes() const {
  return data;
}

std::shared_ptr<Data> CGTypeface::copyTableData(FontTableTag tag) const {
  auto cfData =
      CTFontCopyTable(ctFont, static_cast<CTFontTableTag>(tag), kCTFontTableOptionNoOptions);
  if (cfData == nullptr) {
    auto cgFont = CTFontCopyGraphicsFont(ctFont, nullptr);
    cfData = CGFontCopyTableForTag(cgFont, tag);
    CGFontRelease(cgFont);
  }
  if (cfData == nullptr) {
    return nullptr;
  }
  const auto* bytePtr = CFDataGetBytePtr(cfData);
  auto length = CFDataGetLength(cfData);
  return Data::MakeAdopted(
      bytePtr, length, [](const void*, void* context) { CFRelease((CFDataRef)context); },
      (void*)cfData);
}

FontMetrics CGTypeface::getMetrics(float size) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size);
  if (scalerContext == nullptr) {
    return {};
  }
  return scalerContext->generateFontMetrics();
}

Rect CGTypeface::getBounds(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic) const {
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

float CGTypeface::getAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                             bool verticalText) const {
  auto scalerContext =
      CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic, verticalText);
  if (scalerContext == nullptr) {
    return 0;
  }
  auto glyphMetrics = scalerContext->generateGlyphMetrics(glyphID);
  return verticalText ? glyphMetrics.advanceY : glyphMetrics.advanceX;
}

bool CGTypeface::getPath(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                         Path* path) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return false;
  }
  return scalerContext->generatePath(glyphID, path);
}

std::shared_ptr<ImageBuffer> CGTypeface::getGlyphImage(GlyphID glyphID, float size, bool fauxBold,
                                                       bool fauxItalic, Matrix* matrix) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return nullptr;
  }
  return scalerContext->generateImage(glyphID, matrix);
}

Point CGTypeface::getVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                                    bool fauxItalic) const {
  auto scalerContext = CGScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return Point::Zero();
  }
  return scalerContext->getVerticalOffset(glyphID);
}
}  // namespace tgfx
