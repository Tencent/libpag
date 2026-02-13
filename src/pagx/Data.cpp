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

#include "pagx/types/Data.h"
#include <cstring>
#include <new>

namespace pagx {

std::shared_ptr<Data> Data::MakeWithCopy(const void* data, size_t length) {
  if (data == nullptr || length == 0) {
    return nullptr;
  }
  return std::shared_ptr<Data>(new Data(data, length));
}

std::shared_ptr<Data> Data::MakeAdopt(uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return nullptr;
  }
  return std::shared_ptr<Data>(new Data(data, length, true));
}

Data::Data(const void* data, size_t length) : _size(length) {
  if (data != nullptr && length > 0) {
    auto* buffer = new (std::nothrow) uint8_t[length];
    if (buffer != nullptr) {
      std::memcpy(buffer, data, length);
      _data = buffer;
    } else {
      _size = 0;
    }
  }
}

Data::Data(uint8_t* data, size_t length, bool) : _data(data), _size(length) {
}

Data::~Data() {
  delete[] _data;
}

}  // namespace pagx
