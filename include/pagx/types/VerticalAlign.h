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

/**
 * Text vertical alignment within the layout area.
 */
enum class VerticalAlign {
  /**
   * The position.y of the text box represents the first line's baseline Y coordinate.
   * Text extends above (ascent) and below (descent) from this baseline.
   */
  Baseline,
  /**
   * Align text to the top of the layout area. The first line's baseline is positioned at the top
   * edge plus the maximum ascent of glyphs in that line, so that the tallest glyph's ascent touches
   * the top of the text area.
   */
  Top,
  /**
   * Vertically center all lines within the layout area. The total text block height is measured from
   * the first line's ascent top to the last line's descent bottom, including line height spacing
   * between lines. This block is centered within the box height.
   */
  Center,
  /**
   * Align the bottom of the last line's descent to the bottom edge of the layout area. The total
   * text block height is calculated the same way as Center.
   */
  Bottom
};

}  // namespace pagx
