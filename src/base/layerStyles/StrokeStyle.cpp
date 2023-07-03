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
StrokeStyle::~StrokeStyle() {
  delete blendMode;
  delete color;
  delete size;
  delete opacity;
  delete position;
}

bool StrokeStyle::visibleAt(Frame) const {
  return true;
}

void StrokeStyle::transformBounds(Rect* contentBounds, const Point& filterScale,
                                  Frame layerFrame) const {
  auto sizeValue = size->getValueAt(layerFrame);
  contentBounds->outset(sizeValue * filterScale.x, sizeValue * filterScale.y);
}

void StrokeStyle::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  blendMode->excludeVaryingRanges(timeRanges);
  color->excludeVaryingRanges(timeRanges);
  size->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
  position->excludeVaryingRanges(timeRanges);
}

bool StrokeStyle::verify() const {
  if (!LayerStyle::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(blendMode != nullptr && color != nullptr && size != nullptr &&
                  opacity != nullptr && position != nullptr);
}
}  // namespace pag
