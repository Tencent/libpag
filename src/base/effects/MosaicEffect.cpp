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
MosaicEffect::~MosaicEffect() {
  delete horizontalBlocks;
  delete verticalBlocks;
  delete sharpColors;
}

bool MosaicEffect::visibleAt(Frame) const {
  return true;
}

void MosaicEffect::transformBounds(Rect*, const Point&, Frame) const {
}

void MosaicEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  horizontalBlocks->excludeVaryingRanges(timeRanges);
  verticalBlocks->excludeVaryingRanges(timeRanges);
  sharpColors->excludeVaryingRanges(timeRanges);
}

bool MosaicEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(horizontalBlocks != nullptr && verticalBlocks != nullptr &&
                  sharpColors != nullptr);
}
}  // namespace pag
