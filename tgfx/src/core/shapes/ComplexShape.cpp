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

#include "ComplexShape.h"
#include "core/PathRef.h"
#include "gpu/GpuPaint.h"
#include "gpu/TextureEffect.h"
#include "gpu/ops/FillRectOp.h"
#include "gpu/ops/TriangulatingPathOp.h"
#include "tgfx/core/Mask.h"

namespace tgfx {
// https://chromium-review.googlesource.com/c/chromium/src/+/1099564/
static constexpr int AA_TESSELLATOR_MAX_VERB_COUNT = 100;

std::unique_ptr<DrawOp> ComplexShape::makeOp(GpuPaint* paint, const Matrix& viewMatrix) const {
  auto path = getFinalPath();
  auto pathOp = makePathOp(path, paint, viewMatrix);
  if (pathOp != nullptr) {
    return pathOp;
  }
  return makeTextureOp(path, paint, viewMatrix);
}

std::unique_ptr<DrawOp> ComplexShape::makePathOp(const Path& path, GpuPaint* paint,
                                                 const Matrix& viewMatrix) const {
  if (drawAsTexture) {
    return nullptr;
  }
  const auto& skPath = PathRef::ReadAccess(path);
  if (skPath.countVerbs() > AA_TESSELLATOR_MAX_VERB_COUNT) {
    drawAsTexture = true;
    return nullptr;
  }
  std::vector<float> vertices;
  int count = skPath.toAATriangles(DefaultTolerance, *reinterpret_cast<const pk::SkRect*>(&bounds),
                                   &vertices);
  if (count == 0) {
    drawAsTexture = true;
    return nullptr;
  }
  return std::make_unique<TriangulatingPathOp>(
      paint->color, std::make_shared<BufferProvider>(vertices, count), bounds, viewMatrix);
}

std::unique_ptr<DrawOp> ComplexShape::makeTextureOp(const Path& path, GpuPaint* paint,
                                                    const Matrix& viewMatrix) const {
  auto deviceBounds = path.getBounds();
  auto width = ceilf(deviceBounds.width());
  auto height = ceilf(deviceBounds.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (!mask) {
    return nullptr;
  }
  auto matrix = Matrix::MakeTrans(-deviceBounds.x(), -deviceBounds.y());
  matrix.postScale(width / deviceBounds.width(), height / deviceBounds.height());
  mask->setMatrix(matrix);
  mask->fillPath(path);
  // TODO(pengweilv): mip map
  auto texture = mask->updateTexture(paint->context);
  auto localMatrix = Matrix::I();
  localMatrix.postScale(deviceBounds.width(), deviceBounds.height());
  localMatrix.postTranslate(deviceBounds.x(), deviceBounds.y());
  auto maskLocalMatrix = Matrix::I();
  auto invert = Matrix::I();
  if (!localMatrix.invert(&invert)) {
    return nullptr;
  }
  maskLocalMatrix.postConcat(invert);
  maskLocalMatrix.postScale(width, height);
  paint->coverageFragmentProcessors.emplace_back(FragmentProcessor::MulInputByChildAlpha(
      TextureEffect::Make(paint->context, std::move(texture), SamplerState(), maskLocalMatrix)));
  return FillRectOp::Make(paint->color, bounds, viewMatrix, localMatrix);
}
}  // namespace tgfx
