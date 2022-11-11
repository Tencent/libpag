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

#pragma once

#include "gpu/BufferProvider.h"
#include "gpu/TriangulatingPathOp.h"
#include "tgfx/core/Mesh.h"

namespace tgfx {
class TriangularPathMesh : public Mesh {
 public:
  TriangularPathMesh(std::vector<float> vertices, int vertexCount, Rect bounds)
      : provider(std::make_shared<BufferProvider>(std::move(vertices), vertexCount, true)),
        _bounds(bounds) {
  }

  Rect bounds() const override {
    return _bounds;
  }

 private:
  std::unique_ptr<DrawOp> getOp(Color color, const Matrix& viewMatrix) const override {
    return std::make_unique<TriangulatingPathOp>(color, provider, bounds(), viewMatrix);
  }

  std::shared_ptr<BufferProvider> provider;
  Rect _bounds;
};
}  // namespace tgfx
