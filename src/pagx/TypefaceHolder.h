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
#include <vector>

namespace tgfx {
class Typeface;
}

namespace pagx {

/**
 * Internal holder that owns a font source (file path, in-memory bytes, or pre-built
 * tgfx::Typeface) and lazily loads the underlying tgfx::Typeface on demand. The
 * fontFamily/fontStyle are always provided externally (from PAGFont) and used as the registration
 * name for FontConfig lookup. This class is an internal implementation detail of FontConfig and is
 * not part of the public API.
 */
class TypefaceHolder {
 public:
  TypefaceHolder(std::string path, int ttcIndex, std::string family, std::string style);
  TypefaceHolder(std::vector<uint8_t> bytes, int ttcIndex, std::string family, std::string style);
  TypefaceHolder(std::shared_ptr<tgfx::Typeface> typeface, std::string family, std::string style);

  std::shared_ptr<tgfx::Typeface> getTypeface();
  const std::string& getFontFamily() const;
  const std::string& getFontStyle() const;

 private:
  std::string path = {};
  std::vector<uint8_t> bytes = {};
  int ttcIndex = 0;
  std::string fontFamily = {};
  std::string fontStyle = {};
  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
};

}  // namespace pagx
