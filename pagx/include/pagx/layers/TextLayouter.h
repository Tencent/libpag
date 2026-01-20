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

#include <memory>
#include <string>
#include <vector>
#include "tgfx/core/Font.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

/**
 * TextLayouter provides simple text layout functionality for PAGX text rendering.
 * It handles font fallback and basic horizontal text layout.
 */
class TextLayouter {
 public:
  /**
   * Sets the fallback typefaces for PAGX text rendering.
   * When a character cannot be rendered by the primary typeface, the fallback typefaces
   * are tried in order until one that supports the character is found.
   */
  static void SetFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces);

  /**
   * Creates a TextBlob from the given text with basic horizontal layout.
   * Supports font fallback for characters not available in the primary typeface.
   */
  static std::shared_ptr<tgfx::TextBlob> Layout(const std::string& text, const tgfx::Font& font);

 private:
  static std::vector<std::shared_ptr<tgfx::Typeface>> GetFallbackTypefaces();
};

}  // namespace pagx
