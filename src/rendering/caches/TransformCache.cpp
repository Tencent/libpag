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

#include "TransformCache.h"
#include "rendering/renderers/TransformRenderer.h"

namespace pag {
TransformCache::TransformCache(Layer* layer)
    : FrameCache<Transform>(layer->startTime, layer->duration), layer(layer) {
  std::vector<TimeRange> timeRanges = {layer->visibleRange()};
  if (layer->transform != nullptr) {
    layer->transform->excludeVaryingRanges(&timeRanges);
  }
  if (layer->transform3D != nullptr) {
    layer->transform3D->excludeVaryingRanges(&timeRanges);
  }
  auto parent = layer->parent;
  while (parent != nullptr) {
    if (parent->transform != nullptr) {
      parent->transform->excludeVaryingRanges(&timeRanges);
    }
    if (parent->transform3D != nullptr) {
      parent->transform3D->excludeVaryingRanges(&timeRanges);
    }
    SplitTimeRangesAt(&timeRanges, parent->startTime);
    SplitTimeRangesAt(&timeRanges, parent->startTime + parent->duration);
    parent = parent->parent;
  }
  staticTimeRanges = OffsetTimeRanges(timeRanges, -layer->startTime);
}

Transform* TransformCache::createCache(Frame layerFrame) {
  auto transform = new Transform();
 if (layer->transform == nullptr) {
    return transform;
  }
  RenderTransform(transform, layer->transform, layerFrame);
  auto parent = layer->parent;
  while (parent != nullptr && (parent->transform != nullptr || parent->transform3D != nullptr)) {
    if (parent->transform != nullptr) {
      Transform parentTransform = {};
      RenderTransform(&parentTransform, parent->transform, layerFrame);
      transform->matrix.postConcat(parentTransform.matrix);
    } else if (parent->transform3D != nullptr) {
      auto anchorPointP = parent->transform3D->anchorPoint->getValueAt(layerFrame);
      Point3D positionP;
      if (parent->transform3D->position) {
        positionP = parent->transform3D->position->getValueAt(layerFrame);
      } else {
        positionP.x = parent->transform3D->xPosition->getValueAt(layerFrame);
        positionP.y = parent->transform3D->yPosition->getValueAt(layerFrame);
      }
      auto scaleP = parent->transform3D->scale->getValueAt(layerFrame);
      auto zRotationP = parent->transform3D->zRotation->getValueAt(layerFrame);
      tgfx::Matrix parentMatrix2D;
      parentMatrix2D.setTranslate(-anchorPointP.x, -anchorPointP.y);
      parentMatrix2D.postScale(scaleP.x, scaleP.y);
      parentMatrix2D.postRotate(zRotationP);
      parentMatrix2D.postTranslate(positionP.x, positionP.y);
      transform->matrix.postConcat(parentMatrix2D);
    }
    parent = parent->parent;
  }
  return transform;
}

std::shared_ptr<Transform> TransformCache::getTransform3D(Frame layerFrame){
  auto transform = std::make_shared<Transform>();
  if (layer->transform3D != nullptr) {
    RenderTransform3D(transform.get(), layer->transform3D, layerFrame);
  }
  return transform;
}


}  // namespace pag