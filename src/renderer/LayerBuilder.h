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
#include "pagx/FontConfig.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * Result of building a layer tree, containing the root layer and an optional mapping from PAGX
 * Layer nodes to their corresponding tgfx::Layer objects.
 */
struct LayerBuildResult {
  std::shared_ptr<tgfx::Layer> root = nullptr;
  std::unordered_map<const Layer*, std::shared_ptr<tgfx::Layer>> layerMap = {};
};

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * Text elements are rendered using the TextLayout to create ShapedText.
 */
class LayerBuilder {
 public:
  /**
   * Builds a layer tree from a PAGXDocument.
   * @param document The document to build from.
   * @param fontConfig Optional font config for text measurement and rendering. If nullptr,
   *                     only system fonts are available.
   * @return The root layer of the built layer tree.
   */
  static std::shared_ptr<tgfx::Layer> Build(PAGXDocument* document,
                                            FontConfig* fontConfig = nullptr);

  /**
   * Builds a layer tree and returns a mapping from PAGX Layer nodes to tgfx::Layer objects. This
   * mapping allows callers to look up the rendered layer for any PAGX Layer node.
   * @param document The document to build from.
   * @param fontConfig Optional font config for text measurement and rendering. If nullptr,
   *                     only system fonts are available.
   * @return A LayerBuildResult containing the root layer and a mapping from PAGX Layer nodes to
   *         their corresponding tgfx::Layer objects.
   */
  static LayerBuildResult BuildWithMap(PAGXDocument* document, FontConfig* fontConfig = nullptr);
};

}  // namespace pagx
