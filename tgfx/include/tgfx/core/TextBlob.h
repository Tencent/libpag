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
   * Returns the bounds of the text blob's glyphs.
   */
  Rect getBounds(const Stroke* stroke = nullptr) const;

  /**
   * Creates a path corresponding to glyph shapes in the text blob. Copies the glyph fills to the
   * path if the stroke is not passed in, otherwise copies the glyph outlines to the path. If text
   * font is backed by bitmaps or cannot generate outlines, returns false and leaves the path
   * unchanged.
   */
  bool getPath(Path* path, const Stroke* stroke = nullptr) const;

  /**
   * Creates a texture buffer capturing the pixels in the text blob.
   * @param resolutionScale The "intended" resolution for the output. Larger values (scale > 1)
   * indicate that the result should be more precise, smaller values (0 < scale < 1) indicate that
   * the result can be less precise.
   * @param matrix The output transformation that should apply to the texture when drawing.
   * @return An alpha only texture buffer is returned if the text font is not backed by bitmaps.
   */
  virtual std::shared_ptr<TextureBuffer> getImage(float resolutionScale, Matrix* matrix) const = 0;

 protected:
  Font font = {};
  std::vector<GlyphID> glyphIDs = {};
  std::vector<Point> positions = {};

  friend class Mask;
};
}  // namespace tgfx
