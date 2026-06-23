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

namespace tgfx {
class Typeface;
}

namespace pagx {

class LayoutContext;

/**
 * Owns a font source (file path, system name, or pre-built tgfx::Typeface) and lazily loads the
 * underlying tgfx::Typeface on demand. This is the pagx-side font handle stored by FontConfig,
 * avoiding tgfx leakage through the public FontConfig API while preserving full font capabilities
 * (shaping, embedding, glyph lookup) for internal layout/render use.
 */
class PAGTypeface {
 public:
  /**
   * Creates from a font file path. The given fontFamily/fontStyle are used as the registration
   * name for FontConfig lookup; the underlying tgfx::Typeface is lazy-loaded on first use.
   * @param path The font file path.
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The font family name used for FontConfig lookup.
   * @param fontStyle The font style name used for FontConfig lookup.
   */
  static std::shared_ptr<PAGTypeface> MakeFromPath(const std::string& path, int ttcIndex,
                                                    const std::string& fontFamily,
                                                    const std::string& fontStyle);

  /**
   * Creates from a system font name. The underlying tgfx::Typeface is lazy-loaded on first use.
   * @param fontFamily The system font family name.
   * @param fontStyle The system font style name.
   */
  static std::shared_ptr<PAGTypeface> MakeFromName(const std::string& fontFamily,
                                                    const std::string& fontStyle);

  /**
   * Wraps a pre-built tgfx::Typeface. Advanced entry point — avoid in common use; prefer
   * MakeFromPath/MakeFromName. Use only when you need to inject a typeface built externally (e.g.
   * via tgfx::CustomTypefaceBuilder). The fontFamily/fontStyle are read from the typeface. Note:
   * typefaces from CustomTypefaceBuilder are render-only (no font tables), so they cannot serve
   * runtime text shaping via FontConfig.
   * @param typeface The pre-built tgfx::Typeface to wrap.
   */
  static std::shared_ptr<PAGTypeface> MakeFromTypeface(std::shared_ptr<tgfx::Typeface> typeface);

  /**
   * Returns the registration family name. For MakeFromPath/MakeFromName this is the name passed
   * in; for MakeFromTypeface this is read from the typeface.
   */
  const std::string& fontFamily() const;

  /**
   * Returns the registration style name. For MakeFromPath/MakeFromName this is the name passed
   * in; for MakeFromTypeface this is read from the typeface.
   */
  const std::string& fontStyle() const;

 private:
  friend class LayoutContext;

  // Returns the underlying tgfx::Typeface, lazy-loading on first call for MakeFromPath/MakeFromName.
  std::shared_ptr<tgfx::Typeface> getTypeface() const;

  enum class Source { Path, Name, Typeface };

  PAGTypeface(std::string path, int ttcIndex, std::string family, std::string style);
  explicit PAGTypeface(std::string family, std::string style);
  explicit PAGTypeface(std::shared_ptr<tgfx::Typeface> typeface);

  Source source;
  std::string path;
  int ttcIndex = 0;
  std::string _fontFamily;
  std::string _fontStyle;
  mutable std::shared_ptr<tgfx::Typeface> _typeface;
};

}  // namespace pagx
