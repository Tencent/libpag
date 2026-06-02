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
#include "pagx/nodes/Property.h"
#include "tgfx/layers/Layer.h"

namespace tgfx {
class BlurFilter;
class DropShadowFilter;
class DropShadowStyle;
class FillStyle;
class Gradient;
class Image;
class ImagePattern;
class SolidColor;
class StrokeStyle;
class Text;
}  // namespace tgfx

namespace pagx {

class BlurFilter;
class ColorStop;
class DropShadowFilter;
class DropShadowStyle;
class Fill;
class Gradient;
class Image;
class ImagePattern;
class SolidColor;
class Stroke;
class Text;

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

  // Records the reverse mapping from a rendered tgfx::Layer back to its source PAGX node. Used by
  // hit testing to resolve a hit tgfx layer to the PAGX Layer node that produced it.
  void mapLayer(const tgfx::Layer* tgfxLayer, const Node* node) {
    if (tgfxLayer != nullptr && node != nullptr) {
      layerToNode[tgfxLayer] = node;
    }
  }

  // Returns the source PAGX node for a rendered tgfx::Layer, or nullptr if the layer is an internal
  // sub-layer that was not registered as a node's primary layer.
  const Node* nodeForLayer(const tgfx::Layer* tgfxLayer) const {
    auto it = layerToNode.find(tgfxLayer);
    return it == layerToNode.end() ? nullptr : it->second;
  }

 private:
  std::unordered_map<const Node*, RuntimeTarget> targets = {};
  std::unordered_map<const tgfx::Layer*, const Node*> layerToNode = {};
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
   * Builds a top-level layer tree but stops at Composition slot boundaries — Layers whose
   * `composition` field is non-null produce empty container tgfx layers without recursing into
   * Composition.layers. The runtime PAGFile uses this entry point and then populates each slot
   * with an independent PAGComposition build via BuildCompositionSubtree(), keeping per-slot
   * layerMaps isolated.
   * @param document The document to build from. Must have had applyLayout() called.
   */
  static LayerBuildResult BuildWithSlotsHandedOff(PAGXDocument* document);

  /**
   * Builds a Composition's child layer subtree into a fresh LayerBuildResult. Used by the runtime
   * PAGComposition slot to obtain its own per-slot binding, masks, and tgfx layer instances.
   * @param composition The Composition to build. Must reference layers from a document that has
   *                    had applyLayout() called.
   */
  static LayerBuildResult BuildCompositionSubtree(const Composition* composition);
};

}  // namespace pagx
