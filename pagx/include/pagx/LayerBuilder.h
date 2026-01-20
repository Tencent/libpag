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
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * Result of building a layer tree from a PAGXDocument.
 */
struct PAGXContent {
  /**
   * The root layer of the built layer tree.
   */
  std::shared_ptr<tgfx::Layer> root = nullptr;

  /**
   * The width of the content.
   */
  float width = 0;

  /**
   * The height of the content.
   */
  float height = 0;
};

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * This is the bridge between the independent pagx module and tgfx rendering.
 */
class LayerBuilder {
 public:
  struct Options {
    /**
     * Fallback typefaces for text rendering.
     */
    std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces = {};

    /**
     * Base path for resolving relative resource paths.
     */
    std::string basePath = {};

    Options() = default;
  };

  /**
   * Builds a layer tree from a PAGXDocument.
   */
  static PAGXContent Build(const PAGXDocument& document, const Options& options = Options());

  /**
   * Builds a layer tree from a PAGX file.
   */
  static PAGXContent FromFile(const std::string& filePath, const Options& options = Options());

  /**
   * Builds a layer tree from PAGX XML data.
   */
  static PAGXContent FromData(const uint8_t* data, size_t length, const Options& options = Options());

  /**
   * Builds a layer tree from an SVG file.
   * This is a convenience method that first parses the SVG, then builds the layer tree.
   */
  static PAGXContent FromSVGFile(const std::string& filePath, const Options& options = Options());
};

}  // namespace pagx
