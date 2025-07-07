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

#include "TransformRenderer.h"
#include "base/utils/TGFXCast.h"

namespace pag {
void RenderTransform(Transform* transform, Transform2D* transform2D, Frame layerFrame) {
  auto& matrix = transform->matrix;
  auto anchorPoint = transform2D->anchorPoint->getValueAt(layerFrame);
  auto scale = transform2D->scale->getValueAt(layerFrame);
  Point position = {};
  if (transform2D->position != nullptr) {
    position = transform2D->position->getValueAt(layerFrame);
  } else {
    position.x = transform2D->xPosition->getValueAt(layerFrame);
    position.y = transform2D->yPosition->getValueAt(layerFrame);
  }
  auto rotation = transform2D->rotation->getValueAt(layerFrame);
  transform->alpha = ToAlpha(transform2D->opacity->getValueAt(layerFrame));
  matrix.postTranslate(-anchorPoint.x, -anchorPoint.y);
  matrix.postScale(scale.x, scale.y);
  matrix.postRotate(rotation);
  matrix.postTranslate(position.x, position.y);
}
}  // namespace pag
