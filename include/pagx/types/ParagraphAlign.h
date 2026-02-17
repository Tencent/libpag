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
 * Paragraph alignment within the layout area along the block-flow direction. In horizontal mode,
 * this controls vertical positioning of lines. In vertical mode, this controls horizontal
 * positioning of columns. The naming follows DirectWrite's DWRITE_PARAGRAPH_ALIGNMENT convention,
 * using direction-neutral names (Near/Far instead of Top/Bottom) that work correctly for both
 * horizontal and vertical writing modes. Baseline and Near both position the first column with its
 * em box right edge touching the right side of the text area. The lineHeight multiplier only
 * affects spacing between columns, not the first column's position.
 */
enum class ParagraphAlign {
  /**
   * The position.y of the text box represents the first line's baseline Y coordinate.
   * Text extends above (ascent) and below (descent) from this baseline.
   */
  Baseline,
  /**
   * Align text to the near edge of the layout area (top in horizontal mode, right in vertical
   * mode) using the line-box model. The first line's line box near edge is aligned to the near
   * edge of the text area. The baseline is positioned at halfLeading + ascent from the near edge,
   * where halfLeading = (lineHeight - metricsHeight) / 2.
   */
  Near,
  /**
   * Center all lines within the layout area along the block-flow direction. The total text block
   * size (sum of all line heights/column widths) is centered within the corresponding box
   * dimension.
   */
  Center,
  /**
   * Align the last line's line box far edge to the far edge of the layout area (bottom in
   * horizontal mode, left in vertical mode). The total text block size is the sum of all line
   * heights/column widths.
   */
  Far
};

}  // namespace pagx
