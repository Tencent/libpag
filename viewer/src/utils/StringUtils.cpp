/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "StringUtils.h"
#include <pag/file.h>

namespace pag::Utils {

QString ToQString(double num) {
  QString result;
  return result.setNum(num, 'f', 2);
}

QString ToQString(int32_t num) {
  QString result;
  return result.setNum(num);
}

QString ToQString(int64_t num) {
  QString result;
  return result.setNum(num);
}

QString GetMemorySizeUnit(int64_t size) {
  constexpr int64_t kbThreshold = 1024;
  constexpr int64_t mbThreshold = 1024 * 1024;
  constexpr int64_t gbThreshold = 1024 * 1024 * 1024;
  if (size < kbThreshold) {
    return "B";
  }
  if (size < mbThreshold) {
    return "KB";
  }
  if (size < gbThreshold) {
    return "MB";
  }
  return "GB";
}

QString GetMemorySizeNumString(int64_t size) {
  constexpr double kbThreshold = 1024;
  constexpr double mbThreshold = 1024 * 1024;
  constexpr double gbThreshold = 1024 * 1024 * 1024;
  double dSize = static_cast<double>(size);
  if (dSize < kbThreshold) {
    return ToQString(size);
  }
  if (dSize < mbThreshold) {
    return ToQString(dSize / static_cast<double>(kbThreshold));
  }
  if (dSize < gbThreshold) {
    return ToQString(dSize / static_cast<double>(mbThreshold));
  }
  return ToQString(dSize / static_cast<double>(gbThreshold));
}

using TagCodeVersionPair = std::pair<TagCode, std::string>;
static const std::vector<TagCodeVersionPair> TagCodeVersionList = {
    {pag::TagCode::FileAttributes, "1.0"}, {pag::TagCode::LayerAttributesExtra, "2.0"},
    {pag::TagCode::MosaicEffect, "3.2"},   {pag::TagCode::GradientOverlayStyle, "4.1"},
    {pag::TagCode::CameraOption, "4.2"},   {pag::TagCode::ImageScaleModes, "4.3"}};

std::string TagCodeToVersion(uint16_t tagCode) {
  for (const auto& pair : TagCodeVersionList) {
    if (tagCode <= static_cast<uint16_t>(pair.first)) {
      return pair.second;
    }
  }
  return "Unknown";
}

pag::Color QStringToColor(const QString& color) {
  Color pagColor{};
  auto strColor = color.toStdString();
  auto strColor12 = strColor.substr(1, 2);
  auto r = strColor12.c_str();
  auto strColor34 = strColor.substr(3, 2);
  auto g = strColor34.c_str();
  auto strColor56 = strColor.substr(5, 2);
  auto b = strColor56.c_str();

  char* str;
  uint8_t red = strtol(r, &str, 16);
  uint8_t green = strtol(g, &str, 16);
  uint8_t blue = strtol(b, &str, 16);
  pagColor.red = red;
  pagColor.green = green;
  pagColor.blue = blue;
  return pagColor;
}

QString ColorToQString(const Color& color) {
  char hexColor[8] = {0};
  snprintf(hexColor, 8, "#%02X%02X%02X", color.red, color.green, color.blue);
  return {hexColor};
}

}  // namespace pag::Utils
