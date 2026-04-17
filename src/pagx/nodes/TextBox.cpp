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
    measuredWidth = width;
    measuredHeight = height;
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
  measuredWidth = std::isnan(width) ? result.bounds.width : width;
  measuredHeight = std::isnan(height) ? result.bounds.height : height;
  if (hasPadding) {
    if (std::isnan(width)) {
      measuredWidth += padding.left + padding.right;
    }
    if (std::isnan(height)) {
      measuredHeight += padding.top + padding.bottom;
    }
  }
}

void TextBox::setLayoutSize(LayoutContext* context, float width, float height) {
  layoutWidth = !std::isnan(width) ? width : measuredWidth;
  layoutHeight = !std::isnan(height) ? height : measuredHeight;
  updateLayout(context);
  // An axis is content-measured when neither the parent nor the element itself specifies its size.
  bool widthFromContent = std::isnan(width) && std::isnan(this->width);
  bool heightFromContent = std::isnan(height) && std::isnan(this->height);
  // For TextBox, only a change in the wrap axis (width for horizontal, height for vertical)
  // can affect the cross axis measurement. Re-typeset to compute the correct cross-axis size.
  bool horizontal = (writingMode == WritingMode::Horizontal);
  bool wrapAxisChanged = horizontal ? (!std::isnan(width) && width != measuredWidth)
                                    : (!std::isnan(height) && height != measuredHeight);
  bool crossAxisFromContent = horizontal ? heightFromContent : widthFromContent;
  if (crossAxisFromContent && wrapAxisChanged) {
    bool hasPadding = !padding.isZero();
    float boxW =
        hasPadding ? std::max(0.0f, layoutWidth - padding.left - padding.right) : layoutWidth;
    float boxH =
        hasPadding ? std::max(0.0f, layoutHeight - padding.top - padding.bottom) : layoutHeight;
    auto params = MakeTextLayoutParams(this, boxW, boxH);
    std::vector<Text*> childText = {};
    TextLayout::CollectTextElements(elements, childText);
    auto result = TextLayout::Layout(childText, params, context);
    float crossSize = std::ceil(horizontal ? result.bounds.height : result.bounds.width);
    if (hasPadding) {
      crossSize += horizontal ? (padding.top + padding.bottom) : (padding.left + padding.right);
    }
    if (horizontal) {
      layoutHeight = crossSize;
    } else {
      layoutWidth = crossSize;
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
    auto* text = childText[i];
    text->textBounds = result.getTextBounds(text);
    text->glyphData->layoutRuns = result.extractLayoutRuns(text);
    text->glyphData->fontLineHeight = result.getFontLineHeight(text);
    if (hasPadding) {
      text->textBounds.x += padding.left;
      text->textBounds.y += padding.top;
      for (auto& run : text->glyphData->layoutRuns) {
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
    text->measuredX = text->textBounds.x;
    text->measuredY = text->textBounds.y;
    text->measuredWidth = text->textBounds.width;
    text->measuredHeight = text->textBounds.height;
    // Reset layoutWidth/Height so layoutBounds() falls back to measured values. This is necessary
    // because Group::updateLayout may have called setLayoutSize(NAN,NAN) on this Text before
    // TextBox re-typesets it, leaving stale layoutWidth/Height from the old measured size.
    text->layoutWidth = NAN;
    text->layoutHeight = NAN;
  }
}

}  // namespace pagx
