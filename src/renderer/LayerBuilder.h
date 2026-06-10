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
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Channel.h"
#include "tgfx/layers/Layer.h"

namespace tgfx {
class Gradient;
}  // namespace tgfx

namespace pagx {

/**
 * Runtime color stop binding keeps the parent gradient and stop index for a ColorStop node.
 */
struct RuntimeColorStop {
  std::shared_ptr<tgfx::Gradient> gradient = nullptr;
  size_t index = 0;
};

/**
 * Runtime binding maps PAGX document nodes to the tgfx runtime objects created for one concrete
 * layer tree instance. Animation channel writers use it to update the right runtime object without
 * storing runtime state on document nodes.
 */
using RuntimeWriter = void (*)(void* object, const KeyValue& value, float mix);

struct RuntimeTarget {
  void setObject(std::shared_ptr<void> object) {
    this->object = std::move(object);
  }

  template <typename T>
  std::shared_ptr<T> getObject() const {
    return std::static_pointer_cast<T>(object);
  }

  void setWriter(const std::string& channel, RuntimeWriter writer) {
    if (!channel.empty() && writer != nullptr) {
      writers[channel] = writer;
    }
  }

  bool apply(const std::string& channel, const KeyValue& value, float mix) const {
    auto it = writers.find(channel);
    if (it == writers.end() || object == nullptr) {
      return false;
    }
    it->second(object.get(), value, mix);
    return true;
  }

 private:
  std::shared_ptr<void> object = nullptr;
  std::unordered_map<std::string, RuntimeWriter> writers = {};
};

struct RuntimeBinding {
  template <typename T>
  void set(const Node* node, std::shared_ptr<T> object) {
    if (node == nullptr || object == nullptr) {
      return;
    }
    auto& target = targets[node];
    target.setObject(std::move(object));
  }

  void setWriter(const Node* node, const std::string& channel, RuntimeWriter writer) {
    if (node == nullptr) {
      return;
    }
    targets[node].setWriter(channel, std::move(writer));
  }

  template <typename T>
  std::shared_ptr<T> get(const Node* node) const {
    auto it = targets.find(node);
    if (it == targets.end()) {
      return nullptr;
    }
    return it->second.getObject<T>();
  }

  bool apply(const Node* node, const std::string& channel, const KeyValue& value, float mix) const {
    auto it = targets.find(node);
    if (it == targets.end()) {
      return false;
    }
    return it->second.apply(channel, value, mix);
  }

 private:
  std::unordered_map<const Node*, RuntimeTarget> targets = {};
};

/**
 * Result of building PAGX nodes into a tgfx layer tree.
 */
struct LayerBuildResult {
  std::shared_ptr<tgfx::Layer> root = nullptr;
  RuntimeBinding binding = {};

  /**
   * Returns the tgfx layer corresponding to the specified PAGX Layer node, or nullptr if the node
   * was not rendered into this build result.
   */
  std::shared_ptr<tgfx::Layer> getLayer(const Layer* layer) const {
    return binding.get<tgfx::Layer>(layer);
  }
};

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * The document must have applyLayout() called before building.
 *
 * Thread safety: not safe to call Build / BuildWithMap concurrently on the same PAGXDocument.
 * The renderer lazily caches a built tgfx::Typeface on each Font node referenced by the document
 * on the first call, and the cache is updated without synchronization. Callers that share a
 * PAGXDocument across threads must serialize Build / BuildWithMap calls themselves (e.g. by
 * mutex or by rendering on a single dedicated thread).
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
   * @return A LayerBuildResult containing the root layer and runtime binding maps. Returns empty
   *         result if document is null or
   *         layout was not applied.
   */
  static LayerBuildResult BuildWithMap(PAGXDocument* document);

  /**
   * Builds a top-level layer tree for runtime use, stopping at Composition slot boundaries —
   * Layers whose `composition` field is non-null produce empty container tgfx layers without
   * recursing into Composition.layers. The runtime PAGFile uses this entry point and then
   * populates each slot with an independent PAGComposition build via BuildCompositionSubtree(),
   * keeping per-slot layerMaps isolated.
   * @param document The document to build from. Must have had applyLayout() called.
   */
  static LayerBuildResult BuildForRuntime(PAGXDocument* document);

  /**
   * Builds a Composition's child layer subtree into a fresh LayerBuildResult. Used by the runtime
   * PAGComposition slot to obtain its own per-slot binding, masks, and tgfx layer instances.
   * @param composition The Composition to build. Must reference layers from a document that has
   *                    had applyLayout() called.
   */
  static LayerBuildResult BuildCompositionSubtree(const Composition* composition);
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
