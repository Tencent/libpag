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

void TextGlyphs::set(Text* text, ShapedText data) {
  if (text != nullptr && data.textBlob != nullptr) {
    shapedTextMap[text] = std::move(data);
  }
}

const ShapedText* TextGlyphs::get(const Text* text) const {
  auto it = shapedTextMap.find(const_cast<Text*>(text));
  if (it != shapedTextMap.end()) {
    return &it->second;
  }
  return nullptr;
}

bool TextGlyphs::empty() const {
  return shapedTextMap.empty();
}

}  // namespace pagx
