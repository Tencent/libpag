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

#pragma once

#include <memory>
#include <unordered_map>
#include "tgfx/core/TextBlob.h"

namespace pagx {

class Text;
class FontEmbedderImpl;

/**
 * TextGlyphs holds the text typesetting results, mapping Text nodes to their shaped TextBlob
 * representations. This class serves as the bridge between text typesetting (by Typesetter) and
 * downstream consumers (LayerBuilder for rendering, FontEmbedder for font embedding).
 */
class TextGlyphs {
 public:
  TextGlyphs() = default;

  /**
   * Sets the TextBlob for a Text node.
   */
  void setTextBlob(Text* text, std::shared_ptr<tgfx::TextBlob> textBlob);

  /**
   * Returns the TextBlob for the given Text node, or nullptr if not found.
   */
  std::shared_ptr<tgfx::TextBlob> getTextBlob(const Text* text) const;

  /**
   * Returns true if there are no mappings.
   */
  bool empty() const;

 private:
  friend class FontEmbedder;

  std::unordered_map<Text*, std::shared_ptr<tgfx::TextBlob>> textBlobs = {};
};

}  // namespace pagx
