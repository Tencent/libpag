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

#include <emscripten/val.h>
#include <unordered_map>
#include <vector>
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace tgfx {
class WebTypeface : public Typeface {
 public:
  static std::shared_ptr<WebTypeface> Make(const std::string& name, const std::string& style = "");

  ~WebTypeface() override;

  uint32_t uniqueID() const override {
    return _uniqueID;
  }

  std::string fontFamily() const override {
    return name;
  }

  std::string fontStyle() const override {
    return style;
  }

  int glyphsCount() const override {
    return 0;
  }

  int unitsPerEm() const override {
    return 0;
  }

  bool hasColor() const override;

  std::string getText(GlyphID glyphID) const;

  GlyphID getGlyphID(const std::string&) const override;

 protected:
  Point getGlyphVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                               bool fauxItalic) const override;

  float getGlyphAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                        bool verticalText) const override;

  FontMetrics getMetrics(float size) const override;

  bool getGlyphPath(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                    Path* path) const override;
  Rect getGlyphBounds(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic) const override;

  std::shared_ptr<TextureBuffer> getGlyphImage(GlyphID glyphID, float size, bool fauxBold,
                                               bool fauxItalic, Matrix* matrix) const override;

 private:
  explicit WebTypeface(std::string name, std::string style);

  uint32_t _uniqueID;
  std::unordered_map<float, FontMetrics>* fontMetricsMap =
      new std::unordered_map<float, FontMetrics>;
  emscripten::val scalerContextClass = emscripten::val::null();
  std::string name;
  std::string style;
  std::string webFontFamily;
};
}  // namespace tgfx
