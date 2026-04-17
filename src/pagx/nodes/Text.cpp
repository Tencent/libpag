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

#include "pagx/nodes/Text.h"
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

Text::~Text() {
  delete glyphData;
}

Text::Text() : glyphData(new GlyphData()) {
}

static TextLayoutParams MakeStandaloneParams(const Text* text) {
  TextLayoutParams params = {};
  params.baseline = text->baseline;
  switch (text->textAnchor) {
    case TextAnchor::Start:
      params.textAlign = TextAlign::Start;
      break;
    case TextAnchor::Center:
      params.textAlign = TextAlign::Center;
      break;
    case TextAnchor::End:
      params.textAlign = TextAlign::End;
      break;
  }
  return params;
}

void Text::onMeasure(LayoutContext* context) {
  fontScale_ = 1.0f;
  auto params = MakeStandaloneParams(this);
  auto result = TextLayout::Layout({this}, params, context);
  glyphData->layoutRuns = result.extractLayoutRuns(this);
  textBounds = result.bounds;
  measuredX = position.x + textBounds.x;
  measuredY = position.y + textBounds.y;
  measuredWidth = textBounds.width;
  measuredHeight = textBounds.height;
}

void Text::setLayoutSize(LayoutContext* context, float width, float height) {
  float scale = LayoutNode::ComputeUniformScale(measuredWidth, measuredHeight, width, height);
  if (scale != 1.0f) {
    fontScale_ = scale;
    auto params = MakeStandaloneParams(this);
    params.fontScale = scale;
    auto result = TextLayout::Layout({this}, params, context);
    glyphData->layoutRuns = result.extractLayoutRuns(this);
    textBounds = result.bounds;
  }
  layoutWidth = textBounds.width;
  layoutHeight = textBounds.height;
}

void Text::setLayoutPosition(LayoutContext*, float x, float y) {
  if (!std::isnan(x)) {
    layoutX = x;
  }
  if (!std::isnan(y)) {
    layoutY = y;
  }
}

Point Text::renderPosition() const {
  auto bounds = layoutBounds();
  return {bounds.x - textBounds.x, bounds.y - textBounds.y};
}

float Text::renderFontSize() const {
  return fontSize * fontScale_;
}

}  // namespace pagx
