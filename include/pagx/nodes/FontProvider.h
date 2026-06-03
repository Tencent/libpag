/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

namespace pagx {

class PathData;

/**
 * FontProvider describes a custom font supplied by business code at document-load time.
 *
 * The provider deliberately exposes PAGX types only. It mirrors the idea of tgfx custom typefaces
 * without exposing tgfx classes in the public PAGX API. Full TextLayout integration can consume
 * this provider in a later step; loader owns any cache needed to keep shared providers alive.
 */
class FontProvider {
 public:
  virtual ~FontProvider() = default;

  /**
   * Returns the font design-space units per em.
   */
  virtual int unitsPerEm() const = 0;

  /**
   * Returns the number of glyphs exposed by this provider.
   */
  virtual int glyphCount() const = 0;

  /**
   * Returns the advance width of the glyph at glyphIndex in design-space units.
   */
  virtual float glyphAdvance(int glyphIndex) const = 0;

  /**
   * Writes the vector outline for glyphIndex into path. Returns false if the glyph has no vector
   * path or glyphIndex is invalid. The PathData pointer is owned by caller and must not be stored.
   */
  virtual bool getGlyphPath(int glyphIndex, PathData* path) const = 0;
};

}  // namespace pagx
