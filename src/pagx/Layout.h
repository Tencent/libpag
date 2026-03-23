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
#include "ShapedText.h"
#include "pagx/FontProvider.h"

namespace pagx {

class PAGXDocument;

/**
 * Layout performs unified auto layout and text shaping for a PAGXDocument. It coordinates:
 * - Container layout (arranging child Layers with flex, gap, alignment, etc.)
 * - Constraint positioning (positioning elements relative to container bounds)
 * - Text shaping (converting Text elements into positioned glyph data)
 *
 * After calling apply(), the document's layer positions are updated in-place, and the shaped text
 * results are available via shapedTextMap() for rendering.
 */
class Layout {
 public:
  /**
   * Creates a Layout with an optional external FontConfig.
   * If externalConfig is provided, Layout uses it directly (does not take ownership).
   * Otherwise, Layout creates and owns its own FontConfig.
   */
  explicit Layout(FontConfig* externalConfig = nullptr)
      : externalConfig_(externalConfig), ownsProvider_(externalConfig == nullptr) {
    if (ownsProvider_) {
      fontProvider_ = std::make_unique<FontConfig>();
    }
  }

  ~Layout();

  /**
   * Returns the FontConfig used for font matching during layout and text rendering.
   * Register typefaces and fallback fonts on this config before calling apply().
   */
  FontConfig* getFontProvider();

  /**
   * Performs unified layout on all layers and content in the document. This includes:
   * - Arranging child Layers according to their layout modes (horizontal, vertical)
   * - Positioning content elements using constraints (left, right, top, bottom, etc.)
   * - Shaping all Text elements into glyph data (TextBlob)
   */
  void apply(PAGXDocument* document);

  /**
   * Returns the shaped text map produced by the most recent apply() call. The map contains
   * TextBlob and anchor data for each Text element in the document.
   */
  const ShapedTextMap& shapedTextMap() const;

 private:
  std::unique_ptr<FontConfig> fontProvider_ = nullptr;
  FontConfig* externalConfig_ = nullptr;
  bool ownsProvider_ = false;
  ShapedTextMap shapedTextMap_ = {};
};

}  // namespace pagx
