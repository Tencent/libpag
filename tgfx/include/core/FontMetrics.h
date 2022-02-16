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

namespace tgfx {
class FontMetrics {
 public:
  /**
   * Extent above baseline.
   */
  float top = 0;
  /**
   * Distance to reserve above baseline.
   */
  float ascent = 0;
  /**
   * Distance to reserve below baseline.
   */
  float descent = 0;
  /**
   * Extent below baseline.
   */
  float bottom = 0;
  /**
   * Distance to add between lines.
   */
  float leading = 0;
  /**
   * Minimum x.
   */
  float xMin = 0;
  /**
   * Maximum x.
   */
  float xMax = 0;
  /**
   * Height of lower-case 'x'.
   */
  float xHeight = 0;
  /**
   * Height of an upper-case letter.
   */
  float capHeight = 0;
  /**
   * Underline thickness.
   */
  float underlineThickness = 0;
  /**
   * Underline position relative to baseline.
   */
  float underlinePosition = 0;
};

struct GlyphMetrics {
  float width = 0;
  float height = 0;

  // The offset from the glyphs' origin on the baseline to the top left of the glyph mask.
  float top = 0;
  float left = 0;

  // The advance for this glyph.
  float advanceX = 0;
  float advanceY = 0;
};
}  // namespace tgfx
