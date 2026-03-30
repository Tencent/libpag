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
#include "pagx/layout/LayoutNode.h"

namespace pagx {

static TextLayoutParams MakeStandaloneParams(const Text* text) {
  TextLayoutParams params = {};
  params.baseline = text->baseline;
  // Map textAnchor to textAlign so the unified layout path applies the correct horizontal offset.
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

void Text::onMeasure(const LayoutContext& context) {
  auto params = MakeStandaloneParams(this);
  textBounds = TextLayout::Layout({this}, params, context);
  preferredX = textBounds.x;
  preferredY = textBounds.y;
  preferredWidth = textBounds.width;
  preferredHeight = textBounds.height;
}

void Text::setLayoutSize(const LayoutContext& context, float width, float height) {
  float scale = LayoutNode::ComputeUniformScale(preferredWidth, preferredHeight, width, height);
  if (scale != 1.0f) {
    fontSize = fontSize * scale;
    auto params = MakeStandaloneParams(this);
    textBounds = TextLayout::Layout({this}, params, context);
  }
  actualWidth = textBounds.width;
  actualHeight = textBounds.height;
}

void Text::setLayoutPosition(const LayoutContext&, float x, float y) {
  if (!std::isnan(x)) {
    position.x = x - textBounds.x;
  }
  if (!std::isnan(y)) {
    position.y = y - textBounds.y;
  }
}

}  // namespace pagx
