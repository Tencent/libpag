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
TextAnimatorTypographyProperties::~TextAnimatorTypographyProperties() {
  delete trackingType;
  delete trackingAmount;
  delete position;
  delete scale;
  delete rotation;
  delete opacity;
}

void TextAnimatorTypographyProperties::excludeVaryingRanges(
    std::vector<TimeRange>* timeRanges) const {
  if (trackingType != nullptr) {
    trackingType->excludeVaryingRanges(timeRanges);
  }
  if (trackingAmount != nullptr) {
    trackingAmount->excludeVaryingRanges(timeRanges);
  }
  if (position != nullptr) {
    position->excludeVaryingRanges(timeRanges);
  }
  if (scale != nullptr) {
    scale->excludeVaryingRanges(timeRanges);
  }
  if (rotation != nullptr) {
    rotation->excludeVaryingRanges(timeRanges);
  }
  if (opacity != nullptr) {
    opacity->excludeVaryingRanges(timeRanges);
  }
}

bool TextAnimatorTypographyProperties::verify() const {
  // trackingAmount is required，but trackingType could be empty.
  VerifyAndReturn(trackingAmount != nullptr || position != nullptr || scale != nullptr ||
                  rotation != nullptr || opacity != nullptr);
}
}  // namespace pag
