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
#include "pagx/nodes/Point.h"

namespace pagx {

/**
 * TextSpan represents a text span that generates glyph paths for rendering. It defines the text
 * content, font properties, and positioning within a shape layer.
 */
class TextSpan : public Element {
 public:
  /**
   * The position of the text blob.
   */
  Point position = {};

  /**
   * The font family name.
   */
  std::string font = {};

  /**
   * The font size in pixels. The default value is 12.
   */
  float fontSize = 12;

  /**
   * The font weight, ranging from 100 to 900. The default value is 400 (normal).
   */
  int fontWeight = 400;

  /**
   * The font style (e.g., "normal", "italic", "oblique"). The default value is "normal".
   */
  std::string fontStyle = "normal";

  /**
   * The tracking value that adjusts spacing between characters. The default value is 0.
   */
  float tracking = 0;

  /**
   * The text content to render.
   */
  std::string text = {};

  NodeType nodeType() const override {
    return NodeType::TextSpan;
  }
};

}  // namespace pagx
