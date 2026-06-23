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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGImage.h"
#include "tgfx/core/Image.h"

namespace pagx {

static std::shared_ptr<tgfx::Image> DecodeDataURI(const std::string& dataURI) {
  auto commaPos = dataURI.find(',');
  if (dataURI.find("data:") != 0 || commaPos == std::string::npos) {
    return nullptr;
  }
  auto base64 = dataURI.substr(commaPos + 1);
  static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<uint8_t> bytes = {};
  int val = 0;
  int bits = 0;
  for (char c : base64) {
    if (c == '=') {
      break;
    }
    const char* pos = strchr(table, c);
    if (pos == nullptr) {
      continue;
    }
    val = (val << 6) | static_cast<int>(pos - table);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      bytes.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
    }
  }
  auto tgfxData = tgfx::Data::MakeWithCopy(bytes.data(), bytes.size());
  return tgfx::Image::MakeFromEncoded(tgfxData);
}

std::shared_ptr<PAGImage> PAGImage::MakeFromPath(const std::string& path) {
  std::shared_ptr<tgfx::Image> image = nullptr;
  if (path.find("data:") == 0) {
    image = DecodeDataURI(path);
  } else if (!path.empty()) {
    image = tgfx::Image::MakeFromFile(path);
  }
  if (image == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGImage>(new PAGImage(std::move(image), path));
}

std::shared_ptr<PAGImage> PAGImage::MakeFromData(const std::shared_ptr<Data>& data) {
  if (data == nullptr || data->size() == 0) {
    return nullptr;
  }
  auto tgfxData = tgfx::Data::MakeWithCopy(data->data(), data->size());
  auto image = tgfx::Image::MakeFromEncoded(tgfxData);
  if (image == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGImage>(new PAGImage(std::move(image), {}));
}

const std::string& PAGImage::source() const {
  return _source;
}
PAGImage::PAGImage(std::shared_ptr<tgfx::Image> image, std::string source)
    : _tgfxImage(std::move(image)), _source(std::move(source)) {
}

}  // namespace pagx
