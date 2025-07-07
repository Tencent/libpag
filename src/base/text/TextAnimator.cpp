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
TextAnimator::~TextAnimator() {
  for (auto& selector : selectors) {
    delete selector;
  }
  delete colorProperties;
  delete typographyProperties;
}

void TextAnimator::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  for (auto& selector : selectors) {
    selector->excludeVaryingRanges(timeRanges);
  }
  if (colorProperties != nullptr) {
    colorProperties->excludeVaryingRanges(timeRanges);
  }
  if (typographyProperties != nullptr) {
    typographyProperties->excludeVaryingRanges(timeRanges);
  }
}

bool TextAnimator::verify() const {
  for (auto& selector : selectors) {
    if (selector == nullptr || !selector->verify()) {
      VerifyFailed();
      return false;
    }
  }
  if (colorProperties != nullptr && !colorProperties->verify()) {
    VerifyFailed();
    return false;
  }
  if (typographyProperties != nullptr && !typographyProperties->verify()) {
    VerifyFailed();
    return false;
  }
  return true;
}
}  // namespace pag
