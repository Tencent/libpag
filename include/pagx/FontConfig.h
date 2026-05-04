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
class Typeface;
}

namespace pagx {

/**
 * FontConfig manages registered and fallback typefaces for font lookup.
 * This decouples font configuration from text layout and layout computation,
 * allowing multiple components (LayoutContext, TextLayout, LayerBuilder) to share the same font
 * registry. Font lookup is performed by LayoutContext using the data stored here.
 */
class FontConfig {
 public:
  FontConfig();
  ~FontConfig();
  FontConfig(const FontConfig& other);
  FontConfig& operator=(const FontConfig& other);
  FontConfig(FontConfig&& other) noexcept;
  FontConfig& operator=(FontConfig&& other) noexcept;

  /**
   * Registers a typeface for exact family+style lookup.
   * Typically used for fonts explicitly provided by the application.
   */
  void registerTypeface(std::shared_ptr<tgfx::Typeface> typeface);

  /**
   * Adds fallback typefaces used when a character is not found in the primary font
   * (either registered or system). Typefaces are tried in order.
   */
  void addFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces);

  /**
   * Adds a deferred fallback font that will be loaded on demand when needed.
   * Supports both file-based loading and name-based system font lookup.
   * @param path The font file path (empty if using fontFamily for name-based loading).
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The font family name for matching and name-based loading.
   * @param fontStyle The font style name (e.g. "Regular", "Bold").
   */
  void addFallbackFont(const std::string& path, int ttcIndex, const std::string& fontFamily,
                       const std::string& fontStyle);

  /**
   * Looks up a typeface by family + style. Tries in order:
   *   (1) exact family+style match among registered typefaces
   *   (2) family-only match among registered typefaces (prefers Regular/Medium/Normal)
   *   (3) family match among user fallback typefaces (may trigger lazy load)
   * Returns nullptr if nothing matches. Does NOT consult system fonts — callers that need
   * platform system lookup should fall through to tgfx::Typeface::MakeFromName themselves.
   * Non-const because step (3) may lazy-load a deferred fallback font.
   */
  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle);

  /**
   * Returns all user fallback typefaces in registration order. Lazy-loads deferred entries; any
   * holder that fails to resolve is skipped. Useful for wiring a FontProvider (pagx/pag) that
   * serves the same fallback chain used by the layout engine.
   */
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces();

 private:
  struct Data;
  std::unique_ptr<Data> data;
  friend class LayoutContext;
};

}  // namespace pagx
