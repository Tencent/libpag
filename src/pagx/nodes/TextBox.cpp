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
#include "pagx/LayoutContext.h"
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/layout/LayoutNode.h"

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
  auto params = MakeTextLayoutParams(this, width, height);
  std::vector<Text*> childText = {};
  TextLayout::CollectTextElements(elements, childText);
  auto result = TextLayout::Layout(childText, params, context);
  preferredWidth = std::isnan(width) ? result.bounds.width : width;
  preferredHeight = std::isnan(height) ? result.bounds.height : height;
}

void TextBox::setLayoutSize(LayoutContext*, float width, float height) {
  layoutWidth = !std::isnan(width) ? width : preferredWidth;
  layoutHeight = !std::isnan(height) ? height : preferredHeight;
}

void TextBox::updateLayout(LayoutContext* context) {
  auto nodes = CollectLayoutNodes(elements, true);
  PerformConstraintLayout(nodes, layoutWidth, layoutHeight, context);
  auto params = MakeTextLayoutParams(this, layoutWidth, layoutHeight);
  std::vector<Text*> childText = {};
  TextLayout::CollectTextElements(elements, childText);
  auto result = TextLayout::Layout(childText, params, context);
  for (size_t i = 0; i < childText.size(); i++) {
    childText[i]->textBounds = result.getTextBounds(childText[i]);
    childText[i]->layoutRuns = result.extractLayoutRuns(childText[i]);
  }
}

}  // namespace pagx
