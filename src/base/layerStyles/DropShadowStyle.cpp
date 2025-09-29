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
#include "base/utils/TGFXCast.h"
#include "base/utils/Verify.h"
#include "pag/file.h"
#include "tgfx/core/ImageFilter.h"

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
  auto spreadValue = spread->getValueAt(layerFrame);
  auto sizeValue = size->getValueAt(layerFrame);
  auto spreadSize = sizeValue * spreadValue;
  auto blurSize = sizeValue * (1.f - spreadValue);
  auto blurXSize = blurSize * filterScale.x;
  auto blurYSize = blurSize * filterScale.y;
  auto distanceValue = distance->getValueAt(layerFrame);
  float offsetX = 0.f;
  float offsetY = 0.f;
  if (distanceValue > 0.f) {
    auto angleValue = angle->getValueAt(layerFrame);
    auto radians = DegreesToRadians(angleValue - 180.f);
    offsetX = cosf(radians) * distanceValue * filterScale.x;
    offsetY = -sinf(radians) * distanceValue * filterScale.y;
  }
  contentBounds->outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  if (spreadValue == 1.f) {
    contentBounds->offset(offsetX, offsetY);
  } else {
    *contentBounds = ToPAG(tgfx::ImageFilter::DropShadowOnly(offsetX, offsetY, blurXSize / 2,
                                                             blurYSize / 2, tgfx::Color::White())
                               ->filterBounds(*ToTGFX(contentBounds)));
  }
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
