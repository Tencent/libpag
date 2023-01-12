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

#include <unordered_map>
#include "base/utils/UniqueID.h"
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
Layer::Layer() : uniqueID(UniqueID::Next()) {
}

Layer::~Layer() {
  delete cache;
  delete transform;
  delete transform3D;
  delete timeRemap;
  for (auto& mask : masks) {
    delete mask;
  }
  for (auto& effect : effects) {
    delete effect;
  }
  for (auto& style : layerStyles) {
    delete style;
  }
  for (auto& marker : markers) {
    delete marker;
  }
}

void Layer::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) {
  if (transform != nullptr) {
    transform->excludeVaryingRanges(timeRanges);
  }
  if (transform3D != nullptr) {
    transform3D->excludeVaryingRanges(timeRanges);
  }
  if (timeRemap != nullptr) {
    timeRemap->excludeVaryingRanges(timeRanges);
  }
  for (auto& mask : masks) {
    mask->excludeVaryingRanges(timeRanges);
  }
  for (auto& effect : effects) {
    effect->excludeVaryingRanges(timeRanges);
  }
  for (auto& layerStyle : layerStyles) {
    layerStyle->excludeVaryingRanges(timeRanges);
  }
}

bool Layer::verify() const {
  if (containingComposition == nullptr || duration <= 0 || (transform == nullptr && transform3D == nullptr)) {
    VerifyFailed();
    return false;
  }
  if (transform != nullptr && !transform->verify()) {
    VerifyFailed();
    return false;
  }
  if (transform3D != nullptr && !transform3D->verify()) {
    VerifyFailed();
    return false;
  }
  for (auto mask : masks) {
    if (mask == nullptr || !mask->verify()) {
      VerifyFailed();
      return false;
    }
  }
  return verifyExtra();
}

bool Layer::verifyExtra() const {
  for (auto layerStyle : layerStyles) {
    if (layerStyle == nullptr || !layerStyle->verify()) {
      VerifyFailed();
      return false;
    }
  }
  for (auto effect : effects) {
    if (effect == nullptr || !effect->verify()) {
      VerifyFailed();
      return false;
    }
  }
  for (auto& marker : markers) {
    if (marker == nullptr || marker->comment.empty()) {
      VerifyFailed();
      return false;
    }
  }
  return true;
}

Rect Layer::getBounds() const {
  return Rect::MakeEmpty();
}

template<typename T>
static Point getMaxScaleInternal(Property<T>* property) {
  auto maxScale = Point::Make(1, 1);
  if (property->animatable()) {
    auto keyframes = static_cast<AnimatableProperty<T>*>(property)->keyframes;
    float scaleX = fabs(keyframes[0]->startValue.x);
    float scaleY = fabs(keyframes[0]->startValue.y);
    for (auto& keyframe : keyframes) {
      auto x = fabs(keyframe->endValue.x);
      auto y = fabs(keyframe->endValue.y);
      if (scaleX < x) {
        scaleX = x;
      }
      if (scaleY < y) {
        scaleY = y;
      }
    }
    maxScale.x = scaleX;
    maxScale.y = scaleY;
  } else {
    maxScale.x = fabs(property->value.x);
    maxScale.y = fabs(property->value.y);
  }
  return maxScale;
}

Point Layer::getMaxScaleFactor() {
  auto maxScale = Point::Make(1, 1);
  if (transform != nullptr) {
    maxScale = getMaxScaleInternal(transform->scale);
  } else if (transform3D != nullptr) {
    maxScale = getMaxScaleInternal(transform3D->scale);
  }
  if (!effects.empty()) {
    auto bounds = getBounds();
    if (!bounds.isEmpty()) {
      for (auto* effect : effects) {
        auto scale = effect->getMaxScaleFactor(bounds);
        maxScale.x *= fabs(scale.x);
        maxScale.y *= fabs(scale.y);
      }
    }
  }
  if (parent != nullptr) {
    auto parentScale = parent->getMaxScaleFactor();
    maxScale.x *= parentScale.x;
    maxScale.y *= parentScale.y;
  }
  return maxScale;
}

TimeRange Layer::visibleRange() {
  TimeRange range = {startTime, startTime + duration - 1};
  return range;
}

}  // namespace pag
