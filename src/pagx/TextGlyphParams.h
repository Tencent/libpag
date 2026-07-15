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

namespace pagx {

// Per-Text shaping attributes consumed by the shaper. Produced from a Text node's document fields,
// or supplied by a runtime holder with ViewModel-driven overrides applied. The shaper reads only
// this struct, never the Text node, so text content can be reshaped without mutating the document.
// Container-level layout parameters (box size, alignment, writing mode, baseline, textScale) stay
// in TextLayoutParams; this struct holds only the fields that vary per Text element.
struct TextGlyphParams {
  std::string text;
  std::string fontFamily;
  std::string fontStyle;
  float fontSize = 12.0f;
  float letterSpacing = 0.0f;
  bool fauxBold = false;
  bool fauxItalic = false;
};

}  // namespace pagx
