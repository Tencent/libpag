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

#include <cmath>
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Element.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * Path represents a freeform shape defined by a PathData containing vertices, in-tangents, and
 * out-tangents.
 */
class Path : public Element {
 public:
  /**
   * The path data containing vertices and control points.
   */
  PathData* data = nullptr;

  /**
   * The position offset of the path coordinate system origin. The default value is (0, 0).
   */
  Point position = {};

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  /**
   * Distance from the left edge of the containing Layer or Group. NAN means not set.
   */
  float left = NAN;

  /**
   * Distance from the right edge of the containing Layer or Group. NAN means not set.
   */
  float right = NAN;

  /**
   * Distance from the top edge of the containing Layer or Group. NAN means not set.
   */
  float top = NAN;

  /**
   * Distance from the bottom edge of the containing Layer or Group. NAN means not set.
   */
  float bottom = NAN;

  /**
   * Horizontal offset from the center of the containing Layer or Group. NAN means not set.
   */
  float centerX = NAN;

  /**
   * Vertical offset from the center of the containing Layer or Group. NAN means not set.
   */
  float centerY = NAN;

  NodeType nodeType() const override {
    return NodeType::Path;
  }

 private:
  Path() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
