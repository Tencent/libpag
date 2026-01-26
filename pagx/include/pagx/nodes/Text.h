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

#include <string>
#include "pagx/nodes/Element.h"
#include "tgfx/core/Point.h"

namespace pagx {

/**
 * Text represents a text geometry element that generates glyphs for rendering. It defines the text
 * content and font properties.
 */
class Text : public Element {
 public:
  /**
   * The position of the text origin (x, y where y is the baseline). Can be overridden by
   * TextLayout. The default value is (0, 0).
   */
  tgfx::Point position = {};

  /**
   * The font family name (e.g., "Arial", "Helvetica").
   */
  std::string fontFamily = "";

  /**
   * The font style/variant name (e.g., "Regular", "Bold", "Italic", "Bold Italic"). This
   * corresponds to the specific font file variant. The default value is "Regular".
   */
  std::string fontStyle = "";

  /**
   * The font size in pixels. The default value is 12.
   */
  float fontSize = 12;

  /**
   * The letter spacing (tracking) value that adjusts spacing between characters. The default value
   * is 0.
   */
  float letterSpacing = 0;

  /**
   * The baseline shift for superscript/subscript effects. Positive values shift up, negative values
   * shift down. The default value is 0.
   */
  float baselineShift = 0;

  /**
   * The text content to render. Supports newline characters (\n) for line breaks, which use a
   * default line height of 1.2 times the font size.
   */
  std::string text = {};

  NodeType nodeType() const override {
    return NodeType::Text;
  }
};

}  // namespace pagx
