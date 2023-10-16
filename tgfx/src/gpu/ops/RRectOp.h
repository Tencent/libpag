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

#include "DrawOp.h"
#include "tgfx/core/Path.h"

namespace tgfx {
class RRectOp : public DrawOp {
 public:
  static std::unique_ptr<RRectOp> Make(Color color, const RRect& rRect, const Matrix& viewMatrix);

 private:
  DEFINE_OP_CLASS_ID

  RRectOp(Color color, const RRect& rRect, const Matrix& viewMatrix, const Matrix& localMatrix);

  bool onCombineIfPossible(Op* op) override;

  void onPrepare(Gpu* gpu) override;

  void onExecute(RenderPass* renderPass) override;

  struct RRectWrap {
    Color color = Color::Transparent();
    float innerXRadius = 0;
    float innerYRadius = 0;
    RRect rRect;
    Matrix viewMatrix = Matrix::I();

    void writeToVertices(std::vector<float>& vertices, bool useScale, AAType aa) const;
  };

  std::vector<RRectWrap> rRects;
  Matrix localMatrix = Matrix::I();
  std::shared_ptr<GpuBuffer> vertexBuffer;
  std::shared_ptr<GpuBuffer> indexBuffer;

  //  bool stroked = false;
  //  Point strokeWidths = Point::Zero();
};
}  // namespace tgfx
