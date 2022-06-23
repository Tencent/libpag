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

#include "TriangularPathMesh.h"
#include "gpu/opengl/GLTriangulatingPathOp.h"

namespace tgfx {
std::unique_ptr<GLDrawOp> TriangularPathMesh::getOp(const Matrix& viewMatrix) const {
  auto point = Point::Zero();
  auto vertices = _vertices;
  auto bounds = this->bounds();
  if (!viewMatrix.isIdentity()) {
    viewMatrix.mapRect(&bounds);
    for (size_t i = 0; i < vertices.size(); i += 3) {
      viewMatrix.mapXY(vertices[i], vertices[i + 1], &point);
      vertices[i] = point.x;
      vertices[i + 1] = point.y;
    }
  }
  auto localMatrix = Matrix::I();
  if (!viewMatrix.invert(&localMatrix)) {
    return nullptr;
  }
  return std::make_unique<GLTriangulatingPathOp>(std::move(vertices), _vertexCount, bounds,
                                                 localMatrix);
}
}  // namespace tgfx
