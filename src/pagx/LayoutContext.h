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
#include "TypefaceHolder.h"
#include "pagx/FontConfig.h"

namespace tgfx {
class Typeface;
}

namespace pagx {

/**
 * Internal layout context that provides font lookup capabilities during the layout phase.
 * Constructed by applyLayout from the document's FontConfig, passed through all layout methods.
 */
class LayoutContext {
 public:
  explicit LayoutContext(FontConfig* fontConfig);

  // Finds a typeface for the given family/style. Searches user-registered fonts first,
  // then falls back to system font lookup via MakeFromName.
  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle);

  // Finds a fallback typeface that contains the given codepoint. Searches user fallback
  // fonts first, then system fallback fonts. Typefaces are created on demand.
  std::shared_ptr<tgfx::Typeface> fallbackTypeface(int32_t codepoint,
                                                   const tgfx::Typeface* primaryTypeface);

  FontConfig* getFontConfig() const {
    return fontConfig;
  }

 private:
  void ensureSystemFallbacks();

  FontConfig* fontConfig = nullptr;
  std::vector<TypefaceHolder> systemFallbacks = {};
  bool systemFallbacksLoaded = false;
};

}  // namespace pagx
