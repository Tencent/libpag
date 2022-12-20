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

#include "GLOpsRenderPass.h"

#include "GLBuffer.h"

namespace tgfx {
class GLFillRectOp : public GLDrawOp {
 public:
  static std::unique_ptr<GLFillRectOp> Make(const Rect& rect, const Matrix& viewMatrix,
                                            const Matrix& localMatrix = Matrix::I());

  static std::unique_ptr<GLFillRectOp> Make(Color color, const Rect& rect, const Matrix& viewMatrix,
                                            const Matrix& localMatrix = Matrix::I());

  static std::unique_ptr<GLFillRectOp> Make(const std::vector<Color>& colors,
                                            const std::vector<Rect>& rects,
                                            const std::vector<Matrix>& viewMatrices,
                                            const std::vector<Matrix>& localMatrices);

  std::vector<float> vertices();

  void execute(OpsRenderPass* opsRenderPass) override;

 private:
  DEFINE_OP_CLASS_ID

  GLFillRectOp(std::vector<Color> colors, std::vector<Rect> rects, std::vector<Matrix> viewMatrices,
               std::vector<Matrix> localMatrices);

  bool onCombineIfPossible(Op* op) override;

  std::vector<float> coverageVertices() const;

  std::vector<float> noCoverageVertices() const;

  std::vector<Color> colors;
  std::vector<Rect> rects;
  std::vector<Matrix> viewMatrices;
  std::vector<Matrix> localMatrices;
};
}  // namespace tgfx