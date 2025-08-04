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
GlowEffect::~GlowEffect() {
  delete glowThreshold;
  delete glowRadius;
  delete glowIntensity;
}

bool GlowEffect::visibleAt(Frame layerFrame) const {
  auto threshold = glowThreshold->getValueAt(layerFrame);
  return threshold < 1.0f;
}

void GlowEffect::transformBounds(Rect*, const Point&, Frame) const {
}

void GlowEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  glowThreshold->excludeVaryingRanges(timeRanges);
  glowRadius->excludeVaryingRanges(timeRanges);
  glowIntensity->excludeVaryingRanges(timeRanges);
}

bool GlowEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(glowThreshold != nullptr && glowRadius != nullptr && glowIntensity != nullptr);
}
}  // namespace pag
