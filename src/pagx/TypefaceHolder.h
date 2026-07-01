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

namespace tgfx {
class Typeface;
}

namespace pagx {

class TypefaceHolder {
 public:
  explicit TypefaceHolder(std::shared_ptr<tgfx::Typeface> typeface);
  TypefaceHolder(std::string path, int ttcIndex, std::string fontFamily, std::string fontStyle);

  std::shared_ptr<tgfx::Typeface> getTypeface();
  const std::string& getFontFamily() const;
  const std::string& getFontStyle() const;

 private:
  std::string path = {};
  std::string fontFamily = {};
  std::string fontStyle = {};
  int ttcIndex = 0;
  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
};

}  // namespace pagx
