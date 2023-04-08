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

#include "WebImageInfo.h"

using namespace emscripten;

namespace tgfx {
static std::string ReadString(const DataView& dataView, size_t offset, size_t length) {
  return std::string(reinterpret_cast<const char*>(dataView.bytes()) + offset, length);
}

static bool Compare(const DataView& dataView, size_t offset, size_t length,
                    const std::string& text) {
  return memcmp(dataView.bytes() + offset, text.c_str(), length) == 0;
}

static bool CompareAnyOf(const DataView& dataView, size_t offset, size_t length,
                         const std::vector<std::string>& textList) {
  return std::any_of(textList.begin(), textList.end(), [&](const std::string& text) {
    return memcmp(dataView.bytes() + offset, text.c_str(), length) == 0;
  });
}

static bool IsPng(std::shared_ptr<Data> imageBytes, int& width, int& height) {
  DataView dataView(imageBytes->bytes(), imageBytes->size(), ByteOrder::BigEndian);
  if (dataView.size() < 4) {
    return false;
  }
  if (!Compare(dataView, 0, 4, "\x89PNG")) {
    return false;
  }
  std::string firstChunkType = ReadString(dataView, 12, 4);
  if (firstChunkType == "IHDR" && dataView.size() >= 24) {
    width = dataView.readUint32(16);
    height = dataView.readUint32(20);
    return true;
  } else if (firstChunkType == "CgBI") {
    if (Compare(dataView, 28, 4, "IHDR") && dataView.size() >= 40) {
      width = dataView.readUint32(32);
      height = dataView.readUint32(36);
      return true;
    }
  }
  return false;
}

static bool IsJpeg(std::shared_ptr<Data> imageBytes, int& width, int& height) {
  DataView dataView(imageBytes->bytes(), imageBytes->size(), ByteOrder::BigEndian);
  if (dataView.size() < 2) {
    return false;
  }
  if (!Compare(dataView, 0, 2, "\xFF\xD8")) {
    return false;
  }
  off_t offset = 2;
  while (offset + 9 <= static_cast<long>(dataView.size())) {
    uint16_t sectionSize = dataView.readUint16(offset + 2);
    if (CompareAnyOf(dataView, offset, 2, {"\xFF\xC0", "\xFF\xC1", "\xFF\xC2"})) {
      height = dataView.readUint16(offset + 5);
      width = dataView.readUint16(offset + 7);
      return true;
    }
    offset += sectionSize + 2;
  }
  return false;
}

static bool IsWebp(std::shared_ptr<Data> imageBytes, int& width, int& height) {
  DataView dataView(imageBytes->bytes(), imageBytes->size());
  if (dataView.size() < 16) {
    return false;
  }
  if (!Compare(dataView, 0, 4, "RIFF") || !Compare(dataView, 8, 4, "WEBP")) {
    return false;
  }
  auto type = ReadString(dataView, 12, 4);
  if (type == "VP8 " && dataView.size() >= 30) {
    width = dataView.readUint16(26) & 0x3FFF;
    height = dataView.readUint16(28) & 0x3FFF;
    return true;
  } else if (type == "VP8L" && dataView.size() >= 25) {
    uint32_t n = dataView.readUint32(21);
    width = (n & 0x3FFF) + 1;
    height = ((n >> 14) & 0x3FFF) + 1;
    return true;
  } else if (type == "VP8X" && dataView.size() >= 30) {
    uint8_t extendedHeader = dataView.readUint8(20);
    bool validStart = (extendedHeader & 0xc0) == 0;
    bool validEnd = (extendedHeader & 0x01) == 0;
    if (validStart && validEnd) {
      width = (dataView.readUint32(24) & 0x00FFFFFF) + 1;
      height = (dataView.readUint32(27) & 0x00FFFFFF) + 1;
      return true;
    }
  }
  return false;
}

static bool CheckWebpSupport() {
  return val::module_property("tgfx").call<val>("hasWebpSupport").as<bool>();
}

ISize WebImageInfo::GetSize(std::shared_ptr<Data> imageBytes) {
  static const bool hasWebpSupport = CheckWebpSupport();
  auto imageSize = ISize::MakeEmpty();
  if (IsPng(imageBytes, imageSize.width, imageSize.height)) {
    return imageSize;
  }
  if (IsJpeg(imageBytes, imageSize.width, imageSize.height)) {
    return imageSize;
  }
  if (hasWebpSupport && IsWebp(imageBytes, imageSize.width, imageSize.height)) {
    return imageSize;
  }
  return imageSize;
}

}  // namespace tgfx
