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

#include "SamplerState.h"

namespace tgfx {
static SamplerState::WrapMode TileModeToWrapMode(TileMode tileMode) {
  switch (tileMode) {
    case TileMode::Clamp:
      return SamplerState::WrapMode::Clamp;
    case TileMode::Repeat:
      return SamplerState::WrapMode::Repeat;
    case TileMode::Mirror:
      return SamplerState::WrapMode::MirrorRepeat;
    case TileMode::Decal:
      return SamplerState::WrapMode::ClampToBorder;
  }
}

SamplerState::SamplerState(TileMode tileMode) {
  wrapModeX = TileModeToWrapMode(tileMode);
  wrapModeY = wrapModeX;
}

SamplerState::SamplerState(TileMode tileModeX, TileMode tileModeY) {
  wrapModeX = TileModeToWrapMode(tileModeX);
  wrapModeY = TileModeToWrapMode(tileModeY);
}
}  // namespace tgfx
