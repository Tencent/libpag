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

enum class BackgroundMode {
  Grid,
  White,
  Black,
};

class GridBackgroundLayer : public tgfx::Layer {
 public:
  static std::shared_ptr<GridBackgroundLayer> Make(int width, int height, float density,
                                                    BackgroundMode mode = BackgroundMode::Grid);

  void setBackgroundMode(BackgroundMode mode);

 protected:
  void onUpdateContent(tgfx::LayerRecorder* recorder) override;

 private:
  GridBackgroundLayer(int width, int height, float density, BackgroundMode mode);

  int width = 0;
  int height = 0;
  float density = 1.f;
  BackgroundMode mode = BackgroundMode::Grid;
};

}  // namespace pagx
