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

#include "pagx/PAGXTypeface.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

std::shared_ptr<PAGXTypeface> PAGXTypeface::MakeFromPath(const std::string& path, int ttcIndex,
                                                         const std::string& fontFamily,
                                                         const std::string& fontStyle) {
  return std::shared_ptr<PAGXTypeface>(new PAGXTypeface(path, ttcIndex, fontFamily, fontStyle));
}

std::shared_ptr<PAGXTypeface> PAGXTypeface::MakeFromName(const std::string& fontFamily,
                                                         const std::string& fontStyle) {
  return std::shared_ptr<PAGXTypeface>(new PAGXTypeface(fontFamily, fontStyle));
}

std::shared_ptr<PAGXTypeface> PAGXTypeface::MakeFromTypeface(
    std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGXTypeface>(new PAGXTypeface(std::move(typeface)));
}

const std::string& PAGXTypeface::fontFamily() const {
  return _fontFamily;
}

const std::string& PAGXTypeface::fontStyle() const {
  return _fontStyle;
}

std::shared_ptr<tgfx::Typeface> PAGXTypeface::getTypeface() const {
  if (_typeface == nullptr) {
    if (source == Source::Path) {
      _typeface = tgfx::Typeface::MakeFromPath(path, ttcIndex);
    } else if (source == Source::Name) {
      _typeface = tgfx::Typeface::MakeFromName(_fontFamily, _fontStyle);
    }
  }
  return _typeface;
}

PAGXTypeface::PAGXTypeface(std::string path, int ttcIndex, std::string family, std::string style)
    : source(Source::Path), path(std::move(path)), ttcIndex(ttcIndex),
      _fontFamily(std::move(family)), _fontStyle(std::move(style)) {
}

PAGXTypeface::PAGXTypeface(std::string family, std::string style)
    : source(Source::Name), _fontFamily(std::move(family)), _fontStyle(std::move(style)) {
}

PAGXTypeface::PAGXTypeface(std::shared_ptr<tgfx::Typeface> typeface)
    : source(Source::Typeface), _typeface(std::move(typeface)) {
  if (_typeface != nullptr) {
    _fontFamily = _typeface->fontFamily();
    _fontStyle = _typeface->fontStyle();
  }
}

}  // namespace pagx
