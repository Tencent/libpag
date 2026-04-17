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

#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Element.h"
#include "pagx/types/Point.h"
#include "pagx/types/Rect.h"
#include "pagx/types/Size.h"

namespace pagx {

/**
 * Rectangle represents a rectangle shape with optional rounded corners.
 */
class Rectangle : public Element, public LayoutNode {
 public:
  /**
   * The center point of the rectangle. Animatable. May be overridden by constraint attributes
   * during layout. NaN means not explicitly set, defaults to (size.width/2, size.height/2).
   */
  Point position = {NAN, NAN};

  /**
   * The size of the rectangle. The default value is {0, 0}.
   */
  Size size = {0.0f, 0.0f};

  /**
   * The corner roundness of the rectangle in pixels. The default value is 0.
   */
  float roundness = 0.0f;

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  NodeType nodeType() const override {
    return NodeType::Rectangle;
  }

  /**
   * Returns the final center position for rendering, computed from layoutBounds.
   */
  Point renderPosition() const;

  /**
   * Returns the final size for rendering, computed from layoutBounds.
   */
  Size renderSize() const;

 protected:
  void onMeasure(LayoutContext* context) override;
  void setLayoutSize(LayoutContext* context, float width, float height) override;

 private:
  Rectangle() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
