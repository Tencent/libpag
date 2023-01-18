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
// Drawing a very small path using triangles may result in blurred output.
static constexpr int MinSizeForTriangles = 50;

ComplexShape::ComplexShape(float resolutionScale) : Shape(resolutionScale) {
}

std::unique_ptr<DrawOp> ComplexShape::makeOp(GpuPaint* paint, const Matrix& viewMatrix) const {
  auto resourceCache = paint->context->resourceCache();
  auto resource = resourceCache->getByContentOwner(this);
  if (resource != nullptr) {
    if (drawAsTexture) {
      return makeTextureOp(std::static_pointer_cast<Texture>(resource), paint, viewMatrix);
    }
    auto buffer = std::static_pointer_cast<GpuBuffer>(resource);
    auto vertexCount = buffer->size() / (sizeof(float) * 3);
    return std::make_unique<TriangulatingPathOp>(paint->color, buffer, vertexCount, bounds,
                                                 viewMatrix);
  }
  auto path = getFinalPath();
  auto pathOp = makePathOp(path, paint, viewMatrix);
  if (pathOp != nullptr) {
    return pathOp;
  }
  drawAsTexture = true;
  return makeTextureOp(path, paint, viewMatrix);
}

std::unique_ptr<DrawOp> ComplexShape::makePathOp(const Path& path, GpuPaint* paint,
                                                 const Matrix& viewMatrix) const {
  if (drawAsTexture) {
    return nullptr;
  }
  auto width = static_cast<int>(ceilf(bounds.width()));
  auto height = static_cast<int>(ceilf(bounds.height()));
  if (std::max(width, height) <= MinSizeForTriangles) {
    return nullptr;
  }
  std::vector<float> vertices = {};
  int count = PathRef::ToAATriangles(path, bounds, &vertices);
  if (count == 0) {
    return nullptr;
  }
  auto buffer = GpuBuffer::Make(paint->context, BufferType::Vertex, &vertices[0],
                                vertices.size() * sizeof(float));
  if (buffer == nullptr) {
    return nullptr;
  }
  auto resourceCache = paint->context->resourceCache();
  resourceCache->setContentOwner(buffer.get(), this);
  return std::make_unique<TriangulatingPathOp>(paint->color, buffer, count, bounds, viewMatrix);
}

std::unique_ptr<DrawOp> ComplexShape::makeTextureOp(const Path& path, GpuPaint* paint,
                                                    const Matrix& viewMatrix) const {
  auto width = ceilf(bounds.width());
  auto height = ceilf(bounds.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (!mask) {
    return nullptr;
  }
  auto matrix = Matrix::MakeTrans(-bounds.x(), -bounds.y());
  matrix.postScale(width / bounds.width(), height / bounds.height());
  mask->setMatrix(matrix);
  mask->fillPath(path);
  // TODO(pengweilv): mip map
  auto texture = mask->updateTexture(paint->context);
  auto resourceCache = paint->context->resourceCache();
  resourceCache->setContentOwner(texture.get(), this);
  return makeTextureOp(texture, paint, viewMatrix);
}

std::unique_ptr<DrawOp> ComplexShape::makeTextureOp(std::shared_ptr<Texture> texture,
                                                    GpuPaint* paint,
                                                    const Matrix& viewMatrix) const {
  auto maskLocalMatrix = Matrix::I();
  maskLocalMatrix.postTranslate(-bounds.x(), -bounds.y());
  maskLocalMatrix.postScale(static_cast<float>(texture->width()) / bounds.width(),
                            static_cast<float>(texture->height()) / bounds.height());
  paint->coverageFragmentProcessors.emplace_back(FragmentProcessor::MulInputByChildAlpha(
      TextureEffect::Make(paint->context, std::move(texture), SamplerState(), maskLocalMatrix)));
  return FillRectOp::Make(paint->color, bounds, viewMatrix);
}
}  // namespace tgfx
