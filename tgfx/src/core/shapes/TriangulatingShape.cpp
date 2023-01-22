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

#include "TriangulatingShape.h"
#include "core/PathRef.h"
#include "gpu/GpuBuffer.h"
#include "gpu/ops/TriangulatingPathOp.h"

namespace tgfx {
TriangulatingShape::TriangulatingShape(std::unique_ptr<PathProxy> pathProxy, float resolutionScale)
    : PathShape(std::move(pathProxy), resolutionScale) {
}

std::unique_ptr<DrawOp> TriangulatingShape::makeOp(GpuPaint* paint,
                                                   const Matrix& viewMatrix) const {
  auto resourceCache = paint->context->resourceCache();
  auto buffer = std::static_pointer_cast<GpuBuffer>(resourceCache->getByContentOwner(this));
  if (buffer != nullptr) {
    auto vertexCount = buffer->size() / (sizeof(float) * 3);
    return std::make_unique<TriangulatingPathOp>(paint->color, buffer, vertexCount, bounds,
                                                 viewMatrix);
  }
  auto path = getFillPath();
  std::vector<float> vertices = {};
  int count = PathRef::ToAATriangles(path, bounds, &vertices);
  if (count == 0) {
    return nullptr;
  }
  buffer = GpuBuffer::Make(paint->context, BufferType::Vertex, &vertices[0],
                           vertices.size() * sizeof(float));
  if (buffer == nullptr) {
    return nullptr;
  }
  resourceCache->setContentOwner(buffer.get(), this);
  return std::make_unique<TriangulatingPathOp>(paint->color, buffer, count, bounds, viewMatrix);
}
}  // namespace tgfx
