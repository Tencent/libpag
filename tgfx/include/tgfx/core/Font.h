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

#include "tgfx/core/Typeface.h"

namespace tgfx {
/**
 * Font controls options applied when drawing and measuring text.
 */
class Font {
 public:
  /**
   * Constructs Font with default values.
   */
  Font();

  /**
   * Constructs Font with default values with Typeface and size in points.
   */
  explicit Font(std::shared_ptr<Typeface> typeface, float size = 12.0f);

  /**
   * Returns a new font with the same attributes of this font, but with the specified size.
   */
  Font makeWithSize(float size) const;

  /**
   * Returns a typeface reference if set, or the default typeface reference, which is never nullptr.
   */
  std::shared_ptr<Typeface> getTypeface() const {
    return typeface;
  }

  /**
   * Sets a new Typeface to this Font.
   */
  void setTypeface(std::shared_ptr<Typeface> newTypeface);

  /**
   * Returns the point size of this font.
   */
  float getSize() const {
    return size;
  }

  /**
   * Sets text size in points. Has no effect if textSize is not greater than or equal to zero.
   */
  void setSize(float newSize);

  /**
   * Returns true if bold is approximated by increasing the stroke width when drawing glyphs.
   */
  bool isFauxBold() const {
    return fauxBold;
  }

  /**
   * Increases stroke width when drawing glyphs to approximate a bold typeface.
   */
  void setFauxBold(bool value) {
    fauxBold = value;
  }

  /**
   * Returns true if italic is approximated by adding skewX value of a canvas's matrix when
   * drawing glyphs.
   */
  bool isFauxItalic() const {
    return fauxItalic;
  }

  /**
   * Adds skewX value of a canvas's matrix when drawing glyphs to approximate a italic typeface.
   */
  void setFauxItalic(bool value) {
    fauxItalic = value;
  }

  /**
   * Returns the FontMetrics associated with this font.
   */
  FontMetrics getMetrics() const {
    return typeface->getMetrics(size);
  }

  /**
   * Returns the glyph ID corresponds to the specified glyph name. The glyph name must be in utf-8
   * encoding. Returns 0 if the glyph name is not associated with this typeface.
   */
  GlyphID getGlyphID(const std::string& name) const {
    return typeface->getGlyphID(name);
  }

  /**
   * Returns the bounding box of the specified glyph.
   */
  Rect getGlyphBounds(GlyphID glyphID) const {
    return typeface->getGlyphBounds(glyphID, size, fauxBold, fauxItalic);
  }

  /**
   * Returns the advance for specified glyph.
   * @param glyphID The id of specified glyph.
   * @param verticalText The intended drawing orientation of the glyph.
   */
  float getGlyphAdvance(GlyphID glyphID, bool verticalText = false) const {
    return typeface->getGlyphAdvance(glyphID, size, fauxBold, fauxItalic, verticalText);
  }

  /**
   * Creates a path corresponding to glyph outline. If glyph has an outline, copies outline to path
   * and returns true. If glyph is described by a bitmap, returns false and ignores path parameter.
   */
  bool getGlyphPath(GlyphID glyphID, Path* path) const {
    return typeface->getGlyphPath(glyphID, size, fauxBold, fauxItalic, path);
  }

  /**
   * Creates a texture buffer capturing the content of the specified glyph. The returned matrix
   * should apply to the glyph image when drawing.
   */
  std::shared_ptr<TextureBuffer> getGlyphImage(GlyphID glyphID, Matrix* matrix) const {
    return typeface->getGlyphImage(glyphID, size, fauxBold, fauxItalic, matrix);
  }

  /**
   * Calculates the offset from the default (horizontal) origin to the vertical origin for specified
   * glyph.
   */
  Point getGlyphVerticalOffset(GlyphID glyphID) const {
    return typeface->getGlyphVerticalOffset(glyphID, size, fauxBold, fauxItalic);
  }

 private:
  std::shared_ptr<Typeface> typeface = nullptr;
  float size = 12.0f;
  bool fauxBold = false;
  bool fauxItalic = false;
};
}  // namespace tgfx