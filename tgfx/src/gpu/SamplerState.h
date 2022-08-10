/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/TileMode.h"

namespace tgfx {
/**
 * Represents the tile modes used to access a texture.
 */
class SamplerState {
 public:
  enum class WrapMode {
    Clamp,
    Repeat,
    MirrorRepeat,
    ClampToBorder,
  };

  SamplerState() = default;

  explicit SamplerState(TileMode tileMode);

  SamplerState(TileMode tileModeX, TileMode tileModeY);

  SamplerState(WrapMode wrapModeX, WrapMode wrapModeY)
      : wrapModeX(wrapModeX), wrapModeY(wrapModeY) {
  }

  WrapMode wrapModeX = WrapMode::Clamp;
  WrapMode wrapModeY = WrapMode::Clamp;
};
}  // namespace tgfx
