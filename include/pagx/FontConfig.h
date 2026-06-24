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

#include <cstddef>
#include <memory>
#include <string>
#include "pagx/PAGFont.h"

namespace pagx {

/**
 * FontConfig manages registered and fallback typefaces for font lookup. This decouples font
 * configuration from text layout and layout computation, allowing multiple components
 * (LayoutContext, TextLayout, LayerBuilder) to share the same font registry. Font lookup is
 * performed by LayoutContext using the data stored here.
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
   * Registers a font from a file path with an explicit registration key. Use this when the
   * registration name comes from a PAGFont (e.g. returned by PAGXDocument::getRequiredFonts()).
   * @param font The PAGFont describing the registration (fontFamily, fontStyle).
   * @param path The font file path.
   * @param ttcIndex The face index within a TTC font collection.
   */
  void registerFont(const PAGFont& font, const std::string& path, int ttcIndex);

  /**
   * Registers a font from an in-memory buffer with an explicit registration key.
   * @param font The PAGFont describing the registration (fontFamily, fontStyle).
   * @param data Pointer to the font data.
   * @param length Size of the font data in bytes.
   * @param ttcIndex The face index within a TTC font collection.
   */
  void registerFont(const PAGFont& font, const void* data, size_t length, int ttcIndex);

  /**
   * Registers a font from a file path. When fontFamily is empty, the registration key is read
   * from the font file itself; otherwise the provided (fontFamily, fontStyle) is used.
   * @param path The font file path.
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The registration family name. Empty to read from the font file.
   * @param fontStyle The registration style name. Empty to read from the font file.
   */
  void registerFont(const std::string& path, int ttcIndex,
                    const std::string& fontFamily = "", const std::string& fontStyle = "");

  /**
   * Registers a font from an in-memory buffer. When fontFamily is empty, the registration key is
   * read from the font data itself; otherwise the provided (fontFamily, fontStyle) is used.
   * @param data Pointer to the font data.
   * @param length Size of the font data in bytes.
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The registration family name. Empty to read from the font data.
   * @param fontStyle The registration style name. Empty to read from the font data.
   */
  void registerFont(const void* data, size_t length, int ttcIndex,
                    const std::string& fontFamily = "", const std::string& fontStyle = "");

  /**
   * Adds a fallback font from a file path with an explicit (fontFamily, fontStyle). Used when a
   * character is not found in primary fonts.
   * @param font The PAGFont describing the fallback (fontFamily, fontStyle).
   * @param path The font file path.
   * @param ttcIndex The face index within a TTC font collection.
   */
  void addFallbackFont(const PAGFont& font, const std::string& path, int ttcIndex);

  /**
   * Adds a fallback font from an in-memory buffer with an explicit (fontFamily, fontStyle).
   * @param font The PAGFont describing the fallback (fontFamily, fontStyle).
   * @param data Pointer to the font data.
   * @param length Size of the font data in bytes.
   * @param ttcIndex The face index within a TTC font collection.
   */
  void addFallbackFont(const PAGFont& font, const void* data, size_t length, int ttcIndex);

  /**
   * Adds a fallback font from a file path. When fontFamily is empty, the (fontFamily, fontStyle)
   * is read from the font file itself; otherwise the provided values are used.
   * @param path The font file path.
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The fallback family name. Empty to read from the font file.
   * @param fontStyle The fallback style name. Empty to read from the font file.
   */
  void addFallbackFont(const std::string& path, int ttcIndex,
                       const std::string& fontFamily = "", const std::string& fontStyle = "");

  /**
   * Adds a fallback font from an in-memory buffer. When fontFamily is empty, the
   * (fontFamily, fontStyle) is read from the font data itself; otherwise the provided values are
   * used.
   * @param data Pointer to the font data.
   * @param length Size of the font data in bytes.
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The fallback family name. Empty to read from the font data.
   * @param fontStyle The fallback style name. Empty to read from the font data.
   */
  void addFallbackFont(const void* data, size_t length, int ttcIndex,
                       const std::string& fontFamily = "", const std::string& fontStyle = "");

  /**
   * Registers a system font by name. Returns false if the system does not have a matching font.
   * @param fontFamily The system font family name.
   * @param fontStyle The system font style name.
   */
  bool registerFont(const std::string& fontFamily, const std::string& fontStyle);

  /**
   * Registers a system font by name. Returns false if the system does not have a matching font.
   * @param font The PAGFont describing the system font (fontFamily, fontStyle).
   */
  bool registerFont(const PAGFont& font);

  /**
   * Adds a system fallback font by name. Returns false if the system does not have a matching
   * font.
   * @param fontFamily The system font family name.
   * @param fontStyle The system font style name.
   */
  bool addFallbackFont(const std::string& fontFamily, const std::string& fontStyle);

  /**
   * Adds a system fallback font by name. Returns false if the system does not have a matching
   * font.
   * @param font The PAGFont describing the system fallback (fontFamily, fontStyle).
   */
  bool addFallbackFont(const PAGFont& font);

 private:
  struct Data;
  std::unique_ptr<Data> data;
  friend class LayoutContext;
};

}  // namespace pagx
