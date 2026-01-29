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
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/Typesetter.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * Text elements are rendered using the Typesetter to create TextGlyphs.
 */
class LayerBuilder {
 public:
  /**
   * Options for building layers from PAGX data.
   */
  struct Options {
    /**
     * Fallback typefaces used when a character is not found in the primary font. Typefaces are
     * tried in order until one containing the character is found.
     */
    std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;
  };

  /**
   * Result of building layers from PAGX data.
   */
  struct Result {
    /**
     * The root layer of the built layer tree. nullptr if parsing or building failed.
     */
    std::shared_ptr<tgfx::Layer> root = nullptr;

    /**
     * The width of the PAGX document.
     */
    float width = 0;

    /**
     * The height of the PAGX document.
     */
    float height = 0;
  };

  /**
   * Parses PAGX data and builds a layer tree. This is a convenience method that combines parsing
   * and building in one step.
   * @param data The raw PAGX data bytes.
   * @param length The length of the data in bytes.
   * @param options Options for building, including fallback typefaces.
   * @return Result containing the root layer and document dimensions.
   */
  static Result FromData(const uint8_t* data, size_t length, const Options& options = Options{});

  /**
   * Builds a layer tree from a PAGXDocument.
   * @param document The document to build from.
   * @param typesetter Optional typesetter for text rendering. If nullptr, a default Typesetter is
   *                   created internally. Pass a custom Typesetter to use registered typefaces
   *                   and fallback fonts.
   * @return The root layer of the built layer tree.
   */
  static std::shared_ptr<tgfx::Layer> Build(PAGXDocument* document,
                                            Typesetter* typesetter = nullptr);
};

}  // namespace pagx
