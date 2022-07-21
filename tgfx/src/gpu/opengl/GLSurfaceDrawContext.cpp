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
#include "GLFillRectOp.h"

namespace tgfx {
std::shared_ptr<SurfaceDrawContext> SurfaceDrawContext::Make(Surface* surface) {
  return std::make_shared<GLSurfaceDrawContext>(surface);
}

void GLSurfaceDrawContext::fillRectWithFP(const Rect& dstRect, const Matrix& localMatrix,
                                          std::unique_ptr<FragmentProcessor> fp) {
  DrawArgs args;
  args.colors.emplace_back(std::move(fp));
  args.context = surface->getContext();
  args.renderTarget = surface->getRenderTarget().get();
  args.renderTargetTexture = surface->getTexture();
  draw(std::move(args), GLFillRectOp::Make(dstRect, Matrix::I(), localMatrix));
}

GLDrawer* GLSurfaceDrawContext::getDrawer() {
  if (_drawer == nullptr) {
    _drawer = GLDrawer::Make(surface->getContext());
  }
  return _drawer.get();
}

void GLSurfaceDrawContext::draw(DrawArgs args, std::unique_ptr<GLDrawOp> op) {
  auto* drawer = getDrawer();
  if (drawer == nullptr) {
    return;
  }
  drawer->draw(std::move(args), std::move(op));
}
}  // namespace tgfx
