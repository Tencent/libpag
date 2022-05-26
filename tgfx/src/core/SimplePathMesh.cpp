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

#include "SimplePathMesh.h"
#include "gpu/opengl/GLDrawer.h"
#include "gpu/opengl/GLFillRectOp.h"
#include "gpu/opengl/GLRRectOp.h"

namespace tgfx {
Rect SimplePathMesh::bounds() const {
  if (rect.has_value()) {
    return *rect;
  }
  if (rRect.has_value()) {
    return rRect->rect;
  }
  return Rect::MakeEmpty();
}

std::pair<std::unique_ptr<GLDrawOp>, Matrix> SimplePathMesh::getOp(const Matrix& viewMatrix) const {
  std::unique_ptr<GLDrawOp> drawOp;
  if (rect.has_value()) {
    drawOp = GLFillRectOp::Make(*rect, viewMatrix);
  } else if (rRect.has_value()) {
    drawOp = GLRRectOp::Make(*rRect, viewMatrix);
  } else {
    return {};
  }
  auto bounds = this->bounds();
  auto localMatrix = Matrix::MakeScale(bounds.width(), bounds.height());
  localMatrix.postTranslate(bounds.x(), bounds.y());
  return {std::move(drawOp), localMatrix};
}
}  // namespace tgfx
