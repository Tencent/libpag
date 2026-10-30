/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/importer/HTMLObjectLayout.h"
#include <algorithm>

namespace pagx {
bool ResolveHTMLObjectSize(const HTMLImageIntrinsicSize& intrinsic, const std::string& fit,
                           float width, float height, float& objectWidth, float& objectHeight) {
  if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0) return false;
  if (width == 0 || height == 0 || intrinsic.width == 0 || intrinsic.height == 0) {
    objectWidth = objectHeight = 0;
    return true;
  }
  float ratio = intrinsic.ratio;
  if (!(ratio > 0) && intrinsic.width > 0 && intrinsic.height > 0)
    ratio = intrinsic.width / intrinsic.height;
  bool hasRatio = std::isfinite(ratio) && ratio > 0;
  float containW = width, containH = height;
  if (hasRatio) {
    if (width / height > ratio) containW = height * ratio;
    else
      containH = width / ratio;
  }
  if (fit.empty() || fit == "fill") {
    objectWidth = width;
    objectHeight = height;
  } else if (fit == "contain") {
    objectWidth = containW;
    objectHeight = containH;
  } else if (fit == "cover") {
    objectWidth = width;
    objectHeight = height;
    if (hasRatio) {
      if (width / height > ratio) objectHeight = width / ratio;
      else
        objectWidth = height * ratio;
    }
  } else if (fit == "none" || fit == "scale-down") {
    const bool hasWidth = std::isfinite(intrinsic.width) && intrinsic.width > 0;
    const bool hasHeight = std::isfinite(intrinsic.height) && intrinsic.height > 0;
    objectWidth = hasWidth ? intrinsic.width
                           : (hasHeight ? (hasRatio ? intrinsic.height * ratio : width) : containW);
    objectHeight = hasHeight
                       ? intrinsic.height
                       : (hasWidth ? (hasRatio ? intrinsic.width / ratio : height) : containH);
    if (fit == "scale-down") {
      objectWidth = std::min(objectWidth, containW);
      objectHeight = std::min(objectHeight, containH);
    }
  } else
    return false;
  return std::isfinite(objectWidth) && std::isfinite(objectHeight);
}

bool ResolveHTMLObjectRect(const HTMLImageIntrinsicSize& intrinsic, const std::string& fit,
                           const HTMLCSSPosition& position, const Rect& content, Rect& object) {
  float width = 0, height = 0;
  if (!ResolveHTMLObjectSize(intrinsic, fit, content.width, content.height, width, height))
    return false;
  object = Rect::MakeXYWH(content.x + position.x.resolve(content.width - width),
                          content.y + position.y.resolve(content.height - height), width, height);
  return std::isfinite(object.x) && std::isfinite(object.y);
}
}  // namespace pagx
