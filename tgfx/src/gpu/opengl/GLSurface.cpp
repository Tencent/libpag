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
#include "GLUtil.h"

namespace pag {
std::shared_ptr<Surface> Surface::MakeFrom(Context* context,
                                           const BackendRenderTarget& renderTarget,
                                           ImageOrigin origin) {
  auto rt = GLRenderTarget::MakeFrom(context, renderTarget, origin);
  return GLSurface::MakeFrom(context, std::move(rt));
}

std::shared_ptr<Surface> Surface::MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           ImageOrigin origin) {
  auto texture =
      std::static_pointer_cast<GLTexture>(Texture::MakeFrom(context, backendTexture, origin));
  return GLSurface::MakeFrom(context, std::move(texture));
}

std::shared_ptr<Surface> Surface::Make(Context* context, int width, int height, bool alphaOnly,
                                       int sampleCount) {
  auto config = alphaOnly ? PixelConfig::ALPHA_8 : PixelConfig::RGBA_8888;
  std::shared_ptr<GLTexture> texture;
  if (alphaOnly) {
    if (GLContext::Unwrap(context)->caps->textureRedSupport) {
      texture = GLTexture::MakeAlpha(context, width, height);
    }
  } else {
    texture = GLTexture::MakeRGBA(context, width, height);
  }
  if (texture == nullptr) {
    return nullptr;
  }
  auto gl = GLContext::Unwrap(context);
  sampleCount = gl->caps->getSampleCount(sampleCount, config);
  auto renderTarget = GLRenderTarget::MakeFrom(context, texture.get(), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  // 对于内部创建的 RenderTarget 默认清屏。
  auto surface = new GLSurface(context, renderTarget, texture);
  surface->getCanvas()->clear();
  return std::shared_ptr<Surface>(surface);
}

std::shared_ptr<GLSurface> GLSurface::MakeFrom(Context* context,
                                               std::shared_ptr<GLRenderTarget> renderTarget) {
  if (renderTarget == nullptr || context == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<GLSurface>(new GLSurface(context, std::move(renderTarget)));
}

std::shared_ptr<GLSurface> GLSurface::MakeFrom(Context* context,
                                               std::shared_ptr<GLTexture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto renderTarget = GLRenderTarget::MakeFrom(context, texture.get());
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto surface = new GLSurface(context, renderTarget, texture);
  return std::shared_ptr<GLSurface>(surface);
}

GLSurface::GLSurface(Context* context, std::shared_ptr<GLRenderTarget> renderTarget,
                     std::shared_ptr<GLTexture> texture)
    : Surface(context), renderTarget(std::move(renderTarget)), texture(std::move(texture)) {
}

GLSurface::~GLSurface() {
  delete canvas;
}

Canvas* GLSurface::getCanvas() {
  if (canvas == nullptr) {
    canvas = new GLCanvas(this);
  }
  return canvas;
}

bool GLSurface::wait(const BackendSemaphore& semaphore) {
  if (semaphore.glSync() == nullptr) {
    return false;
  }
  const auto* gl = GLContext::Unwrap(getContext());
  if (!gl->caps->semaphoreSupport) {
    return false;
  }
  gl->waitSync(semaphore.glSync(), 0, GL::TIMEOUT_IGNORED);
  gl->deleteSync(semaphore.glSync());
  return true;
}

bool GLSurface::flush(BackendSemaphore* semaphore) {
  if (semaphore == nullptr) {
    if (canvas) {
      canvas->flush();
    }
    return false;
  }
  const auto* gl = GLContext::Unwrap(getContext());
  if (!gl->caps->semaphoreSupport) {
    return false;
  }
  auto* sync = gl->fenceSync(GL::SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (sync) {
    semaphore->initGL(sync);
    // If we inserted semaphores during the flush, we need to call glFlush.
    gl->flush();
    return true;
  }
  return false;
}

std::shared_ptr<GLRenderTarget> GLSurface::getRenderTarget() const {
  return renderTarget;
}

std::shared_ptr<Texture> GLSurface::getTexture() const {
  if (canvas) {
    canvas->flush();
  }
  renderTarget->resolve(getContext());
  return texture;
}

bool GLSurface::onReadPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const {
  if (canvas) {
    canvas->flush();
  }
  auto context = getContext();
  renderTarget->resolve(context);
  return renderTarget->readPixels(context, dstInfo, dstPixels, srcX, srcY);
}
}  // namespace pag
