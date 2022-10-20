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

#include <vector>
#include "tgfx/core/Typeface.h"

namespace pag {
class PositionedGlyphs {
 public:
  PositionedGlyphs() = default;

  explicit PositionedGlyphs(
      std::vector<std::tuple<std::shared_ptr<tgfx::Typeface>, tgfx::GlyphID, uint32_t>> glyphs)
      : glyphs(std::move(glyphs)) {
  }

  std::shared_ptr<tgfx::Typeface> getTypeface(size_t atIndex) const {
    return std::get<0>(glyphs[atIndex]);
  }

  tgfx::GlyphID getGlyphID(size_t atIndex) const {
    return std::get<1>(glyphs[atIndex]);
  }

  uint32_t getStringIndex(size_t atIndex) const {
    return std::get<2>(glyphs[atIndex]);
  }

  size_t glyphCount() const {
    return glyphs.size();
  }

 private:
  std::vector<std::tuple<std::shared_ptr<tgfx::Typeface>, tgfx::GlyphID, uint32_t>> glyphs;
};
}  // namespace pag
