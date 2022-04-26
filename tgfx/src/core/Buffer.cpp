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

#include "tgfx/core/Buffer.h"
#include <cstring>
#include "core/utils/Log.h"

namespace tgfx {
Buffer::Buffer(size_t length) : _size(length) {
  _data = length > 0 ? new (std::nothrow) uint8_t[length] : nullptr;
  if (_data == nullptr) {
    _size = 0;
  }
}

Buffer::Buffer(const void* data, size_t length) {
  _data = length > 0 ? new (std::nothrow) uint8_t[length] : nullptr;
  if (data != nullptr) {
    _size = length;
    memcpy(_data, data, length);
  }
}

Buffer::Buffer(std::shared_ptr<Data> data) : Buffer(data->data(), data->size()) {
}

Buffer::~Buffer() {
  if (_data != nullptr) {
    delete[] _data;
  }
}

std::shared_ptr<Data> Buffer::release() {
  if (empty()) {
    return nullptr;
  }
  auto data = Data::MakeAdopted(_data, _size, Data::DeleteProc);
  _data = nullptr;
  _size = 0;
  return data;
}

void Buffer::clear() {
  if (empty()) {
    return;
  }
  memset(_data, 0, _size);
}

std::shared_ptr<Data> Buffer::copyRange(size_t offset, size_t length) {
  length = getClampedLength(offset, length);
  if (length == 0) {
    return nullptr;
  }
  return Data::MakeWithCopy(_data + offset, length);
}

void Buffer::writeRange(size_t offset, size_t length, const void* bytes) {
  length = getClampedLength(offset, length);
  if (length == 0) {
    return;
  }
  memcpy(_data + offset, bytes, length);
}

uint8_t Buffer::operator[](size_t index) const {
  DEBUG_ASSERT(index >= 0 && index < _size);
  return _data[index];
}

uint8_t& Buffer::operator[](size_t index) {
  DEBUG_ASSERT(index >= 0 && index < _size);
  return _data[index];
}

size_t Buffer::getClampedLength(size_t offset, size_t length) const {
  size_t available = _size;
  if (offset >= available || length == 0) {
    return 0;
  }
  available -= offset;
  if (length > available) {
    length = available;
  }
  return length;
}
}  // namespace tgfx
