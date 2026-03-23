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

#include "pagx/layout/LayoutUtils.h"
#include <cmath>
#include "TextLayout.h"
#include "pagx/FontProvider.h"
#include "pagx/layout/ElementMeasure.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

Rect LayoutUtils::ComputeTextBounds(const Text* text, FontConfig* fontProvider) {
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

Rect LayoutUtils::GetContentBounds(const Element* element, FontConfig* fontProvider) {
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

}  // namespace pagx
