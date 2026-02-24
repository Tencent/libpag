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

#include "GridBackground.h"
#include "tgfx/layers/LayerRecorder.h"

namespace pagx {

std::shared_ptr<GridBackgroundLayer> GridBackgroundLayer::Make(int width, int height,
                                                               float density) {
  return std::shared_ptr<GridBackgroundLayer>(new GridBackgroundLayer(width, height, density));
}

GridBackgroundLayer::GridBackgroundLayer(int width, int height, float density)
    : width(width), height(height), density(density) {
  invalidateContent();
}

void GridBackgroundLayer::onUpdateContent(tgfx::LayerRecorder* recorder) {
  tgfx::LayerPaint backgroundPaint(tgfx::Color::White());
  recorder->addRect(tgfx::Rect::MakeWH(static_cast<float>(width), static_cast<float>(height)),
                    backgroundPaint);

  tgfx::LayerPaint tilePaint(tgfx::Color{0.8f, 0.8f, 0.8f, 1.f});
  // Use fixed logical size (32px) so the grid looks the same on all screens.
  int logicalTileSize = 32;
  int tileSize = static_cast<int>(static_cast<float>(logicalTileSize) * density);
  if (tileSize <= 0) {
    tileSize = logicalTileSize;
  }
  for (int y = 0; y < height; y += tileSize) {
    bool draw = (y / tileSize) % 2 == 1;
    for (int x = 0; x < width; x += tileSize) {
      if (draw) {
        recorder->addRect(
            tgfx::Rect::MakeXYWH(static_cast<float>(x), static_cast<float>(y),
                                 static_cast<float>(tileSize), static_cast<float>(tileSize)),
            tilePaint);
      }
      draw = !draw;
    }
  }
}

}  // namespace pagx
