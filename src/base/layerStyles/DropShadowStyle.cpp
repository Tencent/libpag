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

#include "base/utils/MathExtra.h"
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
DropShadowStyle::~DropShadowStyle() {
  delete blendMode;
  delete color;
  delete opacity;
  delete angle;
  delete distance;
  delete size;
  delete spread;
}

bool DropShadowStyle::visibleAt(Frame layerFrame) const {
  auto distanceValue = distance->getValueAt(layerFrame);
  auto sizeValue = size->getValueAt(layerFrame);
  return distanceValue > 0 || sizeValue > 0;
}

void DropShadowStyle::transformBounds(Rect* contentBounds, const Point& filterScale,
                                      Frame layerFrame) const {
  auto angleValue = angle->getValueAt(layerFrame);
  auto distanceValue = distance->getValueAt(layerFrame);
  auto sizeValue = size->getValueAt(layerFrame);
  auto radians = DegreesToRadians(angleValue - 180);
  float offsetX = cosf(radians) * distanceValue;
  float offsetY = -sinf(radians) * distanceValue;
  contentBounds->offset(offsetX * filterScale.x, offsetY * filterScale.y);
  contentBounds->outset(sizeValue * filterScale.x, sizeValue * filterScale.y);
}

void DropShadowStyle::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  blendMode->excludeVaryingRanges(timeRanges);
  color->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
  angle->excludeVaryingRanges(timeRanges);
  distance->excludeVaryingRanges(timeRanges);
  size->excludeVaryingRanges(timeRanges);
  spread->excludeVaryingRanges(timeRanges);
}

bool DropShadowStyle::verify() const {
  if (!LayerStyle::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(blendMode != nullptr && color != nullptr && opacity != nullptr &&
                  angle != nullptr && distance != nullptr && size != nullptr && spread != nullptr);
}
}  // namespace pag
