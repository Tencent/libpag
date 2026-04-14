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
 * The document must have applyLayout() called before building.
 */
class LayerBuilder {
 public:
  /**
   * Builds a layer tree from a PAGXDocument.
   * @param document The document to build from. Must have had applyLayout() called.
   * @param maxImageDimension Maximum allowed image dimension (width or height). Images exceeding
   *                          this limit will be scaled down proportionally. A value of 0 means no
   *                          limit. Useful for memory-constrained environments like WASM.
   * @return The root layer of the built layer tree, or nullptr if document is null or layout was
   *         not applied.
   */
  static std::shared_ptr<tgfx::Layer> Build(PAGXDocument* document, int maxImageDimension = 0);

  /**
   * Builds a layer tree and returns a mapping from PAGX Layer nodes to tgfx::Layer objects. This
   * mapping allows callers to look up the rendered layer for any PAGX Layer node.
   * @param document The document to build from. Must have had applyLayout() called.
   * @param maxImageDimension Maximum allowed image dimension (width or height). Images exceeding
   *                          this limit will be scaled down proportionally. A value of 0 means no
   *                          limit. Useful for memory-constrained environments like WASM.
   * @return A LayerBuildResult containing the root layer and a mapping from PAGX Layer nodes to
   *         their corresponding tgfx::Layer objects. Returns empty result if document is null or
   *         layout was not applied.
   */
  static LayerBuildResult BuildWithMap(PAGXDocument* document, int maxImageDimension = 0);
};

}  // namespace pagx
