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

void TextGlyphs::add(Text* text, std::shared_ptr<tgfx::TextBlob> textBlob) {
  if (text != nullptr && textBlob != nullptr) {
    textBlobs[text] = std::move(textBlob);
  }
}

std::shared_ptr<tgfx::TextBlob> TextGlyphs::get(const Text* text) const {
  auto it = textBlobs.find(const_cast<Text*>(text));
  if (it != textBlobs.end()) {
    return it->second;
  }
  return nullptr;
}

bool TextGlyphs::contains(const Text* text) const {
  return textBlobs.find(const_cast<Text*>(text)) != textBlobs.end();
}

void TextGlyphs::forEach(
    std::function<void(Text*, std::shared_ptr<tgfx::TextBlob>)> callback) const {
  for (const auto& pair : textBlobs) {
    callback(pair.first, pair.second);
  }
}

size_t TextGlyphs::size() const {
  return textBlobs.size();
}

bool TextGlyphs::empty() const {
  return textBlobs.empty();
}

}  // namespace pagx
