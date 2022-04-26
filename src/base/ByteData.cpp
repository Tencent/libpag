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

#include "pag/file.h"
#include "tgfx/core/Stream.h"

namespace pag {
std::unique_ptr<ByteData> ByteData::FromPath(const std::string& filePath) {
  auto stream = tgfx::Stream::MakeFromFile(filePath);
  if (stream == nullptr) {
    return nullptr;
  }
  auto data = ByteData::Make(stream->size());
  if (data == nullptr) {
    return nullptr;
  }
  stream->read(data->data(), stream->size());
  return data;
}

std::unique_ptr<ByteData> ByteData::MakeCopy(const void* bytes, size_t length) {
  if (length == 0) {
    return Make(0);
  }
  auto data = new (std::nothrow) uint8_t[length];
  if (data == nullptr) {
    return nullptr;
  }
  memcpy(data, bytes, length);
  auto byteData = new ByteData(data, length);
  return std::unique_ptr<ByteData>(byteData);
}

std::unique_ptr<ByteData> ByteData::MakeWithoutCopy(void* data, size_t length) {
  if (length == 0) {
    return Make(0);
  }
  auto byteData = new ByteData(reinterpret_cast<uint8_t*>(data), length, nullptr);
  return std::unique_ptr<ByteData>(byteData);
}

std::unique_ptr<ByteData> ByteData::MakeAdopted(uint8_t* data, size_t length,
                                                std::function<void(uint8_t*)> releaseCallback) {
  if (length == 0) {
    data = nullptr;
  }
  auto byteData = new ByteData(data, length, releaseCallback);
  return std::unique_ptr<ByteData>(byteData);
}

std::unique_ptr<ByteData> ByteData::Make(size_t length) {
  auto data = length > 0 ? new (std::nothrow) uint8_t[length] : nullptr;
  auto byteData = new ByteData(data, length);
  return std::unique_ptr<ByteData>(byteData);
}

}  // namespace pag
