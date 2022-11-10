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

#include "gpu/Gpu.h"
#include "gpu/opengl/GLOpsRenderPass.h"

namespace tgfx {
class GLGpu : public Gpu {
 public:
  static std::unique_ptr<Gpu> Make(Context* context);

  void copyRenderTargetToTexture(RenderTarget* renderTarget, Texture* texture, const Rect& srcRect,
                                 const Point& dstPoint) override;

  void resolveRenderTarget(RenderTarget* renderTarget) override;

  bool insertSemaphore(Semaphore* semaphore) override;

  bool waitSemaphore(const Semaphore* semaphore) override;

  OpsRenderPass* getOpsRenderPass(std::shared_ptr<RenderTarget> renderTarget,
                                  std::shared_ptr<Texture> renderTargetTexture) override;

  void submit(OpsRenderPass* renderPass) override;

 private:
  explicit GLGpu(Context* context) : Gpu(context) {
  }

  std::unique_ptr<GLOpsRenderPass> opsRenderPass;
};
}  // namespace tgfx
