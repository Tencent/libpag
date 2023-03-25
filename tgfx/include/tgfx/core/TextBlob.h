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

#include "tgfx/core/Font.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Stroke.h"

namespace tgfx {
/**
 * TextBlob combines multiple glyphs, Font, and positions into an immutable container.
 */
class TextBlob {
 public:
  /**
   * Creates a new TextBlob from the given glyphs, positions and text font.
   */
  static std::shared_ptr<TextBlob> MakeFrom(const GlyphID glyphIDs[], const Point positions[],
                                            size_t glyphCount, const Font& font);

  virtual ~TextBlob() = default;

  /**
   * Returns true if this TextBlob has color glyphs, for example, color emojis.
   */
  virtual bool hasColor() const = 0;

  /**
   * Returns the bounds of the text blob's glyphs.
   */
  virtual Rect getBounds(const Stroke* stroke = nullptr) const = 0;

  /**
   * Creates a path corresponding to glyph shapes in the text blob. Copies the glyph fills to the
   * path if the stroke is not passed in, otherwise copies the glyph outlines to the path. If text
   * font is backed by bitmaps or cannot generate outlines, returns false and leaves the path
   * unchanged.
   */
  virtual bool getPath(Path* path, const Stroke* stroke = nullptr) const = 0;
};
}  // namespace tgfx
