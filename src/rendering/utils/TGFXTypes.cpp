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

#include "TGFXTypes.h"

namespace pag {
static constexpr std::pair<Enum, Blend> kBlendModeMap[] = {
    {BlendMode::Normal, Blend::SrcOver},
    {BlendMode::Multiply, Blend::Multiply},
    {BlendMode::Screen, Blend::Screen},
    {BlendMode::Overlay, Blend::Overlay},
    {BlendMode::Darken, Blend::Darken},
    {BlendMode::Lighten, Blend::Lighten},
    {BlendMode::ColorDodge, Blend::ColorDodge},
    {BlendMode::ColorBurn, Blend::ColorBurn},
    {BlendMode::HardLight, Blend::HardLight},
    {BlendMode::SoftLight, Blend::SoftLight},
    {BlendMode::Difference, Blend::Difference},
    {BlendMode::Exclusion, Blend::Exclusion},
    {BlendMode::Hue, Blend::Hue},
    {BlendMode::Saturation, Blend::Saturation},
    {BlendMode::Color, Blend::Color},
    {BlendMode::Luminosity, Blend::Luminosity},
    {BlendMode::Add, Blend::Plus}};

Blend ToTGFXBlend(Enum blendMode) {
  for (const auto& pair : kBlendModeMap) {
    if (pair.first == blendMode) {
      return pair.second;
    }
  }
  return Blend::SrcOver;
}

Stroke::Cap ToTGFXCap(Enum cap) {
  switch (cap) {
    case LineCap::Round:
      return Stroke::Cap::Round;
    case LineCap::Square:
      return Stroke::Cap::Square;
    default:
      return Stroke::Cap::Butt;
  }
}

Stroke::Join ToTGFXJoin(Enum join) {
  switch (join) {
    case LineJoin::Round:
      return Stroke::Join::Round;
    case LineJoin::Bevel:
      return Stroke::Join::Bevel;
    default:
      return Stroke::Join::Miter;
  }
}

Color4f ToTGFXColor(Color color, Opacity opacity) {
  return {static_cast<float>(color.red) / 255.0f, static_cast<float>(color.green) / 255.0f,
          static_cast<float>(color.blue) / 255.0f, ToAlpha(opacity)};
}

float ToAlpha(Opacity opacity) {
  if (opacity == 255) {
    return 1.0f;
  }
  return static_cast<float>(opacity) / 255.0f;
}

}  // namespace pag
