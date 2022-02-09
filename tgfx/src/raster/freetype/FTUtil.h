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

#include <memory>

#include "ft2build.h"
#include FT_FREETYPE_H

namespace pag {
inline FT_F26Dot6 FloatToFDot6(float x) {
  return static_cast<FT_F26Dot6>(x * 64.f);
}

inline float FDot6ToFloat(FT_F26Dot6 x) {
  return static_cast<float>(x) * 0.015625f;
}

inline FT_F26Dot6 FDot6Floor(FT_F26Dot6 x) {
  return x >> 6;
}

inline FT_F26Dot6 FDot6Ceil(FT_F26Dot6 x) {
  return ((x) + 63) >> 6;
}

inline FT_F26Dot6 FDot6Round(FT_F26Dot6 x) {
  return (((x) + 32) >> 6);
}

class FTLibrary {
 public:
  FTLibrary();

  FTLibrary(const FTLibrary&) = delete;

  FTLibrary& operator=(const FTLibrary&) = delete;

  ~FTLibrary();

  FT_Library library() const {
    return _library;
  }

 private:
  FT_Library _library = nullptr;
};

}  // namespace pag
