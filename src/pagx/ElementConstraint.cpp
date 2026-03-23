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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/layout/ElementConstraint.h"
#include "pagx/nodes/LayoutElement.h"

namespace pagx {

static bool IsLayoutElement(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Polystar ||
         type == NodeType::Path || type == NodeType::Text || type == NodeType::TextBox ||
         type == NodeType::Group || type == NodeType::TextPath;
}

bool ElementConstraint::IsStretchable(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::TextBox;
}

Constraints ElementConstraint::GetConstraints(const Element* element) {
  if (!IsLayoutElement(element->nodeType())) {
    return {};
  }
  auto* layoutElement = static_cast<const LayoutElement*>(element);
  Constraints c = {};
  c.left = layoutElement->left;
  c.right = layoutElement->right;
  c.top = layoutElement->top;
  c.bottom = layoutElement->bottom;
  c.centerX = layoutElement->centerX;
  c.centerY = layoutElement->centerY;
  return c;
}

}  // namespace pagx
