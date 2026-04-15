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

#include "pagx/nodes/TextBox.h"
#include <cmath>
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

static TextLayoutParams MakeTextLayoutParams(const TextBox* textBox, float boxWidth,
                                             float boxHeight) {
  TextLayoutParams params = {};
  params.boxWidth = boxWidth;
  params.boxHeight = boxHeight;
  params.textAlign = textBox->textAlign;
  params.paragraphAlign = textBox->paragraphAlign;
  params.writingMode = textBox->writingMode;
  params.lineHeight = textBox->lineHeight;
  params.wordWrap = textBox->wordWrap;
  params.overflow = textBox->overflow;
  return params;
}

void TextBox::onMeasure(LayoutContext* context) {
  if (!std::isnan(width) && !std::isnan(height)) {
    preferredWidth = width;
    preferredHeight = height;
    return;
  }
  bool hasPadding = !padding.isZero();
  float boxW = width;
  float boxH = height;
  if (hasPadding) {
    if (!std::isnan(boxW)) {
      boxW = std::max(0.0f, boxW - padding.left - padding.right);
    }
    if (!std::isnan(boxH)) {
      boxH = std::max(0.0f, boxH - padding.top - padding.bottom);
    }
  }
  auto params = MakeTextLayoutParams(this, boxW, boxH);
  std::vector<Text*> childText = {};
  TextLayout::CollectTextElements(elements, childText);
  auto result = TextLayout::Layout(childText, params, context);
  preferredWidth = std::isnan(width) ? result.bounds.width : width;
  preferredHeight = std::isnan(height) ? result.bounds.height : height;
  if (hasPadding) {
    if (std::isnan(width)) {
      preferredWidth += padding.left + padding.right;
    }
    if (std::isnan(height)) {
      preferredHeight += padding.top + padding.bottom;
    }
  }
}

void TextBox::setLayoutSize(LayoutContext* context, float width, float height) {
  layoutWidth = !std::isnan(width) ? width : preferredWidth;
  layoutHeight = !std::isnan(height) ? height : preferredHeight;
  // When the constraint-derived width differs from the measured width and height is
  // content-measured (no explicit height AND not constrained), re-typeset to compute the correct
  // multi-line height.
  if (!std::isnan(width) && std::isnan(height) && std::isnan(this->height) &&
      width != preferredWidth) {
    bool hasPadding = !padding.isZero();
    float boxW =
        hasPadding ? std::max(0.0f, layoutWidth - padding.left - padding.right) : layoutWidth;
    float boxH =
        hasPadding ? std::max(0.0f, layoutHeight - padding.top - padding.bottom) : layoutHeight;
    auto params = MakeTextLayoutParams(this, boxW, boxH);
    std::vector<Text*> childText = {};
    TextLayout::CollectTextElements(elements, childText);
    auto result = TextLayout::Layout(childText, params, context);
    layoutHeight = std::ceil(result.bounds.height);
    if (hasPadding) {
      layoutHeight += padding.top + padding.bottom;
    }
  }
}

void TextBox::updateLayout(LayoutContext* context) {
  bool hasPadding = !padding.isZero();
  float cw = hasPadding ? std::max(0.0f, layoutWidth - padding.left - padding.right) : layoutWidth;
  float ch =
      hasPadding ? std::max(0.0f, layoutHeight - padding.top - padding.bottom) : layoutHeight;
  // Non-Text elements: constraint layout with padding.
  auto nodes = CollectLayoutNodes(elements, true);
  PerformConstraintLayout(nodes, layoutWidth, layoutHeight, padding, context);
  // Text elements: typeset with inset bounds, then offset positions by padding.
  auto params = MakeTextLayoutParams(this, cw, ch);
  std::vector<Text*> childText = {};
  TextLayout::CollectTextElements(elements, childText);
  auto result = TextLayout::Layout(childText, params, context);
  for (size_t i = 0; i < childText.size(); i++) {
    childText[i]->textBounds = result.getTextBounds(childText[i]);
    childText[i]->glyphData->layoutRuns = result.extractLayoutRuns(childText[i]);
    if (hasPadding) {
      childText[i]->textBounds.x += padding.left;
      childText[i]->textBounds.y += padding.top;
      for (auto& run : childText[i]->glyphData->layoutRuns) {
        for (auto& pos : run.positions) {
          pos.x += padding.left;
          pos.y += padding.top;
        }
        for (auto& xf : run.xforms) {
          xf.tx += padding.left;
          xf.ty += padding.top;
        }
      }
    }
  }
}

}  // namespace pagx
