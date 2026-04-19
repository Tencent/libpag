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
  // TextBox keeps width/height as the authored text-box dimensions for shaping. this->width /
  // this->height constrain the shaping box and also become the preferred size when set; otherwise
  // the shaped text bounds drive the preferred size. percentWidth/percentHeight are not consulted
  // here; they are resolved by the parent via PerformConstraintLayout.
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

void TextBox::setLayoutSize(LayoutContext* context, float targetWidth, float targetHeight) {
  // Mirror Group::setLayoutSize: keep content-measured axes NaN during pass 1 so percent
  // descendants fall back to their preferred size instead of locking onto a provisional value.
  bool widthFromContent = std::isnan(targetWidth) && std::isnan(this->width);
  bool heightFromContent = std::isnan(targetHeight) && std::isnan(this->height);
  layoutWidth = widthFromContent ? NAN : (!std::isnan(targetWidth) ? targetWidth : preferredWidth);
  layoutHeight =
      heightFromContent ? NAN : (!std::isnan(targetHeight) ? targetHeight : preferredHeight);
  updateLayout(context);
  // Settle deferred axes to the preferred size and re-run updateLayout so descendants pick up
  // the numeric container size before the cross-axis re-typesetting below.
  if (widthFromContent || heightFromContent) {
    if (widthFromContent) {
      layoutWidth = preferredWidth;
    }
    if (heightFromContent) {
      layoutHeight = preferredHeight;
    }
    updateLayout(context);
  }
  // For TextBox, only a change in the wrap axis (width for horizontal, height for vertical)
  // can affect the cross axis measurement. Re-typeset to compute the correct cross-axis size,
  // then re-run updateLayout so non-Text descendants pick up the refined container size.
  bool horizontal = (writingMode == WritingMode::Horizontal);
  bool wrapAxisChanged = horizontal
                             ? (!std::isnan(targetWidth) && targetWidth != preferredWidth)
                             : (!std::isnan(targetHeight) && targetHeight != preferredHeight);
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
    float prevCross = horizontal ? layoutHeight : layoutWidth;
    if (horizontal) {
      layoutHeight = crossSize;
    } else {
      layoutWidth = crossSize;
    }
    if (crossSize != prevCross) {
      updateLayout(context);
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
    text->preferredX = text->textBounds.x;
    text->preferredY = text->textBounds.y;
    text->preferredWidth = text->textBounds.width;
    text->preferredHeight = text->textBounds.height;
    // Reset layoutWidth/Height so layoutBounds() falls back to preferred values. This is necessary
    // because Group::updateLayout may have called setLayoutSize(NAN,NAN) on this Text before
    // TextBox re-typesets it, leaving stale layoutWidth/Height from the old preferred size.
    text->layoutWidth = NAN;
    text->layoutHeight = NAN;
  }
}

}  // namespace pagx
