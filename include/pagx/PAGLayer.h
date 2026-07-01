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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/types/Matrix.h"
#include "pagx/types/Rect.h"

namespace tgfx {
class Layer;
}  // namespace tgfx

namespace pagx {

class Layer;
class PAGComposition;
class PAGScene;

/**
 * Identifies the concrete runtime type of a PAGLayer in the runtime layer hierarchy, so callers can
 * downcast without RTTI. Layer is a plain runtime layer (may carry children), Composition adds
 * animation playback.
 */
enum class LayerType { Layer, Composition };

/**
 * PAGLayer is the base class of the runtime layer hierarchy. One PAGLayer node exists per source
 * layer of a running composition, mirroring the document's layer tree. It holds the rendered
 * content, its children, and advances the animations declared on its owning composition.
 * When the same source layer appears in multiple composition instances, each instance owns its own
 * PAGLayer node with its own on-screen position.
 */
class PAGLayer : public std::enable_shared_from_this<PAGLayer> {
 public:
  virtual ~PAGLayer();

  /**
   * Returns the concrete runtime type of this layer. Use it to downcast safely (via static_cast)
   * to PAGComposition without runtime type information.
   */
  virtual LayerType layerType() const;

  /**
   * Returns the display name of the layer. Returns an empty string if the layer has no name.
   */
  std::string name() const;

  /**
   * Returns the unique identifier of the source node this layer was built from, matching the node's
   * "id" attribute in the PAGX document. Returns an empty string if the source node has no id (for
   * example, the root composition or a node created without one).
   */
  std::string id() const;

  /**
   * Returns the matrix that maps this layer's local coordinate space to the surface coordinate
   * space, reflecting the layer's current on-screen position (including any animation applied so
   * far). Use it to map points or bounds from the layer onto the surface. Returns the identity
   * matrix if the mapping is unavailable.
   */
  Matrix getGlobalMatrix() const;

  /**
   * Returns the layer's bounds in its own local coordinate space, before its own transform is
   * applied. Returns an empty rectangle if the bounds are unavailable. For bounds in surface
   * coordinates, use PAGScene::getGlobalBounds.
   */
  Rect getBounds() const;

  /**
   * Returns whether this layer overlaps the given surface point. pixelHitTest selects the actual
   * shape (true) or the bounding box (false).
   * @param surfaceX the x coordinate in surface (device) space.
   * @param surfaceY the y coordinate in surface (device) space.
   * @param pixelHitTest whether to test against the actual shape (true) or the bounding box (false).
   */
  bool hitTestPoint(float surfaceX, float surfaceY, bool pixelHitTest = false);

  /**
   * Advances the animations owned by this layer and its descendants by the given amount of time.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   */
  virtual void advance(int64_t deltaMicroseconds);

  /**
   * Applies the current state of the animations owned by this layer and its descendants to the
   * displayed content.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  virtual void apply(float mix = 1.0f);

  /**
   * Returns the source Layer node associated with this runtime layer, or nullptr for the root
   * composition.
   */
  const Layer* getNode() const {
    return node;
  }

  /**
   * Returns the runtime children of this layer. PAGComposition children are PAGComposition
   * instances; plain PAGLayer children are the layers nested under this container.
   */
  const std::vector<std::shared_ptr<PAGLayer>>& getChildren() const {
    return children;
  }

 protected:
  // Constructs a runtime layer node for the given source Layer node, the runtime tgfx layer it maps
  // to in a specific composition instance, and the root PAGScene used for surface coordinate
  // conversion and for resolving the tree root. node may be null for the root composition (which
  // has no owning source layer). Created by the PAGComposition/PAGScene factories.
  PAGLayer(const Layer* node, std::shared_ptr<tgfx::Layer> runtimeLayer,
           const std::shared_ptr<PAGScene>& scene);

  const Layer* node = nullptr;
  std::shared_ptr<tgfx::Layer> runtimeLayer = nullptr;
  std::vector<std::shared_ptr<PAGLayer>> children = {};

  /**
   * Traverses the runtime layer subtree and calls visitor for every PAGComposition node found.
   * Visits descendants only and does NOT invoke the visitor on this node itself (even if this is
   * a PAGComposition). PAGComposition overrides this to include itself — after visiting all of
   * its descendants, it calls visitor(this). This ensures every composition in the tree is visited
   * exactly once, including the node on which the traversal is started.
   */
  virtual void forEachComposition(void (*visitor)(PAGComposition*));

  std::weak_ptr<PAGScene> rootScene = {};

  friend class PAGScene;
  friend class PAGComposition;
};

}  // namespace pagx
