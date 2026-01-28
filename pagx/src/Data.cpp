/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/nodes/Data.h"
#include <cstring>

namespace pagx {

std::shared_ptr<Data> Data::MakeWithCopy(const void* data, size_t length) {
  if (data == nullptr || length == 0) {
    return nullptr;
  }
  return std::shared_ptr<Data>(new Data(data, length));
}

Data::Data(const void* data, size_t length) : _size(length) {
  if (data != nullptr && length > 0) {
    auto* buffer = new uint8_t[length];
    std::memcpy(buffer, data, length);
    _data = buffer;
  }
}

Data::~Data() {
  delete[] _data;
}

}  // namespace pagx
