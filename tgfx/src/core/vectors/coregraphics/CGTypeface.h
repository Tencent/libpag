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

#pragma once

#include <CoreText/CoreText.h>
#include "tgfx/core/Typeface.h"

namespace tgfx {
class CGTypeface : public Typeface {
 public:
  static std::shared_ptr<CGTypeface> Make(CTFontRef ctFont);

  ~CGTypeface() override;

  uint32_t uniqueID() const override {
    return _uniqueID;
  }

  std::string fontFamily() const override;

  std::string fontStyle() const override;

  int glyphsCount() const override {
    return static_cast<int>(CTFontGetGlyphCount(ctFont));
  }

  int unitsPerEm() const override;

  bool hasColor() const override;

  GlyphID getGlyphID(const std::string& name) const override;

 protected:
  Rect getGlyphBounds(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic) const override;

  FontMetrics getMetrics(float size) const override;

  Point getGlyphVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                               bool fauxItalic) const override;

  bool getGlyphPath(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                    Path* path) const override;

  std::shared_ptr<TextureBuffer> getGlyphImage(GlyphID glyphID, float size, bool fauxBold,
                                               bool fauxItalic, Matrix* matrix) const override;

  float getGlyphAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                        bool verticalText) const override;

 private:
  explicit CGTypeface(CTFontRef ctFont);

  uint32_t _uniqueID = 0;
  CTFontRef ctFont = nullptr;
  std::weak_ptr<CGTypeface> weakThis;

  friend class CGScalerContext;
};
}  // namespace tgfx
