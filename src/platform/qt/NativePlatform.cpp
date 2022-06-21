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

#include "NativePlatform.h"
#include <vector>
#include "pag/pag.h"

namespace pag {

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

bool NativePlatform::registerFallbackFonts() const {
  std::vector<std::string> fallbackList;
#ifdef WIN32
  fallbackList = {"Microsoft YaHei",    "Times New Roman", "Microsoft Sans Serif",
                  "Microsoft JhengHei", "Leelawadee UI",   "MS Gothic",
                  "Malgun Gothic",      "STSong"};
#else
  fallbackList = {"PingFang SC",       "Apple SD Gothic Neo",
                  "Apple Color Emoji", "Helvetica",
                  "Myanmar Sangam MN", "Thonburi",
                  "Mishafi",           "Menlo",
                  "Kailasa",           "Kefa",
                  "Kohinoor Telugu",   "Hiragino Maru Gothic ProN"};
#endif
  PAGFont::SetFallbackFontNames(fallbackList);
  return true;
}
}  // namespace pag