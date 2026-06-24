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

#include <string>

namespace pagx {

/**
 * Describes a font requirement by (fontFamily, fontStyle). Returned by
 * PAGXDocument::getRequiredFonts() to enumerate which fonts a document references. The caller
 * resolves each entry to a font file and registers it with FontConfig before applyLayout() or
 * embed().
 */
class PAGFont {
 public:
  PAGFont() = default;
  PAGFont(std::string family, std::string style)
      : fontFamily(std::move(family)), fontStyle(std::move(style)) {}

  std::string fontFamily;
  std::string fontStyle;

  bool operator==(const PAGFont& other) const {
    return fontFamily == other.fontFamily && fontStyle == other.fontStyle;
  }
  bool operator<(const PAGFont& other) const {
    if (fontFamily != other.fontFamily) {
      return fontFamily < other.fontFamily;
    }
    return fontStyle < other.fontStyle;
  }
};

}  // namespace pagx
