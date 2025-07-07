/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
BrightnessContrastEffect::~BrightnessContrastEffect() {
  delete brightness;
  delete contrast;
  delete useOldVersion;
}

bool BrightnessContrastEffect::visibleAt(Frame) const {
  return true;
}

void BrightnessContrastEffect::transformBounds(Rect*, const Point&, Frame) const {
}

void BrightnessContrastEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  brightness->excludeVaryingRanges(timeRanges);
  contrast->excludeVaryingRanges(timeRanges);
  useOldVersion->excludeVaryingRanges(timeRanges);
}

bool BrightnessContrastEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(brightness != nullptr && contrast != nullptr && useOldVersion != nullptr);
}
}  // namespace pag
