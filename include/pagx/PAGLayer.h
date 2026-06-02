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
#include "pagx/types/Matrix.h"

namespace pagx {

class Layer;
class PAGFile;

/**
 * PAGLayer is a runtime handle to a single Layer node within a PAGComposition instance. It is
 * returned by hit testing to identify a layer under a point. A handle is tied to one runtime
 * instance: the same source Layer referenced by multiple compositions yields a distinct PAGLayer
 * per instance.
 */
class PAGLayer {
 public:
  ~PAGLayer();

  /**
   * Returns the display name of the source Layer this handle refers to. Returns an empty string if
   * the source layer is unavailable.
   */
  std::string getName() const;

  /**
   * Returns the matrix that maps this layer's local coordinate space to the surface coordinate
   * space. It combines the layer's accumulated transform within the runtime tree (including any
   * animation applied so far) with the display list's zoomScale and contentOffset. Use it to map
   * points or bounds from the layer's local space onto the surface. Returns the identity matrix if
   * the handle has no runtime layer or the surface mapping is unavailable.
   */
  Matrix getGlobalMatrix() const;

  /**
   * Checks whether this layer overlaps the given surface point. Surface coordinates are converted
   * to the runtime tree's root space before testing. shapeHitTest selects the actual shape (true)
   * or the bounding box (false). Returns false if the handle has no runtime layer or the surface
   * point cannot be mapped.
   * @param surfaceX the x coordinate in surface (device) space.
   * @param surfaceY the y coordinate in surface (device) space.
   * @param shapeHitTest whether to test against the actual shape (true) or the bounding box (false).
   */
  bool hitTestPoint(float surfaceX, float surfaceY, bool shapeHitTest = false);

 private:
  PAGLayer();

  // Creates a PAGLayer handle for the given source Layer node, the runtime tgfx layer it maps to in
  // a specific composition instance, and the root PAGFile used for surface coordinate conversion
  // and for resolving the tree root. Returns nullptr if node is null. The runtimeLayer is passed as
  // void* to keep tgfx out of this public header; PAGLayer.cpp casts it back. Used by
  // PAGComposition hit testing.
  static std::shared_ptr<PAGLayer> Wrap(const Layer* node, const void* runtimeLayer,
                                        PAGFile* rootFile);

  struct Impl;
  std::unique_ptr<Impl> impl;

  friend class PAGComposition;
};

}  // namespace pagx
