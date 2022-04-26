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

#include "tgfx/core/FontMetrics.h"
#include "tgfx/core/Path.h"
#include "tgfx/gpu/TextureBuffer.h"

namespace tgfx {
/**
 * 16 bit unsigned integer to hold a glyph index
 */
typedef uint16_t GlyphID;

/**
 * A set of character glyphs and layout information for drawing text.
 */
class Typeface {
 public:
  /**
   * Returns the default typeface reference, which is never nullptr.
   */
  static std::shared_ptr<Typeface> MakeDefault();

  /**
   * Returns a typeface reference for the given font family and font style. If the family and
   * style cannot be matched identically, a best match is found and returned, which is never
   * nullptr.
   */
  static std::shared_ptr<Typeface> MakeFromName(const std::string& fontFamily,
                                                const std::string& fontStyle);

  /**
   * Creates a new typeface for the given file path and ttc index. Returns nullptr if the typeface
   * can't be created.
   */
  static std::shared_ptr<Typeface> MakeFromPath(const std::string& fontPath, int ttcIndex = 0);

  /**
   * Creates a new typeface for the given file bytes and ttc index. Returns nullptr if the typeface
   * can't be created.
   */
  static std::shared_ptr<Typeface> MakeFromBytes(const void* data, size_t length, int ttcIndex = 0);

  virtual ~Typeface() = default;

  /**
   * Returns the uniqueID for the specified typeface.
   */
  virtual uint32_t uniqueID() const = 0;

  /**
   * Returns the family name of this typeface.
   */
  virtual std::string fontFamily() const = 0;

  /**
   * Returns the style name of this typeface.
   */
  virtual std::string fontStyle() const = 0;

  /**
   *  Return the number of glyphs in this typeface.
   */
  virtual int glyphsCount() const = 0;

  /**
   * Returns the number of glyph space units per em for this typeface.
   */
  virtual int unitsPerEm() const = 0;

  /**
   * Returns true if this typeface has color glyphs, for example, color emojis.
   */
  virtual bool hasColor() const = 0;

  /**
   * Returns the glyph ID corresponds to the specified glyph name. The glyph name must be in utf-8
   * encoding. Returns 0 if the glyph name is not associated with this typeface.
   */
  virtual GlyphID getGlyphID(const std::string& name) const = 0;

 protected:
  /**
   * Returns the FontMetrics associated with this typeface.
   */
  virtual FontMetrics getMetrics(float size) const = 0;

  /**
   * Returns the bounding box of the specified glyph. The bounds is specified in glyph space
   * units.
   */
  virtual Rect getGlyphBounds(GlyphID glyphID, float size, bool fauxBold,
                              bool fauxItalic) const = 0;

  /**
   * Returns the advance for specified glyph. The value is specified in glyph space units.
   * @param glyphID The id of specified glyph.
   * @param vertical The intended drawing orientation of the glyph.
   */
  virtual float getGlyphAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                                bool verticalText) const = 0;

  /**
   * Creates a path corresponding to glyph outline. If glyph has an outline, copies outline to path
   * and returns true. If glyph is described by a bitmap, returns false and ignores path parameter.
   * The points in path are specified in glyph space units.
   */
  virtual bool getGlyphPath(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                            Path* path) const = 0;

  /**
   * Creates a texture buffer capturing the content of the specified glyph. The returned matrix
   * should apply to the glyph image when drawing.
   */
  virtual std::shared_ptr<TextureBuffer> getGlyphImage(GlyphID glyphID, float size, bool fauxBold,
                                                       bool fauxItalic, Matrix* matrix) const = 0;

  /**
   * Calculates the offset from the default (horizontal) origin to the vertical origin for specified
   * glyph. The offset is specified in glyph space units.
   */
  virtual Point getGlyphVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                                       bool fauxItalic) const = 0;

  friend class Font;
};
}  // namespace tgfx
