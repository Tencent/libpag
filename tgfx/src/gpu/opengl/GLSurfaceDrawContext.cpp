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

#include "GLSurfaceDrawContext.h"
#include "GLCanvas.h"
#include "GLFillRectOp.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
SurfaceDrawContext* SurfaceDrawContext::Get(Surface* surface) {
  return static_cast<GLCanvas*>(surface->getCanvas())->getSurfaceDrawContext();
}

void GLSurfaceDrawContext::fillRectWithFP(const Rect& dstRect, const Matrix& localMatrix,
                                          std::unique_ptr<FragmentProcessor> fp) {
  auto op = GLFillRectOp::Make(dstRect, Matrix::I(), localMatrix);
  std::vector<std::unique_ptr<FragmentProcessor>> colors;
  colors.emplace_back(std::move(fp));
  op->setColors(std::move(colors));
  addOp(std::move(op));
}

GLDrawer* GLSurfaceDrawContext::getDrawer() {
  if (_drawer == nullptr) {
    _drawer = GLDrawer::Make(surface->getContext());
  }
  return _drawer.get();
}

GLSurfaceDrawContext::~GLSurfaceDrawContext() {
  DEBUG_ASSERT(ops.empty());
}

void GLSurfaceDrawContext::addOp(std::unique_ptr<GLOp> op) {
  if (!ops.empty() && ops.back()->combineIfPossible(op.get())) {
    return;
  }
  ops.emplace_back(std::move(op));
}

void GLSurfaceDrawContext::flush() {
  if (ops.empty()) {
    return;
  }
  auto* drawer = getDrawer();
  if (drawer == nullptr) {
    return;
  }
  auto tempOps = std::move(ops);
  DrawArgs args;
  args.context = surface->getContext();
  args.renderTarget = surface->getRenderTarget();
  args.renderTargetTexture = surface->getTexture();
  args.drawer = drawer;
  drawer->set(args.renderTarget.get());
  for (auto& op : tempOps) {
    op->draw(args);
  }
  drawer->reset();
}
}  // namespace tgfx
