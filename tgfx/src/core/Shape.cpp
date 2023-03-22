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
#include "core/PathRef.h"
#include "shapes/PathProxy.h"
#include "shapes/RRectShape.h"
#include "shapes/RectShape.h"
#include "shapes/TextureShape.h"
#include "shapes/TriangulatingShape.h"
#include "tgfx/core/PathEffect.h"

namespace tgfx {
std::shared_ptr<Shape> Shape::MakeFromFill(const Path& path, float resolutionScale) {
  if (path.isEmpty() || resolutionScale <= 0) {
    return nullptr;
  }
  std::shared_ptr<Shape> shape;
  Rect rect = {};
  RRect rRect = {};
  if (path.asRect(&rect)) {
    shape = std::make_shared<RectShape>(rect, resolutionScale);
  } else if (path.asRRect(&rRect)) {
    shape = std::make_shared<RRectShape>(rRect, resolutionScale);
  } else {
    auto proxy = PathProxy::MakeFromFill(path);
    auto bounds = proxy->getBounds(resolutionScale);
    auto width = static_cast<int>(ceilf(bounds.width()));
    auto height = static_cast<int>(ceilf(bounds.height()));
    if (path.countVerbs() <= AA_TESSELLATOR_MAX_VERB_COUNT &&
        width * height >= path.countPoints() * AA_TESSELLATOR_BUFFER_SIZE_FACTOR) {
      shape = std::make_shared<TriangulatingShape>(std::move(proxy), resolutionScale);
    } else {
      shape = std::make_shared<TextureShape>(std::move(proxy), resolutionScale);
    }
  }
  shape->weakThis = shape;
  return shape;
}

std::shared_ptr<Shape> Shape::MakeFromFill(std::shared_ptr<TextBlob> textBlob,
                                           float resolutionScale) {
  if (textBlob == nullptr || resolutionScale <= 0) {
    return nullptr;
  }
  auto proxy = PathProxy::MakeFromFill(textBlob);
  if (proxy == nullptr) {
    return nullptr;
  }
  auto shape = std::make_shared<TextureShape>(std::move(proxy), resolutionScale);
  shape->weakThis = shape;
  return shape;
}

std::shared_ptr<Shape> Shape::MakeFromStroke(const Path& path, const Stroke& stroke,
                                             float resolutionScale) {
  if (path.isEmpty() || resolutionScale <= 0) {
    return nullptr;
  }
  auto strokePath = path;
  auto effect = PathEffect::MakeStroke(stroke);
  if (effect == nullptr) {
    return nullptr;
  }
  effect->applyTo(&strokePath);
  return MakeFromFill(strokePath, resolutionScale);
}

std::shared_ptr<Shape> Shape::MakeFromStroke(std::shared_ptr<TextBlob> textBlob,
                                             const Stroke& stroke, float resolutionScale) {
  if (textBlob == nullptr || resolutionScale <= 0) {
    return nullptr;
  }
  auto proxy = PathProxy::MakeFromStroke(textBlob, stroke);
  if (proxy == nullptr) {
    return nullptr;
  }
  auto shape = std::make_shared<TextureShape>(std::move(proxy), resolutionScale);
  shape->weakThis = shape;
  return shape;
}

Shape::Shape(float resolutionScale) : _resolutionScale(resolutionScale) {
  uniqueKey = UniqueKey::Next();
}
}  // namespace tgfx
