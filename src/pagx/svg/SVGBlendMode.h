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

#include <cstring>
#include <string>
#include "pagx/types/BlendMode.h"

namespace pagx {

struct SVGBlendModeEntry {
  BlendMode mode;
  const char* name;
};

static constexpr SVGBlendModeEntry kSVGBlendModes[] = {
    {BlendMode::Normal, "normal"},
    {BlendMode::Multiply, "multiply"},
    {BlendMode::Screen, "screen"},
    {BlendMode::Overlay, "overlay"},
    {BlendMode::Darken, "darken"},
    {BlendMode::Lighten, "lighten"},
    {BlendMode::ColorDodge, "color-dodge"},
    {BlendMode::ColorBurn, "color-burn"},
    {BlendMode::HardLight, "hard-light"},
    {BlendMode::SoftLight, "soft-light"},
    {BlendMode::Difference, "difference"},
    {BlendMode::Exclusion, "exclusion"},
    {BlendMode::Hue, "hue"},
    {BlendMode::Saturation, "saturation"},
    {BlendMode::Color, "color"},
    {BlendMode::Luminosity, "luminosity"},
};

inline const char* BlendModeToSVGString(BlendMode mode) {
  for (const auto& entry : kSVGBlendModes) {
    if (entry.mode == mode) {
      return entry.name;
    }
  }
  return nullptr;
}

inline BlendMode SVGBlendModeFromString(const std::string& str) {
  for (const auto& entry : kSVGBlendModes) {
    if (str == entry.name) {
      return entry.mode;
    }
  }
  return BlendMode::Normal;
}

}  // namespace pagx