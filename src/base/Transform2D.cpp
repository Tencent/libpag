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
std::unique_ptr<Transform2D> Transform2D::MakeDefault() {
  auto transform = new Transform2D();
  transform->anchorPoint = new Property<Point>();
  transform->anchorPoint->value = Point::Zero();
  transform->position = new Property<Point>();
  transform->position->value = Point::Zero();
  transform->scale = new Property<Point>();
  transform->scale->value = Point::Make(1, 1);
  transform->rotation = new Property<float>();
  transform->rotation->value = 0.0f;
  transform->opacity = new Property<Opacity>();
  transform->opacity->value = Opaque;
  return std::unique_ptr<Transform2D>(transform);
}
Transform2D::~Transform2D() {
  delete anchorPoint;
  delete position;
  delete xPosition;
  delete yPosition;
  delete scale;
  delete rotation;
  delete opacity;
}

void Transform2D::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  anchorPoint->excludeVaryingRanges(timeRanges);
  if (position != nullptr) {
    position->excludeVaryingRanges(timeRanges);
  } else {
    xPosition->excludeVaryingRanges(timeRanges);
    yPosition->excludeVaryingRanges(timeRanges);
  }
  scale->excludeVaryingRanges(timeRanges);
  rotation->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
}

bool Transform2D::verify() const {
  VerifyAndReturn(anchorPoint != nullptr &&
                  (position != nullptr || (xPosition != nullptr && yPosition != nullptr)) &&
                  scale != nullptr && rotation != nullptr && opacity != nullptr);
}
}  // namespace pag
