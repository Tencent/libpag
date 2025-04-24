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

#include "StringUtils.h"
#include <pag/file.h>

namespace pag::Utils {

QString toQString(double num) {
  QString result;
  return result.setNum(num, 'f', 2);
  ;
}

QString toQString(int32_t num) {
  QString result;
  return result.setNum(num);
}

QString toQString(int64_t num) {
  QString result;
  return result.setNum(num);
}

QString getMemorySizeUnit(int64_t size) {
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

QString getMemorySizeNumString(int64_t size) {
  constexpr double kbThreshold = 1024;
  constexpr double mbThreshold = 1024 * 1024;
  constexpr double gbThreshold = 1024 * 1024 * 1024;
  double dSize = static_cast<double>(size);
  if (dSize < kbThreshold) {
    return toQString(size);
  }
  if (dSize < mbThreshold) {
    return toQString(dSize / static_cast<double>(kbThreshold));
  }
  if (dSize < gbThreshold) {
    return toQString(dSize / static_cast<double>(mbThreshold));
  }
  return toQString(dSize / static_cast<double>(gbThreshold));
}

using TagCodeVersionPair = std::pair<TagCode, std::string>;
static const std::vector<TagCodeVersionPair> TagCodeVersionList = {
    {pag::TagCode::FileAttributes, "1.0"}, {pag::TagCode::LayerAttributesExtra, "2.0"},
    {pag::TagCode::MosaicEffect, "3.2"},   {pag::TagCode::GradientOverlayStyle, "4.1"},
    {pag::TagCode::CameraOption, "4.2"},   {pag::TagCode::ImageScaleModes, "4.3"}};

std::string tagCodeToVersion(uint16_t tagCode) {
  for (const auto& pair : TagCodeVersionList) {
    if (tagCode <= static_cast<uint16_t>(pair.first)) {
      return pair.second;
    }
  }
  return "Unknown";
}

}  // namespace pag::Utils