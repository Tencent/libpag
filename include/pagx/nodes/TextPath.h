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

#include <string>
#include "pagx/nodes/Element.h"
#include "pagx/nodes/PathData.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * TextPath is a text modifier that places text along a path. It allows text to follow the contour
 * of a path shape. The path can be specified either inline or by referencing a PathData resource.
 */
class TextPath : public Element {
 public:
  /**
   * The path data that the text follows.
   */
  PathData* path = nullptr;

  /**
   * The origin point of the baseline in the TextPath's local coordinate space. The baseline is a
   * straight line starting from this origin at the angle specified by baselineAngle. Each glyph's
   * distance along the baseline determines where it lands on the curve, and its perpendicular offset
   * from the baseline is preserved as a perpendicular offset from the curve. Default is (0, 0).
   */
  Point baselineOrigin = {};

  /**
   * The angle of the baseline in degrees. 0 means a horizontal baseline (text flows left to right
   * along the X axis), 90 means a vertical baseline (text flows top to bottom along the Y axis).
   * The default value is 0.
   */
  float baselineAngle = 0;

  /**
   * The margin from the start of the path in pixels. The default value is 0.
   */
  float firstMargin = 0;

  /**
   * The margin from the end of the path in pixels. The default value is 0.
   */
  float lastMargin = 0;

  /**
   * Whether characters are rotated to be perpendicular to the path. The default value is true.
   */
  bool perpendicular = true;

  /**
   * Whether to reverse the direction of the path. The default value is false.
   */
  bool reversed = false;

  /**
   * Whether text is stretched to fit the available path length. When enabled, glyphs are laid out
   * consecutively using their advance widths, then spacing is adjusted proportionally to fill the
   * path region between firstMargin and lastMargin. The default value is false.
   */
  bool forceAlignment = false;

  NodeType nodeType() const override {
    return NodeType::TextPath;
  }

 private:
  TextPath() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
