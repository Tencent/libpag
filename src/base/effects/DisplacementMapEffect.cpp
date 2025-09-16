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

#include "base/utils/MathUtil.h"
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
DisplacementMapEffect::~DisplacementMapEffect() {
  delete useForHorizontalDisplacement;
  delete maxHorizontalDisplacement;
  delete useForVerticalDisplacement;
  delete maxVerticalDisplacement;
  delete displacementMapBehavior;
  delete edgeBehavior;
  delete expandOutput;
}

static bool IsConst(DisplacementMapSource s) {
  return s == DisplacementMapSource::Full || s == DisplacementMapSource::Half ||
         s == DisplacementMapSource::Off;
}

bool DisplacementMapEffect::visibleAt(Frame layerFrame) const {
  if (displacementMapLayer == nullptr) {
    return false;
  }
  auto mapContentFrame = layerFrame - displacementMapLayer->startTime;
  if (mapContentFrame < 0 || mapContentFrame >= displacementMapLayer->duration) {
    return false;
  }
  return !((IsConst(useForHorizontalDisplacement->getValueAt(layerFrame)) &&
            IsConst(useForVerticalDisplacement->getValueAt(layerFrame))) ||
           (FloatNearlyZero(maxHorizontalDisplacement->getValueAt(layerFrame)) &&
            FloatNearlyZero(maxVerticalDisplacement->getValueAt(layerFrame))));
}

void DisplacementMapEffect::transformBounds(Rect* contentBounds, const Point&,
                                            Frame layerFrame) const {
  if (expandOutput->getValueAt(layerFrame)) {
    auto horizontal = maxHorizontalDisplacement->getValueAt(layerFrame);
    auto vertical = maxVerticalDisplacement->getValueAt(layerFrame);
    contentBounds->outset(std::abs(horizontal), std::abs(vertical));
  }
}

void DisplacementMapEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  useForHorizontalDisplacement->excludeVaryingRanges(timeRanges);
  maxHorizontalDisplacement->excludeVaryingRanges(timeRanges);
  useForVerticalDisplacement->excludeVaryingRanges(timeRanges);
  maxVerticalDisplacement->excludeVaryingRanges(timeRanges);
  displacementMapBehavior->excludeVaryingRanges(timeRanges);
  edgeBehavior->excludeVaryingRanges(timeRanges);
  expandOutput->excludeVaryingRanges(timeRanges);
}

bool DisplacementMapEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(useForHorizontalDisplacement != nullptr && maxHorizontalDisplacement != nullptr &&
                  useForVerticalDisplacement != nullptr && maxVerticalDisplacement != nullptr &&
                  displacementMapBehavior != nullptr && edgeBehavior != nullptr &&
                  expandOutput != nullptr);
}
}  // namespace pag
