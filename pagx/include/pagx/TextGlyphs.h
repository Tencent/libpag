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
#include <vector>
#include "tgfx/core/Point.h"
#include "tgfx/core/TextBlob.h"

namespace pagx {

class Text;

/**
 * Shaped text data containing the TextBlob and per-glyph anchor offsets.
 */
struct ShapedText {
  /**
   * The shaped TextBlob containing glyph positions.
   */
  std::shared_ptr<tgfx::TextBlob> textBlob = nullptr;

  /**
   * Per-glyph anchor offsets relative to default anchor (advance * 0.5, 0).
   */
  std::vector<tgfx::Point> anchors = {};
};

/**
 * TextGlyphs holds the text typesetting results, mapping Text nodes to their shaped TextBlob
 * representations. This class serves as the bridge between text typesetting (by Typesetter) and
 * downstream consumers (LayerBuilder for rendering, FontEmbedder for font embedding).
 */
class TextGlyphs {
 public:
  TextGlyphs() = default;

  /**
   * Sets the shaped text data for a Text node.
   */
  void set(Text* text, ShapedText data);

  /**
   * Returns the shaped text data for the given Text node, or nullptr if not found.
   */
  const ShapedText* get(const Text* text) const;

  /**
   * Returns true if there are no mappings.
   */
  bool empty() const;

 private:
  friend class FontEmbedder;

  std::unordered_map<Text*, ShapedText> shapedTextMap = {};
};

}  // namespace pagx
