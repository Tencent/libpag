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
  textScale = 1.0f;
  auto params = MakeStandaloneParams(this);
  auto result = TextLayout::Layout({this}, params, context);
  glyphData->layoutRuns = result.extractLayoutRuns(this);
  textBounds = result.bounds;
  // position is the text origin (x, baseline); textBounds is relative to position.
  preferredX = position.x + textBounds.x;
  preferredY = position.y + textBounds.y;
  // Preferred size: authored width/height overrides the intrinsic text bounds when present.
  preferredWidth = std::isnan(width) ? textBounds.width : width;
  preferredHeight = std::isnan(height) ? textBounds.height : height;
}

void Text::setLayoutSize(LayoutContext* context, float targetWidth, float targetHeight) {
  // Compute scale based on intrinsic text bounds (pre-scaling) and the requested target. NaN on
  // an axis means the parent did not constrain that axis, so ComputeUniformScale skips it. When
  // both axes are NaN, fall back to the preferred size so an authored width/height still drives
  // scaling without parent constraints.
  float tW = targetWidth;
  float tH = targetHeight;
  if (std::isnan(tW) && std::isnan(tH)) {
    tW = preferredWidth;
    tH = preferredHeight;
  }
  float scale = LayoutNode::ComputeUniformScale(textBounds.width, textBounds.height, tW, tH);
  float origW = textBounds.width;
  float origH = textBounds.height;
  if (scale != 1.0f) {
    textScale = scale;
    auto params = MakeStandaloneParams(this);
    params.textScale = scale;
    auto result = TextLayout::Layout({this}, params, context);
    glyphData->layoutRuns = result.extractLayoutRuns(this);
    textBounds = result.bounds;
  }
  // Use mathematically scaled dimensions instead of textBounds from re-typesetting, because font
  // hinting may cause slight differences. renderPosition() compensates via centering.
  layoutWidth = origW * scale;
  layoutHeight = origH * scale;
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
