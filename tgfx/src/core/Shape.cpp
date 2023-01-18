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

#include "tgfx/core/Shape.h"
#include "core/shapes/FillPathShape.h"
#include "core/shapes/RRectShape.h"
#include "core/shapes/RectShape.h"
#include "core/shapes/StrokePathShape.h"

namespace tgfx {
std::shared_ptr<Shape> Shape::MakeFromFill(const Path& path, float resolutionScale) {
  if (path.isEmpty() || resolutionScale <= 0) {
    return nullptr;
  }
  std::shared_ptr<Shape> shape;
  Rect rect = {};
  RRect rRect = {};
  if (path.asRect(&rect)) {
    rect.scale(resolutionScale, resolutionScale);
    shape = std::make_shared<RectShape>(rect);
  } else if (path.asRRect(&rRect)) {
    rRect.scale(resolutionScale, resolutionScale);
    shape = std::make_shared<RRectShape>(rRect);
  } else {
    shape = std::make_shared<FillPathShape>(path, resolutionScale);
  }
  shape->weakThis = shape;
  return shape;
}

std::shared_ptr<Shape> Shape::MakeFromStroke(const Path& path, const Stroke& stroke,
                                             float resolutionScale) {
  if (path.isEmpty() || stroke.width <= 0 || resolutionScale <= 0) {
    return nullptr;
  }
  auto shape = std::shared_ptr<Shape>(new StrokePathShape(path, stroke, resolutionScale));
  shape->weakThis = shape;
  return shape;
}
}  // namespace tgfx
