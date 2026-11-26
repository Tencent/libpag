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

#include "utils/PAGXTestUtils.h"
#include <cmath>
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Bitmap.h"
#include "utils/TestUtils.h"

namespace pag {

tgfx::Bitmap RenderTargetView(const std::shared_ptr<pagx::PAGXDocument>& document) {
  auto scene = pagx::PAGScene::Make(document);
  EXPECT_NE(scene, nullptr);
  if (scene == nullptr) {
    return {};
  }
  auto outputBounds = scene->getOutputBounds();
  EXPECT_TRUE(outputBounds.has_value());
  if (!outputBounds.has_value()) {
    return {};
  }
  int width = static_cast<int>(ceilf(outputBounds->width));
  int height = static_cast<int>(ceilf(outputBounds->height));
  auto surface = pagx::PAGSurface::MakeOffscreen(width, height);
  EXPECT_NE(surface, nullptr);
  if (surface == nullptr) {
    return {};
  }
  bool drawn = scene->draw(surface);
  EXPECT_TRUE(drawn);
  if (!drawn) {
    return {};
  }
  return MakeSnapshot(surface);
}

}  // namespace pag
