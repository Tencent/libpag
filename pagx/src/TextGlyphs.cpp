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

#include "pagx/TextGlyphs.h"

namespace pagx {

static const std::vector<tgfx::Point> EmptyAnchors = {};

void TextGlyphs::setTextBlob(Text* text, std::shared_ptr<tgfx::TextBlob> textBlob,
                             std::vector<tgfx::Point> anchors) {
  if (text != nullptr && textBlob != nullptr) {
    TextData data = {};
    data.textBlob = std::move(textBlob);
    data.anchors = std::move(anchors);
    textDataMap[text] = std::move(data);
  }
}

std::shared_ptr<tgfx::TextBlob> TextGlyphs::getTextBlob(const Text* text) const {
  auto it = textDataMap.find(const_cast<Text*>(text));
  if (it != textDataMap.end()) {
    return it->second.textBlob;
  }
  return nullptr;
}

const std::vector<tgfx::Point>& TextGlyphs::getAnchors(const Text* text) const {
  auto it = textDataMap.find(const_cast<Text*>(text));
  if (it != textDataMap.end()) {
    return it->second.anchors;
  }
  return EmptyAnchors;
}

bool TextGlyphs::empty() const {
  return textDataMap.empty();
}

}  // namespace pagx
