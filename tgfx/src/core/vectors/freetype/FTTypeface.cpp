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

#include "FTTypeface.h"

#include FT_TRUETYPE_TABLES_H

#include "FTScalerContext.h"
#include "SystemFont.h"
#include "core/utils/UniqueID.h"
#include "tgfx/core/UTF.h"

namespace tgfx {
class EmptyTypeface : public Typeface {
 public:
  uint32_t uniqueID() const override {
    return _uniqueID;
  }

  std::string fontFamily() const override {
    return "";
  }

  std::string fontStyle() const override {
    return "";
  }

  int glyphsCount() const override {
    return 0;
  }

  int unitsPerEm() const override {
    return 0;
  }

  bool hasColor() const override {
    return false;
  }

  GlyphID getGlyphID(const std::string&) const override {
    return 0;
  }

 protected:
  FontMetrics getMetrics(float) const override {
    return {};
  }

  Rect getGlyphBounds(GlyphID, float, bool, bool) const override {
    return {};
  }

  float getGlyphAdvance(GlyphID, float, bool, bool, bool) const override {
    return 0;
  }

  bool getGlyphPath(GlyphID, float, bool, bool, Path*) const override {
    return false;
  }

  std::shared_ptr<TextureBuffer> getGlyphImage(GlyphID, float, bool, bool, Matrix*) const override {
    return nullptr;
  }

  Point getGlyphVerticalOffset(GlyphID, float, bool, bool) const override {
    return Point::Zero();
  }

 private:
  uint32_t _uniqueID = UniqueID::Next();
};

std::shared_ptr<Typeface> Typeface::MakeFromName(const std::string& fontFamily,
                                                 const std::string& fontStyle) {
  return SystemFont::MakeFromName(fontFamily, fontStyle);
}

std::shared_ptr<Typeface> Typeface::MakeFromPath(const std::string& fontPath, int ttcIndex) {
  return FTTypeface::Make(FTFontData(fontPath, ttcIndex));
}

std::shared_ptr<Typeface> Typeface::MakeFromBytes(const void* data, size_t length, int ttcIndex) {
  return FTTypeface::Make(FTFontData(data, length, ttcIndex));
}

std::shared_ptr<Typeface> Typeface::MakeDefault() {
  return std::make_shared<EmptyTypeface>();
}

std::shared_ptr<FTTypeface> FTTypeface::Make(FTFontData data) {
  auto face = FTFace::Make(data);
  if (face == nullptr) {
    return nullptr;
  }
  auto typeface = std::shared_ptr<FTTypeface>(new FTTypeface(std::move(data), std::move(face)));
  typeface->weakThis = typeface;
  return typeface;
}

FTTypeface::FTTypeface(FTFontData data, std::unique_ptr<FTFace> face)
    : _uniqueID(UniqueID::Next()), data(std::move(data)), _face(std::move(face)) {
}

FTTypeface::~FTTypeface() {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  _face = nullptr;
}

int FTTypeface::GetUnitsPerEm(FT_Face face) {
  auto unitsPerEm = face->units_per_EM;
  // At least some versions of FreeType set face->units_per_EM to 0 for bitmap only fonts.
  if (unitsPerEm == 0) {
    auto* ttHeader = static_cast<TT_Header*>(FT_Get_Sfnt_Table(face, FT_SFNT_HEAD));
    if (ttHeader) {
      unitsPerEm = ttHeader->Units_Per_EM;
    }
  }
  return unitsPerEm;
}

int FTTypeface::glyphsCount() const {
  return static_cast<int>(_face->face->num_glyphs);
}

int FTTypeface::unitsPerEm() const {
  return GetUnitsPerEm(_face->face);
}

bool FTTypeface::hasColor() const {
  return FT_HAS_COLOR(_face->face);
}

GlyphID FTTypeface::getGlyphID(const std::string& name) const {
  if (name.empty()) {
    return 0;
  }
  auto count = tgfx::UTF::CountUTF8(name.c_str(), name.size());
  if (count > 1 || count <= 0) {
    return 0;
  }
  const char* start = &(name[0]);
  auto unichar = tgfx::UTF::NextUTF8(&start, start + name.size());
  return FT_Get_Char_Index(_face->face, static_cast<FT_ULong>(unichar));
}

FontMetrics FTTypeface::getMetrics(float size) const {
  auto scalerContext = FTScalerContext::Make(weakThis.lock(), size);
  if (scalerContext == nullptr) {
    return {};
  }
  return scalerContext->generateFontMetrics();
}

Rect FTTypeface::getGlyphBounds(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic) const {
  auto scalerContext = FTScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return Rect::MakeEmpty();
  }
  auto glyphMetrics = scalerContext->generateGlyphMetrics(glyphID);
  auto bounds =
      Rect::MakeXYWH(glyphMetrics.left, glyphMetrics.top, glyphMetrics.width, glyphMetrics.height);
  auto advance = glyphMetrics.advanceX;
  if (bounds.isEmpty() && advance > 0) {
    auto fontMetrics = scalerContext->generateFontMetrics();
    bounds.setLTRB(0, fontMetrics.ascent, advance, fontMetrics.descent);
  }
  return bounds;
}

float FTTypeface::getGlyphAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                                  bool verticalText) const {
  auto scalerContext =
      FTScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic, verticalText);
  if (scalerContext == nullptr) {
    return 0;
  }
  auto glyphMetrics = scalerContext->generateGlyphMetrics(glyphID);
  return verticalText ? glyphMetrics.advanceY : glyphMetrics.advanceX;
}

bool FTTypeface::getGlyphPath(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                              Path* path) const {
  auto scalerContext = FTScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return false;
  }
  return scalerContext->generatePath(glyphID, path);
}

std::shared_ptr<TextureBuffer> FTTypeface::getGlyphImage(GlyphID glyphID, float size, bool fauxBold,
                                                         bool fauxItalic, Matrix* matrix) const {
  auto scalerContext = FTScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return nullptr;
  }
  return scalerContext->generateImage(glyphID, matrix);
}

Point FTTypeface::getGlyphVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                                         bool fauxItalic) const {
  auto scalerContext = FTScalerContext::Make(weakThis.lock(), size, fauxBold, fauxItalic);
  if (scalerContext == nullptr) {
    return Point::Zero();
  }
  auto metrics = scalerContext->generateFontMetrics();
  auto offsetY = metrics.capHeight;
  auto glyphMetrics = scalerContext->generateGlyphMetrics(glyphID);
  return {-glyphMetrics.advanceX * 0.5f, offsetY};
}
}  // namespace tgfx
