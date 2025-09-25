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

#include "base/utils/TGFXCast.h"
#include "base/utils/Verify.h"
#include "pag/file.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
FastBlurEffect::~FastBlurEffect() {
  delete blurriness;
  delete blurDimensions;
  delete repeatEdgePixels;
}

bool FastBlurEffect::visibleAt(Frame layerFrame) const {
  return blurriness->getValueAt(layerFrame) != 0;
}

void FastBlurEffect::transformBounds(Rect* contentBounds, const Point& filterScale,
                                     Frame layerFrame) const {
  auto repeatEdge = repeatEdgePixels->getValueAt(layerFrame);
  if (repeatEdge) {
    return;
  }
  auto direction = blurDimensions->getValueAt(layerFrame);
  auto blurrinessX = blurriness->getValueAt(layerFrame);
  auto blurrinessY = blurrinessX;
  if (direction == BlurDimensionsDirection::Horizontal) {
    blurrinessY = 0;
    blurrinessX *= filterScale.x;
  } else if (direction == BlurDimensionsDirection::Vertical) {
    blurrinessX = 0;
    blurrinessY *= filterScale.y;
  }
  if (auto blur = tgfx::ImageFilter::Blur(blurrinessX / 2, blurrinessY / 2)) {
    *contentBounds = ToPAG(blur->filterBounds(*ToTGFX(contentBounds)));
  }
}

void FastBlurEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  blurriness->excludeVaryingRanges(timeRanges);
  blurDimensions->excludeVaryingRanges(timeRanges);
  repeatEdgePixels->excludeVaryingRanges(timeRanges);
}

bool FastBlurEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(blurriness != nullptr && blurDimensions != nullptr &&
                  repeatEdgePixels != nullptr);
}
}  // namespace pag
