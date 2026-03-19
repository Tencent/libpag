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
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"

namespace pagx {

bool ElementConstraint::IsStretchable(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::TextBox;
}

Constraints ElementConstraint::GetConstraints(const Element* element) {
  Constraints c = {};
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto rect = static_cast<const Rectangle*>(element);
      c.left = rect->left;
      c.right = rect->right;
      c.top = rect->top;
      c.bottom = rect->bottom;
      c.centerX = rect->centerX;
      c.centerY = rect->centerY;
      break;
    }
    case NodeType::Ellipse: {
      auto ellipse = static_cast<const Ellipse*>(element);
      c.left = ellipse->left;
      c.right = ellipse->right;
      c.top = ellipse->top;
      c.bottom = ellipse->bottom;
      c.centerX = ellipse->centerX;
      c.centerY = ellipse->centerY;
      break;
    }
    case NodeType::Polystar: {
      auto polystar = static_cast<const Polystar*>(element);
      c.left = polystar->left;
      c.right = polystar->right;
      c.top = polystar->top;
      c.bottom = polystar->bottom;
      c.centerX = polystar->centerX;
      c.centerY = polystar->centerY;
      break;
    }
    case NodeType::Path: {
      auto path = static_cast<const Path*>(element);
      c.left = path->left;
      c.right = path->right;
      c.top = path->top;
      c.bottom = path->bottom;
      c.centerX = path->centerX;
      c.centerY = path->centerY;
      break;
    }
    case NodeType::Text: {
      auto text = static_cast<const Text*>(element);
      c.left = text->left;
      c.right = text->right;
      c.top = text->top;
      c.bottom = text->bottom;
      c.centerX = text->centerX;
      c.centerY = text->centerY;
      break;
    }
    case NodeType::TextBox: {
      auto textBox = static_cast<const TextBox*>(element);
      c.left = textBox->left;
      c.right = textBox->right;
      c.top = textBox->top;
      c.bottom = textBox->bottom;
      c.centerX = textBox->centerX;
      c.centerY = textBox->centerY;
      break;
    }
    case NodeType::Group: {
      auto group = static_cast<const Group*>(element);
      c.left = group->left;
      c.right = group->right;
      c.top = group->top;
      c.bottom = group->bottom;
      c.centerX = group->centerX;
      c.centerY = group->centerY;
      break;
    }
    default:
      break;
  }
  return c;
}

}  // namespace pagx
