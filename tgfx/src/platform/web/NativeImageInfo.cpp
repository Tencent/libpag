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

#include "NativeImageInfo.h"

using namespace emscripten;

namespace tgfx {
class IIBuffer {
 public:
  explicit IIBuffer(std::shared_ptr<Data> imageBytes) : _imageBytes(std::move(imageBytes)) {
  }

  const void* data() const {
    return _imageBytes->data();
  }

  size_t size() const {
    return _imageBytes->size();
  }

  bool cmp(off_t offset, size_t size, const void* buffer) {
    return memcmp(_imageBytes->bytes() + offset, buffer, size) == 0;
  }

  bool cmpAnyOf(off_t offset, size_t size, const std::initializer_list<const void*>& bufList) {
    return std::any_of(bufList.begin(), bufList.end(), [this, offset, size](const void* buf) {
      return memcmp(_imageBytes->bytes() + offset, buf, size) == 0;
    });
  }

  std::string readString(off_t offset, size_t size) {
    return std::string((char*)_imageBytes->bytes() + offset, size);
  }

  uint8_t readU8(off_t offset) {
    return *((uint8_t*)(_imageBytes->bytes() + offset));
  }

  uint16_t readU16LE(off_t offset) {
    return readU8(offset) + (readU8(offset + 1) << 8);
  }

  uint16_t readU16BE(off_t offset) {
    return (readU8(offset) << 8) + readU8(offset + 1);
  }

  uint32_t readU32LE(off_t offset) {
    return readU8(offset) + (readU8(offset + 1) << 8) + (readU8(offset + 2) << 16) +
           (readU8(offset + 3) << 24);
  }

  uint32_t readU32BE(off_t offset) {
    return (readU8(offset) << 24) + (readU8(offset + 1) << 16) + (readU8(offset + 2) << 8) +
           readU8(offset + 3);
  }

 private:
  std::shared_ptr<Data> _imageBytes = nullptr;
};

static bool IsPng(const std::shared_ptr<IIBuffer>& buffer, int& width, int& height) {
  if (buffer->size() < 4) {
    return false;
  }
  if (!buffer->cmp(0, 4, "\x89PNG")) {
    return false;
  }
  std::string firstChunkType = buffer->readString(12, 4);
  if (firstChunkType == "IHDR" && buffer->size() >= 24) {
    width = buffer->readU32BE(16);
    height = buffer->readU32BE(20);
    return true;
  } else if (firstChunkType == "CgBI") {
    if (buffer->readString(28, 4) == "IHDR" && buffer->size() >= 40) {
      width = buffer->readU32BE(32);
      height = buffer->readU32BE(36);
      return true;
    }
  }
  return false;
}

static bool IsJpeg(const std::shared_ptr<IIBuffer>& buffer, int& width, int& height) {
  if (buffer->size() < 2) {
    return false;
  }
  if (!buffer->cmp(0, 2, "\xFF\xD8")) {
    return false;
  }
  off_t offset = 2;
  while (offset + 9 <= static_cast<long>(buffer->size())) {
    uint16_t sectionSize = buffer->readU16BE(offset + 2);
    if (buffer->cmpAnyOf(offset, 2, {"\xFF\xC0", "\xFF\xC1", "\xFF\xC2"})) {
      height = buffer->readU16BE(offset + 5);
      width = buffer->readU16BE(offset + 7);
      return true;
    }
    offset += sectionSize + 2;
  }
  return false;
}

static bool IsWebp(const std::shared_ptr<IIBuffer>& buffer, int& width, int& height) {
  if (buffer->size() < 16) {
    return false;
  }
  if (!buffer->cmp(0, 4, "RIFF") || !buffer->cmp(8, 4, "WEBP")) {
    return false;
  }
  std::string type = buffer->readString(12, 4);
  if (type == "VP8 " && buffer->size() >= 30) {
    width = buffer->readU16LE(26) & 0x3FFF;
    height = buffer->readU16LE(28) & 0x3FFF;
    return true;
  } else if (type == "VP8L" && buffer->size() >= 25) {
    uint32_t n = buffer->readU32LE(21);
    width = (n & 0x3FFF) + 1;
    height = ((n >> 14) & 0x3FFF) + 1;
    return true;
  } else if (type == "VP8X" && buffer->size() >= 30) {
    uint8_t extendedHeader = buffer->readU8(20);
    bool validStart = (extendedHeader & 0xc0) == 0;
    bool validEnd = (extendedHeader & 0x01) == 0;
    if (validStart && validEnd) {
      width = (buffer->readU32LE(24) & 0x00FFFFFF) + 1;
      height = (buffer->readU32LE(27) & 0x00FFFFFF) + 1;
      return true;
    }
  }
  return false;
}

std::optional<NativeImageInfo> GetNativeImageInfo(std::shared_ptr<Data> imageBytes) {
  NativeImageInfo imageInfo = {};
  auto buffer = std::make_shared<IIBuffer>(std::move(imageBytes));
  if (IsPng(buffer, imageInfo.width, imageInfo.height)) {
    return imageInfo;
  } else if (IsJpeg(buffer, imageInfo.width, imageInfo.height)) {
    return imageInfo;
  } else if (IsWebp(buffer, imageInfo.width, imageInfo.height)) {
    auto nativeImageClass = val::module_property("NativeImage");
    if (!nativeImageClass.as<bool>()) {
      return std::nullopt;
    }
    if (!nativeImageClass.call<bool>("isSupportWebp")) {
      return std::nullopt;
    }
    return imageInfo;
  }
  return std::nullopt;
}

}  // namespace tgfx
