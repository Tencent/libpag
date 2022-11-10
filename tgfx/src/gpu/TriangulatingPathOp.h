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
#include "gpu/BufferProvider.h"
#include "tgfx/core/Path.h"

namespace tgfx {
class TriangulatingPathOp : public DrawOp {
 public:
  static std::unique_ptr<TriangulatingPathOp> Make(Color color, const Path& path, Rect clipBounds,
                                                     const Matrix& localMatrix);

  TriangulatingPathOp(Color color, std::shared_ptr<BufferProvider> bufferProvider, Rect bounds,
                        const Matrix& viewMatrix = Matrix::I(),
                        const Matrix& localMatrix = Matrix::I());

  void execute(OpsRenderPass* opsRenderPass) override;

 private:
  DEFINE_OP_CLASS_ID

  bool onCombineIfPossible(Op* op) override;

  Color color = Color::Transparent();
  std::vector<std::shared_ptr<BufferProvider>> providers;
  Matrix viewMatrix = Matrix::I();
  Matrix localMatrix = Matrix::I();
};
}  // namespace tgfx
