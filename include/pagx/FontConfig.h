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

#include <memory>
#include <string>

namespace pagx {

class PAGTypeface;

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
   * Registers a PAGTypeface for exact (fontFamily, fontStyle) lookup. The lookup key is read
   * from the PAGTypeface's fontFamily()/fontStyle(). Use this for fonts explicitly provided by
   * the application.
   * @param typeface The PAGTypeface to register. Construct via PAGTypeface::MakeFromPath,
   *                 MakeFromName, or MakeFromTypeface.
   */
  void registerTypeface(std::shared_ptr<PAGTypeface> typeface);

  /**
   * Adds a fallback PAGTypeface used when a character is not found in the primary font
   * (either registered or system). Typefaces are tried in order.
   * @param typeface The fallback PAGTypeface.
   */
  void addFallbackTypeface(std::shared_ptr<PAGTypeface> typeface);

 private:
  struct Data;
  std::unique_ptr<Data> data;
  friend class LayoutContext;
};

}  // namespace pagx
