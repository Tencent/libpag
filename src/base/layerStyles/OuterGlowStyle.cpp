/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include "base/utils/TGFXCast.h"
#include "base/utils/Verify.h"
#include "pag/file.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
OuterGlowStyle::~OuterGlowStyle() {
  delete blendMode;
  delete opacity;
  delete noise;
  delete colorType;
  delete color;
  delete colors;
  delete gradientSmoothness;
  delete technique;
  delete spread;
  delete size;
  delete range;
  delete jitter;
}

bool OuterGlowStyle::visibleAt(Frame) const {
  return true;
}

void OuterGlowStyle::transformBounds(Rect* contentBounds, const Point& filterScale,
                                     Frame layerFrame) const {
  auto spreadValue = spread->getValueAt(layerFrame);
  auto sizeValue = size->getValueAt(layerFrame);
  auto rangeValue = range->getValueAt(layerFrame);
  auto spreadSize = sizeValue * spreadValue / rangeValue;
  auto blurSize = sizeValue * (1.f - spreadValue) / rangeValue;
  auto blurXSize = blurSize * filterScale.x;
  auto blurYSize = blurSize * filterScale.y;
  contentBounds->outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  if (spreadValue != 1.f) {
    *contentBounds = ToPAG(tgfx::ImageFilter::DropShadowOnly(0.f, 0.f, blurXSize / 2, blurYSize / 2,
                                                             tgfx::Color::White())
                               ->filterBounds(*ToTGFX(contentBounds)));
  }
}

void OuterGlowStyle::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  blendMode->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
  noise->excludeVaryingRanges(timeRanges);
  colorType->excludeVaryingRanges(timeRanges);
  color->excludeVaryingRanges(timeRanges);
  colors->excludeVaryingRanges(timeRanges);
  gradientSmoothness->excludeVaryingRanges(timeRanges);
  technique->excludeVaryingRanges(timeRanges);
  spread->excludeVaryingRanges(timeRanges);
  size->excludeVaryingRanges(timeRanges);
  range->excludeVaryingRanges(timeRanges);
  jitter->excludeVaryingRanges(timeRanges);
}

bool OuterGlowStyle::verify() const {
  if (!LayerStyle::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(blendMode != nullptr && opacity != nullptr && noise != nullptr &&
                  colorType != nullptr && color != nullptr && colors != nullptr &&
                  gradientSmoothness != nullptr && technique != nullptr && spread != nullptr &&
                  size != nullptr && range != nullptr && jitter != nullptr);
}
}  // namespace pag
