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

#include "Graphic.h"
#include "pag/file.h"
#include "rendering/filters/Transform3DFilter.h"

namespace pag {
class PAGComposition;

struct Transform3DSceneLayer {
  std::shared_ptr<Graphic> graphic;
  Transform3DDepthPlane depthPlane;
  BlendMode blendMode = BlendMode::Normal;
};

/** Returns the 3D scene graphic currently cached by the composition, which tests use to verify
 * reuse and invalidation; returns null when nothing is cached. */
std::shared_ptr<Graphic> GetCached3DGraphic(const PAGComposition* composition);

class Transform3DSceneBuilder {
 public:
  Transform3DSceneBuilder(int width, int height) : width(width), height(height) {
  }

  /** Adds a layer. 2D content separates consecutive 3D scenes while keeping the background
   * blending relationship. */
  void addLayer(std::shared_ptr<Graphic> graphic, Layer* layer, Frame frame,
                const std::shared_ptr<Transform3DContext>& context,
                const tgfx::Matrix& extraMatrix);

  /** Finishes the last 3D scene and returns the composed graphic. */
  std::shared_ptr<Graphic> makeGraphic();

 private:
  void flushScene();
  int width = 0;
  int height = 0;
  std::vector<std::shared_ptr<Graphic>> background;
  std::vector<Transform3DSceneLayer> layers;
};

}  // namespace pag
