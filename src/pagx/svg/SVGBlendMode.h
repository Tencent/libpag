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

#include <string>
#include "pagx/types/BlendMode.h"

namespace pagx {

struct SVGBlendModeEntry {
  BlendMode mode = BlendMode::Normal;
  const char* name = nullptr;
  // SVG `feBlend` only supports the 16 standard blend modes from CSS Compositing Level 1.
  // `plus-lighter` / `plus-darker` are Porter-Duff compositing operators added in Level 2;
  // they are valid for CSS `mix-blend-mode` but must NOT be emitted as an feBlend `mode`.
  bool feBlendSupported = true;
};

static constexpr SVGBlendModeEntry SVG_BLEND_MODES[] = {
    {BlendMode::Normal, "normal", true},
    {BlendMode::Multiply, "multiply", true},
    {BlendMode::Screen, "screen", true},
    {BlendMode::Overlay, "overlay", true},
    {BlendMode::Darken, "darken", true},
    {BlendMode::Lighten, "lighten", true},
    {BlendMode::ColorDodge, "color-dodge", true},
    {BlendMode::ColorBurn, "color-burn", true},
    {BlendMode::HardLight, "hard-light", true},
    {BlendMode::SoftLight, "soft-light", true},
    {BlendMode::Difference, "difference", true},
    {BlendMode::Exclusion, "exclusion", true},
    {BlendMode::Hue, "hue", true},
    {BlendMode::Saturation, "saturation", true},
    {BlendMode::Color, "color", true},
    {BlendMode::Luminosity, "luminosity", true},
    // `plus-lighter` is in CSS Compositing Level 2 and supported by Safari/WebKit and
    // Chromium 111+ (Mar 2023). Renders correctly in modern browsers.
    {BlendMode::PlusLighter, "plus-lighter", false},
    // `plus-darker` is in CSS Compositing Level 2 but currently only Safari/WebKit
    // implements it; Blink (Chrome/Edge/Electron) and Gecko (Firefox) parse the value
    // and silently fall back to `normal`. We still emit the spec-correct value because
    // libpag's own renderer and WebKit honor it, and there is no portable SVG construct
    // (feBlend has no plus-darker mode; feComposite arithmetic on BackgroundImage is
    // unsupported in Blink) that reproduces it across all engines.
    {BlendMode::PlusDarker, "plus-darker", false},
};

inline const char* BlendModeToSVGString(BlendMode mode) {
  for (const auto& entry : SVG_BLEND_MODES) {
    if (entry.mode == mode) {
      return entry.name;
    }
  }
  return nullptr;
}

// Returns the mode name suitable for SVG `feBlend mode=...`, or nullptr when the
// blend mode has no feBlend equivalent (e.g. plus-lighter / plus-darker).
inline const char* BlendModeToFEBlendString(BlendMode mode) {
  for (const auto& entry : SVG_BLEND_MODES) {
    if (entry.mode == mode) {
      return entry.feBlendSupported ? entry.name : nullptr;
    }
  }
  return nullptr;
}

inline BlendMode SVGBlendModeFromString(const std::string& str) {
  for (const auto& entry : SVG_BLEND_MODES) {
    if (str == entry.name) {
      return entry.mode;
    }
  }
  return BlendMode::Normal;
}

}  // namespace pagx
