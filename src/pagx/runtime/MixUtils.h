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

#include "pagx/types/Color.h"
#include "tgfx/core/Color.h"

namespace pagx {

// Linear interpolation helpers used by continuous-channel writers. Discrete channels ignore mix
// and overwrite directly, so they don't go through these helpers.
inline float MixFloat(float current, float target, float mix) {
  return current + (target - current) * mix;
}

inline Color MixColor(const Color& current, const Color& target, float mix) {
  Color result = current;
  result.red = MixFloat(current.red, target.red, mix);
  result.green = MixFloat(current.green, target.green, mix);
  result.blue = MixFloat(current.blue, target.blue, mix);
  result.alpha = MixFloat(current.alpha, target.alpha, mix);
  result.colorSpace = target.colorSpace;
  return result;
}

inline tgfx::Color MixTGFXColor(const tgfx::Color& current, const tgfx::Color& target, float mix) {
  tgfx::Color result = current;
  result.red = MixFloat(current.red, target.red, mix);
  result.green = MixFloat(current.green, target.green, mix);
  result.blue = MixFloat(current.blue, target.blue, mix);
  result.alpha = MixFloat(current.alpha, target.alpha, mix);
  return result;
}

}  // namespace pagx
