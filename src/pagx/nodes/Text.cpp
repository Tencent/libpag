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

float Text::fontLineHeight() const {
  return glyphData->fontLineHeight;
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
  textScale = 1.0f;
  auto params = MakeStandaloneParams(this);
  auto result = TextLayout::Layout({this}, params, context);
  glyphData->layoutRuns = result.extractLayoutRuns(this);
  glyphData->fontLineHeight = result.getFontLineHeight(this);
  textBounds = result.bounds;
  measuredX = position.x + textBounds.x;
  measuredY = position.y + textBounds.y;
  measuredWidth = textBounds.width;
  measuredHeight = textBounds.height;
}

void Text::setLayoutSize(LayoutContext* context, float width, float height) {
  float scale = LayoutNode::ComputeUniformScale(measuredWidth, measuredHeight, width, height);
  if (scale != 1.0f) {
    textScale = scale;
    auto params = MakeStandaloneParams(this);
    params.textScale = scale;
    auto result = TextLayout::Layout({this}, params, context);
    glyphData->layoutRuns = result.extractLayoutRuns(this);
    glyphData->fontLineHeight = result.getFontLineHeight(this);
    textBounds = result.bounds;
  }
  // Use mathematically scaled dimensions instead of textBounds from re-typesetting, because font
  // hinting may cause slight differences. renderPosition() compensates via centering.
  layoutWidth = measuredWidth * scale;
  layoutHeight = measuredHeight * scale;
}

Point Text::renderPosition() const {
  auto bounds = layoutBounds();
  float offsetX = (bounds.width - textBounds.width) * 0.5f;
  float offsetY = (bounds.height - textBounds.height) * 0.5f;
  return {bounds.x + offsetX - textBounds.x, bounds.y + offsetY - textBounds.y};
}

float Text::renderFontSize() const {
  return fontSize * textScale;
}

}  // namespace pagx
