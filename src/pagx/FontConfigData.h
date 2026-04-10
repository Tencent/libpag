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

#include <string>
#include <unordered_map>
#include <vector>
#include "TypefaceHolder.h"
#include "pagx/FontConfig.h"

namespace pagx {

struct FontConfig::Data {
  struct FontKey {
    std::string family = {};
    std::string style = {};

    bool operator==(const FontKey& other) const {
      return family == other.family && style == other.style;
    }
  };

  struct FontKeyHash {
    size_t operator()(const FontKey& key) const {
      return std::hash<std::string>()(key.family) ^ (std::hash<std::string>()(key.style) << 1);
    }
  };

  std::unordered_map<FontKey, std::shared_ptr<tgfx::Typeface>, FontKeyHash> registeredTypefaces =
      {};
  std::vector<TypefaceHolder> fallbackTypefaces = {};
};

}  // namespace pagx
