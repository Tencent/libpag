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
#include "tgfx/core/Data.h"

namespace tgfx {
struct FTFontData {
  FTFontData(std::string path, int ttcIndex) : path(std::move(path)), ttcIndex(ttcIndex) {
  }

  FTFontData(const void* data, size_t length, int ttcIndex)
      : data(Data::MakeWithCopy(data, length)), ttcIndex(ttcIndex) {
  }

  std::string path;
  std::shared_ptr<Data> data;
  int ttcIndex = -1;
};
}  // namespace tgfx
