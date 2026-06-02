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
 * PAGLayer is a handle to one layer of a running composition, returned by hit testing to identify
 * the layer under a point. When the same source layer appears in multiple composition instances,
 * each instance yields its own PAGLayer handle with its own on-screen position.
 */
class PAGLayer {
 public:
  ~PAGLayer();

  /**
   * Returns the display name of the layer. Returns an empty string if the layer has no name.
   */
  std::string getName() const;

  /**
   * Returns the matrix that maps this layer's local coordinate space to the surface coordinate
   * space, reflecting the layer's current on-screen position (including any animation applied so
   * far). Use it to map points or bounds from the layer onto the surface. Returns the identity
   * matrix if the mapping is unavailable.
   */
  Matrix getGlobalMatrix() const;

  /**
   * Returns whether this layer overlaps the given surface point. shapeHitTest selects the actual
   * shape (true) or the bounding box (false).
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
