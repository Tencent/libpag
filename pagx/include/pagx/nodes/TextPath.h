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

#include <string>
#include "pagx/nodes/Element.h"
#include "pagx/nodes/TextAlign.h"

namespace pagx {

/**
 * TextPath is a text modifier that places text along a path. It allows text to follow the contour
 * of a referenced path shape.
 */
class TextPath : public Element {
 public:
  /**
   * A reference to the path shape (e.g., "#pathId") that the text follows.
   */
  std::string path = {};

  /**
   * The alignment of text along the path. The default value is Start.
   * - Start: Align text to the path start.
   * - Center: Center text along the path.
   * - End: Align text to the path end.
   * - Justify: Force text to fill the path by adjusting letter spacing.
   */
  TextAlign textAlign = TextAlign::Start;

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
  bool perpendicularToPath = true;

  /**
   * Whether to reverse the direction of the path. The default value is false.
   */
  bool reversed = false;

  NodeType nodeType() const override {
    return NodeType::TextPath;
  }
};

}  // namespace pagx
