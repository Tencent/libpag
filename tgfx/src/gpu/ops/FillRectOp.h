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

#include <optional>
#include "gpu/ops/DrawOp.h"

namespace tgfx {
class FillRectOp : public DrawOp {
 public:
  static std::unique_ptr<FillRectOp> Make(std::optional<Color> color, const Rect& rect,
                                          const Matrix& viewMatrix,
                                          const Matrix& localMatrix = Matrix::I());

  bool add(std::optional<Color> color, const Rect& rect, const Matrix& viewMatrix,
           const Matrix& localMatrix);

 private:
  DEFINE_OP_CLASS_ID

  FillRectOp(std::optional<Color> color, const Rect& rect, const Matrix& viewMatrix,
             const Matrix& localMatrix);

  bool onCombineIfPossible(Op* op) override;

  void onPrepare(Gpu* gpu) override;

  void onExecute(RenderPass* renderPass) override;

  bool canAdd(size_t count) const;

  std::vector<float> vertices();

  std::vector<float> coverageVertices() const;

  std::vector<float> noCoverageVertices() const;

  bool needsIndexBuffer() const;

  std::vector<Color> colors;
  std::vector<Rect> rects;
  std::vector<Matrix> viewMatrices;
  std::vector<Matrix> localMatrices;
  std::shared_ptr<GpuBuffer> vertexBuffer;
  std::shared_ptr<GpuBuffer> indexBuffer;
};
}  // namespace tgfx
