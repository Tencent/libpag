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

#include "SurfaceDrawContext.h"
#include "DrawingManager.h"
#include "gpu/ops/FillRectOp.h"

namespace tgfx {
void SurfaceDrawContext::addOp(std::unique_ptr<Op> op) {
  getOpsTask()->addOp(std::move(op));
}

void SurfaceDrawContext::fillRectWithFP(const Rect& dstRect, const Matrix& localMatrix,
                                        std::unique_ptr<FragmentProcessor> fp) {
  auto op = FillRectOp::Make({}, dstRect, Matrix::I(), localMatrix);
  std::vector<std::unique_ptr<FragmentProcessor>> colors;
  colors.emplace_back(std::move(fp));
  op->setColors(std::move(colors));
  addOp(std::move(op));
}

OpsTask* SurfaceDrawContext::getOpsTask() {
  if (opsTask == nullptr || opsTask->isClosed()) {
    replaceOpsTask();
  }
  return opsTask.get();
}

void SurfaceDrawContext::replaceOpsTask() {
  opsTask = surface->getContext()->drawingManager()->newOpsTask(surface);
}
}  // namespace tgfx
