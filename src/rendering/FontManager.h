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

#include <unordered_map>
#include "pag/pag.h"
#include "tgfx/core/Typeface.h"

namespace pag {
class TypefaceHolder {
 public:
  static std::shared_ptr<TypefaceHolder> MakeFromName(const std::string& fontFamily,
                                                      const std::string& fontStyle);

  static std::shared_ptr<TypefaceHolder> MakeFromFile(const std::string& fontPath,
                                                      int ttcIndex = 0);

  std::shared_ptr<tgfx::Typeface> getTypeface();

 private:
  std::string fontFamily;
  std::string fontStyle;
  std::string fontPath;
  int ttcIndex = 0;
  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
};

class FontManager {
 public:
  static std::shared_ptr<tgfx::Typeface> GetTypefaceWithoutFallback(const std::string& fontFamily,
                                                                    const std::string& fontStyle);
  static std::shared_ptr<tgfx::Typeface> GetFallbackTypeface(const std::string& name,
                                                             tgfx::GlyphID* glyphID);

  static PAGFont RegisterFont(const std::string& fontPath, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle);

  static PAGFont RegisterFont(const void* data, size_t length, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle);

  static void UnregisterFont(const PAGFont& font);

  static void SetFallbackFontNames(const std::vector<std::string>& fontNames);

  static void SetFallbackFontPaths(const std::vector<std::string>& fontPaths,
                                   const std::vector<int>& ttcIndices);

  ~FontManager();

  bool hasFallbackFonts();

 private:
  PAGFont registerFont(const std::string& fontPath, int ttcIndex = 0,
                       const std::string& fontFamily = "", const std::string& fontStyle = "");

  PAGFont registerFont(const void* data, size_t length, int ttcIndex = 0,
                       const std::string& fontFamily = "", const std::string& fontStyle = "");

  PAGFont registerFont(std::shared_ptr<tgfx::Typeface> typeface, const std::string& fontFamily = "",
                       const std::string& fontStyle = "");

  void unregisterFont(const PAGFont& font);

  std::shared_ptr<tgfx::Typeface> getTypefaceWithoutFallback(const std::string& fontFamily,
                                                             const std::string& fontStyle);

  std::shared_ptr<tgfx::Typeface> getFallbackTypeface(const std::string& name,
                                                      tgfx::GlyphID* glyphID);

  void setFallbackFontNames(const std::vector<std::string>& fontNames);

  void setFallbackFontPaths(const std::vector<std::string>& fontPaths,
                            const std::vector<int>& ttcIndices);

  std::unordered_map<std::string, std::shared_ptr<tgfx::Typeface>> registeredFontMap;
  std::vector<std::shared_ptr<TypefaceHolder>> fallbackFontList;
  std::mutex locker = {};

  std::shared_ptr<tgfx::Typeface> getTypefaceFromCache(const std::string& fontFamily,
                                                       const std::string& fontStyle);

  friend class PAGFont;
};

}  // namespace pag
