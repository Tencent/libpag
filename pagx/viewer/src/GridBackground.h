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

#include "tgfx/layers/Layer.h"

namespace pagx {

class GridBackgroundLayer : public tgfx::Layer {
 public:
  static std::shared_ptr<GridBackgroundLayer> Make(int width, int height, float density);

 protected:
  void onUpdateContent(tgfx::LayerRecorder* recorder) override;

 private:
  GridBackgroundLayer(int width, int height, float density);

  int width = 0;
  int height = 0;
  float density = 1.f;
};

void DrawBackground(tgfx::Canvas* canvas, int width, int height, float density);

}  // namespace pagx
