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

#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
StrokeElement::~StrokeElement() {
  delete miterLimit;
  delete color;
  delete opacity;
  delete strokeWidth;
  delete dashOffset;
  for (auto& dash : dashes) {
    delete dash;
  }
}

void StrokeElement::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  miterLimit->excludeVaryingRanges(timeRanges);
  color->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
  strokeWidth->excludeVaryingRanges(timeRanges);
  if (!dashes.empty()) {
    dashOffset->excludeVaryingRanges(timeRanges);
    for (auto& property : dashes) {
      property->excludeVaryingRanges(timeRanges);
    }
  }
}

bool StrokeElement::verify() const {
  if (!ShapeElement::verify()) {
    VerifyFailed();
    return false;
  }
  for (auto dash : dashes) {
    if (dash == nullptr) {
      VerifyFailed();
      return false;
    }
  }
  VerifyAndReturn(miterLimit != nullptr && color != nullptr && opacity != nullptr &&
                  strokeWidth != nullptr);
}
}  // namespace pag
