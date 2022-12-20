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
Transform3D* Transform3D::MakeDefault() {
  auto transform = new Transform3D();
  transform->anchorPoint = new Property<Point3D>();
  transform->anchorPoint->value = Point3D::Zero();
  transform->position = new Property<Point3D>();
  transform->position->value = Point3D::Zero();
  transform->scale = new Property<Point3D>();
  transform->scale->value = Point3D::Make(1, 1, 1);
  transform->orientation = new Property<Point3D>();
  transform->orientation->value = Point3D::Make(0, 0, 0);
  transform->xRotation = new Property<float>();
  transform->xRotation->value = 0.0f;
  transform->yRotation = new Property<float>();
  transform->yRotation->value = 0.0f;
  transform->zRotation = new Property<float>();
  transform->zRotation->value = 0.0f;
  transform->opacity = new Property<Opacity>();
  transform->opacity->value = Opaque;
  return transform;
}
Transform3D::~Transform3D() {
  delete anchorPoint;
  delete position;
  delete xPosition;
  delete yPosition;
  delete zPosition;
  delete scale;
  delete orientation;
  delete xRotation;
  delete yRotation;
  delete zRotation;
  delete opacity;
}

void Transform3D::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  anchorPoint->excludeVaryingRanges(timeRanges);
  if (position != nullptr) {
    position->excludeVaryingRanges(timeRanges);
  } else {
    xPosition->excludeVaryingRanges(timeRanges);
    yPosition->excludeVaryingRanges(timeRanges);
    zPosition->excludeVaryingRanges(timeRanges);
  }
  scale->excludeVaryingRanges(timeRanges);
  orientation->excludeVaryingRanges(timeRanges);
  xRotation->excludeVaryingRanges(timeRanges);
  yRotation->excludeVaryingRanges(timeRanges);
  zRotation->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
}

bool Transform3D::verify() const {
  VerifyAndReturn(anchorPoint != nullptr && (position != nullptr ||
                   (xPosition != nullptr && yPosition != nullptr && zRotation != nullptr)) &&
                  scale != nullptr && xRotation != nullptr && yRotation != nullptr &&
                  zRotation != nullptr && opacity != nullptr);
}
}  // namespace pag