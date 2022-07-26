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
GradientOverlayStyle::~GradientOverlayStyle() {
  delete blendMode;
  delete opacity;
  delete colors;
  delete gradientSmoothness;
  delete angle;
  delete style;
  delete reverse;
  delete alignWithLayer;
  delete scale;
  delete offset;
}

bool GradientOverlayStyle::visibleAt(Frame) const {
  return true;
}

void GradientOverlayStyle::transformBounds(Rect*, const Point&, Frame) const {
}

void GradientOverlayStyle::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  blendMode->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
  colors->excludeVaryingRanges(timeRanges);
  gradientSmoothness->excludeVaryingRanges(timeRanges);
  angle->excludeVaryingRanges(timeRanges);
  style->excludeVaryingRanges(timeRanges);
  reverse->excludeVaryingRanges(timeRanges);
  alignWithLayer->excludeVaryingRanges(timeRanges);
  scale->excludeVaryingRanges(timeRanges);
  offset->excludeVaryingRanges(timeRanges);
}

bool GradientOverlayStyle::verify() const {
  if (!LayerStyle::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(blendMode != nullptr && opacity != nullptr && colors != nullptr &&
                  gradientSmoothness != nullptr && angle != nullptr && style != nullptr &&
                  reverse != nullptr && alignWithLayer != nullptr && scale != nullptr &&
                  offset != nullptr);
}
}  // namespace pag
