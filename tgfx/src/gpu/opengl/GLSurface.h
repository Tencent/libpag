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

#include "GLCanvas.h"
#include "GLRenderTarget.h"
#include "GLTexture.h"
#include "gpu/Surface.h"

namespace tgfx {
class GLSurface : public Surface {
 public:
  static std::shared_ptr<GLSurface> MakeFrom(Context* context,
                                             std::shared_ptr<GLRenderTarget> renderTarget);

  static std::shared_ptr<GLSurface> MakeFrom(Context* context, std::shared_ptr<GLTexture> texture);

  ~GLSurface() override;

  int width() const override {
    return renderTarget->width();
  }

  int height() const override {
    return renderTarget->height();
  }

  ImageOrigin origin() const override {
    return renderTarget->origin();
  }

  Canvas* getCanvas() override;

  bool wait(const BackendSemaphore& semaphore) override;

  bool flush(BackendSemaphore* semaphore) override;

  std::shared_ptr<GLRenderTarget> getRenderTarget() const;

  std::shared_ptr<Texture> getTexture() const override;

 protected:
  bool onReadPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const override;

 private:
  std::shared_ptr<GLRenderTarget> renderTarget = nullptr;
  std::shared_ptr<GLTexture> texture = nullptr;
  GLCanvas* canvas = nullptr;

  GLSurface(Context* context, std::shared_ptr<GLRenderTarget> renderTarget,
            std::shared_ptr<GLTexture> texture = nullptr);

  friend class Surface;
};
}  // namespace tgfx
