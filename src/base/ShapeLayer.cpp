/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <cmath>
#include "base/utils/Verify.h"
#include "codec/tags/shapes/Fill.h"

namespace pag {
ShapeLayer::~ShapeLayer() {
  for (auto& element : contents) {
    delete element;
  }
}

void ShapeLayer::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) {
  Layer::excludeVaryingRanges(timeRanges);
  for (auto& element : contents) {
    element->excludeVaryingRanges(timeRanges);
  }
}

bool ShapeLayer::verify() const {
  if (!Layer::verify()) {
    VerifyFailed();
    return false;
  }
  for (auto& element : contents) {
    if (element == nullptr || !element->verify()) {
      VerifyFailed();
      return false;
    }
  }
  return true;
}

Rect ShapeLayer::getBounds() const {
  return Rect::MakeWH(containingComposition->width, containingComposition->height);
}
}  // namespace pag
