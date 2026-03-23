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

#include "ConstraintLayout.h"
#include <cmath>
#include "TextLayout.h"
#include "base/utils/Log.h"
#include "pagx/FontProvider.h"
#include "pagx/layout/ElementConstraint.h"
#include "pagx/layout/ElementMeasure.h"
#include "pagx/layout/ElementTransform.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Constraints.h"
#include "pagx/types/Rect.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

namespace {

// Computes text bounds with anchor offset (inlined from AutoLayout.cpp).
// Kept here for self-contained constraint application.
Rect ComputeTextBounds(const Text* text, FontConfig* fontProvider) {
  if (text == nullptr || text->text.empty()) {
    return {};
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  if (fontProvider != nullptr) {
    typeface = fontProvider->findTypeface(text->fontFamily, text->fontStyle);
  }
  if (typeface == nullptr) {
    typeface = tgfx::Typeface::MakeFromName(text->fontFamily, text->fontStyle);
  }

  float textWidth;
  float textHeight;
  float boundsTop;

  if (typeface != nullptr) {
    tgfx::Font font(typeface, text->fontSize);
    font.setFauxBold(text->fauxBold);
    font.setFauxItalic(text->fauxItalic);
    auto textBlob = tgfx::TextBlob::MakeFrom(text->text, font);
    if (textBlob != nullptr) {
      auto bounds = textBlob->getTightBounds();
      textWidth = bounds.right - bounds.left;
      textHeight = bounds.bottom - bounds.top;
      boundsTop = bounds.top;
    } else {
      textWidth = static_cast<float>(text->text.size()) * text->fontSize * 0.6f;
      textHeight = text->fontSize;
      boundsTop = -text->fontSize * 0.8f;
    }
  } else {
    textWidth = static_cast<float>(text->text.size()) * text->fontSize * 0.6f;
    textHeight = text->fontSize;
    boundsTop = -text->fontSize * 0.8f;
  }

  float boundsX = text->position.x;
  float boundsY = text->position.y + boundsTop;
  if (text->textAnchor == TextAnchor::Center) {
    boundsX = text->position.x - textWidth * 0.5f;
  } else if (text->textAnchor == TextAnchor::End) {
    boundsX = text->position.x - textWidth;
  }
  return Rect::MakeXYWH(boundsX, boundsY, textWidth, textHeight);
}

// Wraps ElementMeasure::GetContentBounds with TextBox measurement write-back.
// For TextBox elements without explicit dimensions, automatically measures and sets width/height.
Rect GetContentBounds(const Element* element, FontConfig* fontProvider) {
  if (element->nodeType() == NodeType::TextBox) {
    auto* textBox = const_cast<TextBox*>(static_cast<const TextBox*>(element));
    bool needW = std::isnan(textBox->width);
    bool needH = std::isnan(textBox->height);
    if (needW || needH) {
      auto measured = TextLayout::MeasureTextBox(textBox, fontProvider);
      if (needW) {
        textBox->width = measured.width;
      }
      if (needH) {
        textBox->height = measured.height;
      }
    }
    return Rect::MakeXYWH(0, 0, textBox->width, textBox->height);
  }
  return ElementMeasure::GetContentBounds(
      element, [fontProvider](const Text* text) { return ComputeTextBounds(text, fontProvider); });
}

}  // namespace

void ConstraintLayout::ApplyElementConstraints(Element* element, float containerWidth,
                                               float containerHeight, FontConfig* fontProvider,
                                               bool skipScaling) {
  auto constraints = ElementConstraint::GetConstraints(element);
  if (!constraints.hasAny()) {
    return;
  }
  std::string errorMessage;
  if (!constraints.isValid(errorMessage)) {
    LOGE("[Layout Warning] %s", errorMessage.c_str());
  }

  auto bounds = GetContentBounds(element, fontProvider);

  auto type = element->nodeType();
  bool stretchable = ElementConstraint::IsStretchable(type);

  // Proportional scaling for non-stretchable, non-Group elements
  if (!stretchable && !skipScaling && type != NodeType::Group) {
    bool hasLeftRight = !std::isnan(constraints.left) && !std::isnan(constraints.right);
    bool hasTopBottom = !std::isnan(constraints.top) && !std::isnan(constraints.bottom);
    if (hasLeftRight || hasTopBottom) {
      float scale = 0.0f;
      if (hasLeftRight && bounds.width > 0) {
        float areaWidth = containerWidth - constraints.left - constraints.right;
        scale = areaWidth / bounds.width;
      }
      if (hasTopBottom && bounds.height > 0) {
        float scaleY = (containerHeight - constraints.top - constraints.bottom) / bounds.height;
        scale = (scale > 0) ? std::min(scale, scaleY) : scaleY;
      }
      if (scale > 0 && scale != 1.0f) {
        ElementTransform::ApplyScaling(element, scale);
        bounds = GetContentBounds(element, fontProvider);
      }
    }
  }

  // Horizontal axis: centerX > (left + right) > left > right
  bool hasLeft = !std::isnan(constraints.left);
  bool hasRight = !std::isnan(constraints.right);
  bool hasCenterX = !std::isnan(constraints.centerX);

  if (hasLeft && hasRight) {
    float left = constraints.left;
    float right = constraints.right;
    float areaWidth = containerWidth - left - right;
    if (type == NodeType::Group) {
      auto* group = static_cast<Group*>(element);
      group->position.x = left;
      ElementTransform::UpdateGroupLayoutSize(group, areaWidth, NAN);
    } else if (stretchable) {
      float newCenterX = left + areaWidth * 0.5f;
      ElementTransform::ApplyHorizontalStretch(element, areaWidth, newCenterX);
    } else {
      float tx = left + (areaWidth - bounds.width) * 0.5f - bounds.x;
      ElementTransform::TranslateX(element, tx);
    }
  } else if (hasLeft) {
    float tx = constraints.left - bounds.x;
    ElementTransform::TranslateX(element, tx);
  } else if (hasRight) {
    float tx = (containerWidth - constraints.right) - (bounds.x + bounds.width);
    ElementTransform::TranslateX(element, tx);
  } else if (hasCenterX) {
    float boundsCenter = bounds.x + bounds.width * 0.5f;
    float tx = (containerWidth * 0.5f + constraints.centerX) - boundsCenter;
    ElementTransform::TranslateX(element, tx);
  }

  // Vertical axis: centerY > (top + bottom) > top > bottom
  bool hasTop = !std::isnan(constraints.top);
  bool hasBottom = !std::isnan(constraints.bottom);
  bool hasCenterY = !std::isnan(constraints.centerY);

  if (hasTop && hasBottom) {
    float top = constraints.top;
    float bottom = constraints.bottom;
    float areaHeight = containerHeight - top - bottom;
    if (type == NodeType::Group) {
      auto* group = static_cast<Group*>(element);
      group->position.y = top;
      ElementTransform::UpdateGroupLayoutSize(group, NAN, areaHeight);
    } else if (stretchable) {
      float newCenterY = top + areaHeight * 0.5f;
      ElementTransform::ApplyVerticalStretch(element, areaHeight, newCenterY);
    } else {
      float ty = top + (areaHeight - bounds.height) * 0.5f - bounds.y;
      ElementTransform::TranslateY(element, ty);
    }
  } else if (hasTop) {
    float ty = constraints.top - bounds.y;
    ElementTransform::TranslateY(element, ty);
  } else if (hasBottom) {
    float ty = (containerHeight - constraints.bottom) - (bounds.y + bounds.height);
    ElementTransform::TranslateY(element, ty);
  } else if (hasCenterY) {
    float boundsCenter = bounds.y + bounds.height * 0.5f;
    float ty = (containerHeight * 0.5f + constraints.centerY) - boundsCenter;
    ElementTransform::TranslateY(element, ty);
  }
}

void ConstraintLayout::ApplyLayerConstraints(Layer* layer, float parentWidth, float parentHeight,
                                             float measuredWidth, float measuredHeight) {
  bool hasLeft = !std::isnan(layer->left);
  bool hasRight = !std::isnan(layer->right);
  bool hasCenterX = !std::isnan(layer->centerX);
  bool hasTop = !std::isnan(layer->top);
  bool hasBottom = !std::isnan(layer->bottom);
  bool hasCenterY = !std::isnan(layer->centerY);

  if (!hasLeft && !hasRight && !hasCenterX && !hasTop && !hasBottom && !hasCenterY) {
    return;
  }

  float childWidth = std::isnan(layer->width) ? measuredWidth : layer->width;
  float childHeight = std::isnan(layer->height) ? measuredHeight : layer->height;

  // Horizontal axis: centerX > (left + right) > left > right
  if (hasCenterX) {
    layer->x = (parentWidth - childWidth) * 0.5f + layer->centerX;
  } else if (hasLeft && hasRight) {
    layer->x = layer->left;
    layer->width = parentWidth - layer->left - layer->right;
  } else if (hasLeft) {
    layer->x = layer->left;
  } else if (hasRight) {
    layer->x = parentWidth - layer->right - childWidth;
  }

  // Vertical axis: centerY > (top + bottom) > top > bottom
  if (hasCenterY) {
    layer->y = (parentHeight - childHeight) * 0.5f + layer->centerY;
  } else if (hasTop && hasBottom) {
    layer->y = layer->top;
    layer->height = parentHeight - layer->top - layer->bottom;
  } else if (hasTop) {
    layer->y = layer->top;
  } else if (hasBottom) {
    layer->y = parentHeight - layer->bottom - childHeight;
  }
}

void ConstraintLayout::ApplyLayerConstraints(Layer* layer, float parentWidth, float parentHeight) {
  bool hasLeft = !std::isnan(layer->left);
  bool hasRight = !std::isnan(layer->right);
  bool hasCenterX = !std::isnan(layer->centerX);
  bool hasTop = !std::isnan(layer->top);
  bool hasBottom = !std::isnan(layer->bottom);
  bool hasCenterY = !std::isnan(layer->centerY);

  if (!hasLeft && !hasRight && !hasCenterX && !hasTop && !hasBottom && !hasCenterY) {
    return;
  }

  // Get layer's measured dimensions
  // TODO: After migration, this should call layer->measure() virtual method instead
  // For now, we rely on external measurement being done before constraint application
  float childWidth = std::isnan(layer->width) ? 0 : layer->width;
  float childHeight = std::isnan(layer->height) ? 0 : layer->height;

  // Horizontal axis: centerX > (left + right) > left > right
  if (hasCenterX) {
    layer->x = (parentWidth - childWidth) * 0.5f + layer->centerX;
  } else if (hasLeft && hasRight) {
    layer->x = layer->left;
    layer->width = parentWidth - layer->left - layer->right;
  } else if (hasLeft) {
    layer->x = layer->left;
  } else if (hasRight) {
    layer->x = parentWidth - layer->right - childWidth;
  }

  // Vertical axis: centerY > (top + bottom) > top > bottom
  if (hasCenterY) {
    layer->y = (parentHeight - childHeight) * 0.5f + layer->centerY;
  } else if (hasTop && hasBottom) {
    layer->y = layer->top;
    layer->height = parentHeight - layer->top - layer->bottom;
  } else if (hasTop) {
    layer->y = layer->top;
  } else if (hasBottom) {
    layer->y = parentHeight - layer->bottom - childHeight;
  }
}

void ConstraintLayout::ApplyElementsConstraints(const std::vector<Element*>& elements,
                                                float containerWidth, float containerHeight,
                                                FontConfig* fontProvider) {
  // Check if any TextBox exists
  bool hasTextBox = false;
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::TextBox) {
      hasTextBox = true;
      break;
    }
  }

  for (auto* element : elements) {
    bool skipScaling = hasTextBox && element->nodeType() == NodeType::Text;
    ApplyElementConstraints(element, containerWidth, containerHeight, fontProvider, skipScaling);

    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      float groupW = group->width;
      float groupH = group->height;
      if (std::isnan(groupW) || std::isnan(groupH)) {
        auto bounds = ElementMeasure::GetMeasuredBounds(group, [fontProvider](const Text* text) {
          return ComputeTextBounds(text, fontProvider);
        });
        if (std::isnan(groupW)) {
          groupW = bounds.width;
        }
        if (std::isnan(groupH)) {
          groupH = bounds.height;
        }
      }
      ApplyElementsConstraints(group->elements, groupW, groupH, fontProvider);
    }
  }
}

}  // namespace pagx
