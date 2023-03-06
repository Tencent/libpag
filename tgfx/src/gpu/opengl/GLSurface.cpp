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

#include "GLSurface.h"
#include "GLCaps.h"
#include "GLContext.h"
#include "core/PixelBuffer.h"

namespace tgfx {
std::shared_ptr<Surface> Surface::MakeFrom(std::shared_ptr<RenderTarget> renderTarget,
                                           const SurfaceOptions* options) {
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto glRT = std::static_pointer_cast<GLRenderTarget>(renderTarget);
  return std::shared_ptr<GLSurface>(new GLSurface(std::move(glRT), nullptr, options));
}

std::shared_ptr<Surface> Surface::MakeFrom(std::shared_ptr<Texture> texture, int sampleCount,
                                           const SurfaceOptions* options) {
  if (texture == nullptr || texture->isYUV()) {
    return nullptr;
  }
  auto glTexture = std::static_pointer_cast<GLTexture>(texture);
  auto renderTarget = GLRenderTarget::MakeFrom(glTexture.get(), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto surface = new GLSurface(renderTarget, glTexture, options);
  return std::shared_ptr<GLSurface>(surface);
}

std::shared_ptr<Surface> Surface::Make(Context* context, int width, int height, bool alphaOnly,
                                       int sampleCount, bool mipMapped,
                                       const SurfaceOptions* options) {
  auto caps = GLCaps::Get(context);
  if (alphaOnly && !caps->textureRedSupport) {
    return nullptr;
  }
  std::shared_ptr<Texture> texture = nullptr;
  if (texture == nullptr) {
    if (alphaOnly) {
      texture = Texture::MakeAlpha(context, width, height, SurfaceOrigin::TopLeft, mipMapped);
    } else {
      texture = Texture::MakeRGBA(context, width, height, SurfaceOrigin::TopLeft, mipMapped);
    }
  }
  if (texture == nullptr) {
    return nullptr;
  }
  auto pixelFormat = alphaOnly ? PixelFormat::ALPHA_8 : PixelFormat::RGBA_8888;
  sampleCount = caps->getSampleCount(sampleCount, pixelFormat);
  auto renderTarget = GLRenderTarget::MakeFrom(static_cast<GLTexture*>(texture.get()), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto surface = new GLSurface(renderTarget, std::static_pointer_cast<GLTexture>(texture), options);
  // 对于内部创建的 RenderTarget 默认清屏。
  surface->getCanvas()->clear();
  return std::shared_ptr<Surface>(surface);
}

GLSurface::GLSurface(std::shared_ptr<GLRenderTarget> renderTarget,
                     std::shared_ptr<GLTexture> texture, const SurfaceOptions* options)
    : Surface(std::move(renderTarget), std::move(texture), options) {
  requiresManualMSAAResolve =
      GLCaps::Get(this->renderTarget->getContext())->usesMSAARenderBuffers();
}

bool GLSurface::onReadPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) {
  return std::static_pointer_cast<GLRenderTarget>(renderTarget)
      ->readPixels(dstInfo, dstPixels, srcX, srcY);
}
}  // namespace tgfx
