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

#include "pagx/layout/ElementTransform.h"
#include <cmath>
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"

namespace pagx {

void ElementTransform::TranslateX(Element* element, float tx) {
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      static_cast<Rectangle*>(element)->position.x += tx;
      break;
    case NodeType::Ellipse:
      static_cast<Ellipse*>(element)->position.x += tx;
      break;
    case NodeType::Polystar:
      static_cast<Polystar*>(element)->position.x += tx;
      break;
    case NodeType::Path:
      static_cast<Path*>(element)->position.x += tx;
      break;
    case NodeType::TextPath: {
      auto* textPath = static_cast<TextPath*>(element);
      if (textPath->path != nullptr) {
        textPath->path->translatePoints(tx, 0);
      }
      textPath->baselineOrigin.x += tx;
      break;
    }
    case NodeType::Text:
      static_cast<Text*>(element)->position.x += tx;
      break;
    case NodeType::TextBox:
      static_cast<TextBox*>(element)->position.x += tx;
      break;
    case NodeType::Group:
      static_cast<Group*>(element)->position.x += tx;
      break;
    default:
      break;
  }
}

void ElementTransform::TranslateY(Element* element, float ty) {
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      static_cast<Rectangle*>(element)->position.y += ty;
      break;
    case NodeType::Ellipse:
      static_cast<Ellipse*>(element)->position.y += ty;
      break;
    case NodeType::Polystar:
      static_cast<Polystar*>(element)->position.y += ty;
      break;
    case NodeType::Path:
      static_cast<Path*>(element)->position.y += ty;
      break;
    case NodeType::TextPath: {
      auto* textPath = static_cast<TextPath*>(element);
      if (textPath->path != nullptr) {
        textPath->path->translatePoints(0, ty);
      }
      textPath->baselineOrigin.y += ty;
      break;
    }
    case NodeType::Text:
      static_cast<Text*>(element)->position.y += ty;
      break;
    case NodeType::TextBox:
      static_cast<TextBox*>(element)->position.y += ty;
      break;
    case NodeType::Group:
      static_cast<Group*>(element)->position.y += ty;
      break;
    default:
      break;
  }
}

void ElementTransform::ApplyHorizontalStretch(Element* element, float newWidth, float newCenterX) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto* rect = static_cast<Rectangle*>(element);
      rect->size.width = newWidth;
      rect->position.x = newCenterX;
      break;
    }
    case NodeType::Ellipse: {
      auto* ellipse = static_cast<Ellipse*>(element);
      ellipse->size.width = newWidth;
      ellipse->position.x = newCenterX;
      break;
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<TextBox*>(element);
      textBox->width = newWidth;
      textBox->position.x = newCenterX - newWidth * 0.5f;
      break;
    }
    default:
      break;
  }
}

void ElementTransform::ApplyVerticalStretch(Element* element, float newHeight, float newCenterY) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto* rect = static_cast<Rectangle*>(element);
      rect->size.height = newHeight;
      rect->position.y = newCenterY;
      break;
    }
    case NodeType::Ellipse: {
      auto* ellipse = static_cast<Ellipse*>(element);
      ellipse->size.height = newHeight;
      ellipse->position.y = newCenterY;
      break;
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<TextBox*>(element);
      textBox->height = newHeight;
      textBox->position.y = newCenterY - newHeight * 0.5f;
      break;
    }
    default:
      break;
  }
}

void ElementTransform::ApplyScaling(Element* element, float scale) {
  switch (element->nodeType()) {
    case NodeType::Polystar: {
      auto* polystar = static_cast<Polystar*>(element);
      polystar->outerRadius *= scale;
      polystar->innerRadius *= scale;
      break;
    }
    case NodeType::Path: {
      auto* path = static_cast<Path*>(element);
      if (path->data != nullptr) {
        path->data->scalePoints(scale);
      }
      break;
    }
    case NodeType::TextPath: {
      auto* textPath = static_cast<TextPath*>(element);
      if (textPath->path != nullptr) {
        textPath->path->scalePoints(scale);
      }
      textPath->baselineOrigin.x *= scale;
      textPath->baselineOrigin.y *= scale;
      break;
    }
    case NodeType::Text: {
      auto* text = static_cast<Text*>(element);
      text->fontSize *= scale;
      break;
    }
    default:
      break;
  }
}

void ElementTransform::UpdateGroupLayoutSize(Group* group, float derivedWidth,
                                             float derivedHeight) {
  if (!std::isnan(derivedWidth) && std::isnan(group->width)) {
    group->width = derivedWidth;
  }
  if (!std::isnan(derivedHeight) && std::isnan(group->height)) {
    group->height = derivedHeight;
  }
}

}  // namespace pagx
