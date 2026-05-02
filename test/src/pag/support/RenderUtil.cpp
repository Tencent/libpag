// Copyright (C) 2026 Tencent. All rights reserved.
//
// See RenderUtil.h for contract. Body kept under 25 lines so reviewers can
// eyeball it against the test/src/PAGXTest.cpp:264-270 reference without
// cross-checking.

#include "RenderUtil.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Surface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"

namespace pagx::test {

std::shared_ptr<tgfx::Surface> RenderLayerToSurface(tgfx::Context* context,
                                                    std::shared_ptr<tgfx::Layer> layer,
                                                    int canvasWidth, int canvasHeight,
                                                    float scale) {
  if (context == nullptr || layer == nullptr || canvasWidth <= 0 || canvasHeight <= 0) {
    return nullptr;
  }
  auto surface = tgfx::Surface::Make(context, canvasWidth, canvasHeight);
  if (surface == nullptr) {
    return nullptr;
  }
  // Wrapping `layer` in a dedicated container keeps the caller's layer free of
  // our scale matrix mutation — important because the same `layer` may be
  // handed back to a second RenderLayerToSurface call in self-consistency
  // smoke tests, and a leftover matrix would silently alter the second
  // render.
  auto container = tgfx::Layer::Make();
  if (scale != 1.0f) {
    container->setMatrix(tgfx::Matrix::MakeScale(scale, scale));
  }
  container->addChild(layer);
  tgfx::DisplayList displayList;
  displayList.root()->addChild(container);
  displayList.render(surface.get(), false);
  return surface;
}

}  // namespace pagx::test
