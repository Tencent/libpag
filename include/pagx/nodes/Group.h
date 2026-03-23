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
#include <vector>
#include "pagx/nodes/Element.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * Group is a container that groups multiple vector elements with its own transform. It can contain
 * shapes, painters, modifiers, and nested groups, allowing for hierarchical organization of
 * content.
 */
class Group : public Element {
 public:
  /**
   * The anchor point for transformations.
   */
  Point anchor = {};

  /**
   * The position offset of the group.
   */
  Point position = {};

  /**
   * The rotation angle in degrees. The default value is 0.
   */
  float rotation = 0.0f;

  /**
   * The scale factor as (scaleX, scaleY). The default value is {1, 1}.
   */
  Point scale = {1.0f, 1.0f};

  /**
   * The skew angle in degrees. The default value is 0.
   */
  float skew = 0.0f;

  /**
   * The axis angle in degrees for the skew transformation. The default value is 0.
   */
  float skewAxis = 0.0f;

  /**
   * The opacity of the group, ranging from 0 to 1. The default value is 1.
   */
  float alpha = 1.0f;

  /**
   * The child elements contained in this group.
   */
  std::vector<Element*> elements = {};

  /**
   * The layout width of the group. When set, the group becomes a constraint positioning reference
   * frame for its child elements. Does not affect rendering behavior. NaN means not set.
   */
  float width = NAN;

  /**
   * The layout height of the group. When set, the group becomes a constraint positioning reference
   * frame for its child elements. Does not affect rendering behavior. NaN means not set.
   */
  float height = NAN;

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
    return NodeType::Group;
  }

 protected:
  Group() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
