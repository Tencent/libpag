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

std::shared_ptr<Data> Data::MakeSlice(std::shared_ptr<Data> parent, size_t offset,
                                      size_t length) {
  if (parent == nullptr || offset + length > parent->size()) {
    return nullptr;
  }
  return std::shared_ptr<Data>(new Data(std::move(parent), offset, length));
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

Data::Data(std::shared_ptr<Data> parent, size_t offset, size_t length)
    : _parent(std::move(parent)), _data(_parent->bytes() + offset), _size(length) {
}

Data::~Data() {
  if (_parent == nullptr) {
    delete[] const_cast<uint8_t*>(_data);
  }
}

}  // namespace pagx
