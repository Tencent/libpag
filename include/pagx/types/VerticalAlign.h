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
 * Text vertical alignment within the layout area. In horizontal mode, this controls vertical
 * positioning of lines. In vertical mode, Baseline and Top both position the first column with its
 * em box right edge touching the right side of the text area. The lineHeight multiplier only affects
 * spacing between columns, not the first column's position.
 */
enum class VerticalAlign {
  /**
   * The position.y of the text box represents the first line's baseline Y coordinate.
   * Text extends above (ascent) and below (descent) from this baseline.
   */
  Baseline,
  /**
   * Align text to the top of the layout area using the line-box model. The first line's line box
   * top edge is aligned to the top of the text area. The baseline is positioned at
   * halfLeading + ascent from the top, where halfLeading = (lineHeight - metricsHeight) / 2.
   */
  Top,
  /**
   * Vertically center all lines within the layout area. The total text block height is the sum of
   * all line heights (each line uses its effective lineHeight). This block is centered within the
   * box height.
   */
  Center,
  /**
   * Align the bottom of the last line's line box to the bottom edge of the layout area. The total
   * text block height is the sum of all line heights.
   */
  Bottom
};

}  // namespace pagx
