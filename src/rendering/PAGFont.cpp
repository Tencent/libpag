/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "pag/pag.h"
#include "rendering/FontManager.h"

namespace pag {
PAGFont PAGFont::RegisterFont(const std::string& fontPath, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle) {
  return FontManager::RegisterFont(fontPath, ttcIndex, fontFamily, fontStyle);
}

PAGFont PAGFont::RegisterFont(const void* data, size_t length, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle) {
  return FontManager::RegisterFont(data, length, ttcIndex, fontFamily, fontStyle);
}

void PAGFont::UnregisterFont(const PAGFont& font) {
  return FontManager::UnregisterFont(font);
}

void PAGFont::SetFallbackFontNames(const std::vector<std::string>& fontNames) {
  FontManager::SetFallbackFontNames(fontNames);
}

void PAGFont::SetFallbackFontPaths(const std::vector<std::string>& fontPaths,
                                   const std::vector<int>& ttcIndices) {
  FontManager::SetFallbackFontPaths(fontPaths, ttcIndices);
}
}  // namespace pag
