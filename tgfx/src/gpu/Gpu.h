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

#include <memory>
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/Semaphore.h"

namespace tgfx {
class OpsRenderPass;

class RenderTarget;

class Texture;

class Gpu {
 public:
  virtual ~Gpu() = default;

  Context* context() {
    return _context;
  }

  virtual void writePixels(const Texture* texture, Rect rect, const void* pixels,
                           size_t rowBytes) = 0;

  virtual void copyRenderTargetToTexture(RenderTarget* renderTarget, Texture* texture,
                                         const Rect& srcRect, const Point& dstPoint) = 0;

  virtual void resolveRenderTarget(RenderTarget* renderTarget) = 0;

  virtual bool insertSemaphore(Semaphore* semaphore) = 0;

  virtual bool waitSemaphore(const Semaphore* semaphore) = 0;

  virtual OpsRenderPass* getOpsRenderPass(std::shared_ptr<RenderTarget> renderTarget,
                                          std::shared_ptr<Texture> renderTargetTexture) = 0;

  virtual void submit(OpsRenderPass* opsRenderPass) = 0;

 protected:
  explicit Gpu(Context* context) : _context(context) {
  }

  Context* _context;
};
}  // namespace tgfx
