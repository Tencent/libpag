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
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * Result of building a layer tree, containing the root layer and a mapping from PAGX Layer
 * nodes to their corresponding tgfx::Layer objects.
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
   * @return The root layer of the built layer tree, or nullptr if document is null or layout was
   *         not applied.
   */
  static std::shared_ptr<tgfx::Layer> Build(PAGXDocument* document);

  /**
   * Builds a layer tree and returns a mapping from PAGX Layer nodes to tgfx::Layer objects. This
   * mapping allows callers to look up the rendered layer for any PAGX Layer node.
   * @param document The document to build from. Must have had applyLayout() called.
   * @return A LayerBuildResult containing the root layer and a mapping from PAGX Layer nodes to
   *         their corresponding tgfx::Layer objects. Returns empty result if document is null or
   *         layout was not applied.
   */
  static LayerBuildResult BuildWithMap(PAGXDocument* document);
};

/**
 * LayerBuilderSession wraps LayerBuilder with stateful behavior: it retains the build context
 * after build() returns so callers can later re-generate a layer's contents when an underlying
 * Image resource changes (e.g. progressive upgrade from a low-resolution thumbnail to a full
 * version). The class is intended for the progressive image loading flow on WeChat; other
 * platforms should keep using the static LayerBuilder::Build / BuildWithMap entry points.
 *
 * Lifecycle: create a session, call build() once per document (matching parsePAGX+buildLayers
 * cycle), and then call rebuildForFilePath() whenever a new decoded image has been attached to
 * the document via PAGXDocument::loadDecodedImage(). Destroying the session releases all cached
 * layer/image state.
 */
class LayerBuilderSession {
 public:
  LayerBuilderSession();
  ~LayerBuilderSession();

  LayerBuilderSession(const LayerBuilderSession&) = delete;
  LayerBuilderSession& operator=(const LayerBuilderSession&) = delete;

  /**
   * Builds the layer tree for the given document. Behaves identically to
   * LayerBuilder::BuildWithMap() but keeps the internal context alive for later rebuilds.
   * @return The LayerBuildResult (root layer + pagx Layer to tgfx Layer mapping). The mapping
   *         reflects the first tgfx layer produced for each pagx Layer; callers that need all
   *         copies (for example to refresh every Composition instance that shares a Layer)
   *         should use getTgfxLayers() instead.
   */
  LayerBuildResult build(PAGXDocument* document);

  /**
   * Rebuilds the tgfx vector contents of every layer whose fill/stroke references an
   * ImagePattern backed by an Image node whose filePath matches the given value. Call this
   * after PAGXDocument::loadDecodedImage() so the pending ImagePattern nodes pick up the new
   * tgfx::Image. A single filePath may match multiple Image nodes and each Image node may be
   * referenced by multiple Layers, which in turn may have been duplicated by Composition
   * instancing; all such copies are refreshed in one call.
   * @return The number of tgfx layers whose contents were regenerated. Zero means no layer
   *         currently references the given filePath (or the document has not been built yet).
   */
  size_t rebuildForFilePath(const std::string& filePath);

  /**
   * Returns every tgfx::Layer that was produced for the given pagx Layer during build(). A
   * pagx Layer may map to several tgfx layers when its owning Composition is instanced more
   * than once; the returned vector preserves build order.
   * @return An empty vector when the pagx Layer was not seen during build() (e.g. nullptr,
   *         a mask-only layer that got skipped, or no build() call has been made yet).
   */
  std::vector<std::shared_ptr<tgfx::Layer>> getTgfxLayers(const Layer* pagxLayer) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace pagx
