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

#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
CornerPinEffect::~CornerPinEffect() {
  delete upperLeft;
  delete upperRight;
  delete lowerLeft;
  delete lowerRight;
}

bool CornerPinEffect::visibleAt(Frame) const {
  return true;
}

void CornerPinEffect::transformBounds(Rect* contentBounds, const Point&, Frame layerFrame) const {
  auto upperLeftValue = upperLeft->getValueAt(layerFrame);
  auto upperRightValue = upperRight->getValueAt(layerFrame);
  auto lowerLeftValue = lowerLeft->getValueAt(layerFrame);
  auto lowerRightValue = lowerRight->getValueAt(layerFrame);

  auto left = std::min(std::min(upperLeftValue.x, lowerLeftValue.x),
                       std::min(upperRightValue.x, lowerRightValue.x));
  auto top = std::min(std::min(upperLeftValue.y, lowerLeftValue.y),
                      std::min(upperRightValue.y, lowerRightValue.y));
  auto right = std::max(std::max(upperLeftValue.x, lowerLeftValue.x),
                        std::max(upperRightValue.x, lowerRightValue.x));
  auto bottom = std::max(std::max(upperLeftValue.y, lowerLeftValue.y),
                         std::max(upperRightValue.y, lowerRightValue.y));

  contentBounds->setLTRB(left, top, right, bottom);
}

void CornerPinEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  upperLeft->excludeVaryingRanges(timeRanges);
  upperRight->excludeVaryingRanges(timeRanges);
  lowerLeft->excludeVaryingRanges(timeRanges);
  lowerRight->excludeVaryingRanges(timeRanges);
}

bool CornerPinEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(upperLeft != nullptr && upperRight != nullptr && lowerLeft != nullptr &&
                  lowerRight != nullptr);
}
}  // namespace pag
