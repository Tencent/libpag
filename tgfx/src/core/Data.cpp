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

#include "tgfx/core/Data.h"
#include <cstring>
#include "tgfx/core/Stream.h"

namespace tgfx {
std::shared_ptr<Data> Data::MakeFromFile(const std::string& filePath) {
  auto stream = Stream::MakeFromFile(filePath);
  if (stream == nullptr) {
    return nullptr;
  }
  auto buffer = new (std::nothrow) uint8_t[stream->size()];
  if (buffer == nullptr) {
    return nullptr;
  }
  stream->read(buffer, stream->size());
  auto data = new Data(buffer, stream->size(), Data::DeleteProc, nullptr);
  return std::shared_ptr<Data>(data);
}

std::shared_ptr<Data> Data::MakeWithCopy(const void* bytes, size_t length) {
  if (length == 0) {
    return MakeEmpty();
  }
  auto data = new (std::nothrow) uint8_t[length];
  if (data == nullptr) {
    return nullptr;
  }
  memcpy(data, bytes, length);
  auto byteData = new Data(data, length, Data::DeleteProc, nullptr);
  return std::shared_ptr<Data>(byteData);
}

std::shared_ptr<Data> Data::MakeWithoutCopy(const void* data, size_t length) {
  if (length == 0) {
    return MakeEmpty();
  }
  auto byteData = new Data(data, length, nullptr, nullptr);
  return std::shared_ptr<Data>(byteData);
}

void Data::DeleteProc(const void* data, void*) {
  if (data != nullptr) {
    delete[] reinterpret_cast<const uint8_t*>(data);
  }
}

void Data::FreeProc(const void* data, void*) {
  if (data != nullptr) {
    free(const_cast<void*>(data));
  }
}

std::shared_ptr<Data> Data::MakeAdopted(const void* data, size_t length, ReleaseProc releaseProc,
                                        void* context) {
  if (length == 0) {
    return MakeEmpty();
  }
  auto byteData = new Data(data, length, releaseProc, context);
  return std::shared_ptr<Data>(byteData);
}

std::shared_ptr<Data> Data::MakeEmpty() {
  static auto emptyData = std::shared_ptr<Data>(new Data(nullptr, 0, nullptr, nullptr));
  return emptyData;
}

Data::Data(const void* data, size_t length, ReleaseProc releaseProc, void* context)
    : _data(data), _size(length), releaseProc(releaseProc), releaseContext(context) {
}

Data::~Data() {
  if (releaseProc) {
    releaseProc(_data, releaseContext);
  }
}

}  // namespace tgfx
