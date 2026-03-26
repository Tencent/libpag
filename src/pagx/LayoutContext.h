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

namespace tgfx {
class Font;
class Typeface;
}  // namespace tgfx

namespace pagx {

class FontConfig;

/**
 * Internal layout context that provides font lookup capabilities during the layout phase.
 * Constructed by applyLayout from the document's FontConfig, passed through all layout methods.
 */
class LayoutContext {
 public:
  explicit LayoutContext(FontConfig* fontConfig);

  /** Finds a typeface matching the given family and style (5-level fallback strategy). */
  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle) const;

  /** Builds a list of fallback fonts for character-level fallback during text shaping. */
  std::vector<tgfx::Font> getFallbackFonts(float fontSize, bool fauxBold, bool fauxItalic,
                                           const tgfx::Typeface* primaryTypeface) const;

  /** Returns the underlying FontConfig pointer. */
  FontConfig* getFontConfig() const {
    return fontConfig;
  }

 private:
  FontConfig* fontConfig = nullptr;
};

}  // namespace pagx
