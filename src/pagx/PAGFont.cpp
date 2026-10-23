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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGFont.h"
#include "GlobalFontRegistry.h"

namespace pagx {

bool PAGFont::RegisterFont(const std::string& fontPath) {
  return GlobalFontRegistry::RegisterFontFromPath(fontPath);
}

bool PAGFont::RegisterFont(const void* data, size_t length) {
  return GlobalFontRegistry::RegisterFontFromBytes(data, length);
}

void PAGFont::SetFallbackFontNames(const std::vector<std::string>& names) {
  GlobalFontRegistry::SetFallbackFontNames(names);
}

}  // namespace pagx
