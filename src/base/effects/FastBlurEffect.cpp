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
#include "rendering/filters/gaussianblur/GaussianBlurDefines.h"

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
  auto expandX = (direction == BlurDimensionsDirection::All ||
                  direction == BlurDimensionsDirection::Horizontal)
                     ? contentBounds->width() * BLUR_EXPEND * filterScale.x
                     : 0.0;
  auto expandY =
      (direction == BlurDimensionsDirection::All || direction == BlurDimensionsDirection::Vertical)
          ? contentBounds->height() * BLUR_EXPEND * filterScale.x
          : 0.0;
  contentBounds->outset(expandX, expandY);
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
