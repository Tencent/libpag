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

MotionTileEffect::~MotionTileEffect() {
  delete tileCenter;
  delete tileWidth;
  delete tileHeight;
  delete outputWidth;
  delete outputHeight;
  delete mirrorEdges;
  delete phase;
  delete horizontalPhaseShift;
}

bool MotionTileEffect::visibleAt(Frame) const {
  return true;
}

void MotionTileEffect::transformBounds(Rect* contentBounds, const Point&, Frame layerFrame) const {
  auto outputWidthValue = outputWidth->getValueAt(layerFrame);
  auto outputHeightValue = outputHeight->getValueAt(layerFrame);
  auto width = contentBounds->width() * outputWidthValue / 100.0f;
  auto height = contentBounds->height() * outputHeightValue / 100.0f;
  auto x = contentBounds->x() + (contentBounds->width() - width) * 0.5f;
  auto y = contentBounds->y() + (contentBounds->height() - height) * 0.5f;
  contentBounds->setXYWH(x, y, width, height);
}

void MotionTileEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  tileCenter->excludeVaryingRanges(timeRanges);
  tileWidth->excludeVaryingRanges(timeRanges);
  tileHeight->excludeVaryingRanges(timeRanges);
  outputWidth->excludeVaryingRanges(timeRanges);
  outputHeight->excludeVaryingRanges(timeRanges);
  mirrorEdges->excludeVaryingRanges(timeRanges);
  phase->excludeVaryingRanges(timeRanges);
  horizontalPhaseShift->excludeVaryingRanges(timeRanges);
}

bool MotionTileEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(tileCenter != nullptr && tileWidth != nullptr && tileHeight != nullptr &&
                  outputWidth != nullptr && outputHeight != nullptr && mirrorEdges != nullptr &&
                  phase != nullptr && horizontalPhaseShift != nullptr);
}
}  // namespace pag
