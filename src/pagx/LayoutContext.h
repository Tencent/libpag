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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "TypefaceHolder.h"
#include "pagx/FontConfig.h"
#include "pagx/types/LayoutRounding.h"
#include "pagx/utils/CSSFontStyle.h"

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

  LayoutRounding layoutRounding = LayoutRounding::Pixel;

  float roundLayoutPosition(float value) const {
    return layoutRounding == LayoutRounding::None ? value : std::round(value);
  }

  float roundLayoutSize(float value) const {
    return layoutRounding == LayoutRounding::None ? value : std::ceil(value);
  }

  // Finds a typeface for the given family/style. Searches user-registered fonts first,
  // then falls back to system font lookup via MakeFromName.
  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle, int32_t codepoint = -1,
                                               float size = 12.0f);
  // CSS selection returns a document registration key when one exists, preserving aliases.
  std::shared_ptr<tgfx::Typeface> matchTypeface(const std::string& family,
                                                const CSSFontStyle& request,
                                                std::string* resolvedStyle = nullptr,
                                                float size = 12.0f);

  using TypefacePredicate = std::function<bool(const std::shared_ptr<tgfx::Typeface>&)>;
  std::shared_ptr<tgfx::Typeface> findClusterTypeface(const std::string& family,
                                                      const std::string& style, float size,
                                                      const std::vector<int32_t>& codepoints,
                                                      const TypefacePredicate& accepts);
  std::shared_ptr<tgfx::Typeface> fallbackTypeface(const TypefacePredicate& accepts, float size);

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
  // System lookups are cached so that every text that asks for the same family and style shares
  // one typeface instance. Glyph collection keys glyphs by typeface identity, so a fresh instance
  // per text node would store the same outline once per text.
  std::unordered_map<std::string, std::shared_ptr<tgfx::Typeface>> systemTypefaceCache = {};
};

// Layout recursively enters child containers during both measurement and allocation. Restore the
// enclosing policy on every exit so an override never affects siblings or other compositions.
class ScopedLayoutRounding {
 public:
  ScopedLayoutRounding(LayoutContext* context, LayoutRounding rounding) : context(context) {
    if (context) {
      previous = context->layoutRounding;
      if (rounding != LayoutRounding::Inherit) context->layoutRounding = rounding;
    }
  }
  ~ScopedLayoutRounding() {
    if (context) context->layoutRounding = previous;
  }
  ScopedLayoutRounding(const ScopedLayoutRounding&) = delete;
  ScopedLayoutRounding& operator=(const ScopedLayoutRounding&) = delete;

 private:
  LayoutContext* context;
  LayoutRounding previous = LayoutRounding::Pixel;
};

}  // namespace pagx
