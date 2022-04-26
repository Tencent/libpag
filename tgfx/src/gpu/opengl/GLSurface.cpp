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
#include "tgfx/gpu/opengl/GLSemaphore.h"

namespace tgfx {
std::shared_ptr<Surface> Surface::MakeFrom(std::shared_ptr<RenderTarget> renderTarget) {
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto glRT = std::static_pointer_cast<GLRenderTarget>(renderTarget);
  return std::shared_ptr<GLSurface>(new GLSurface(std::move(glRT)));
}

std::shared_ptr<Surface> Surface::MakeFrom(std::shared_ptr<Texture> texture, int sampleCount) {
  if (texture == nullptr || texture->isYUV()) {
    return nullptr;
  }
  auto glTexture = std::static_pointer_cast<GLTexture>(texture);
  auto renderTarget = GLRenderTarget::MakeFrom(glTexture.get(), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto surface = new GLSurface(renderTarget, glTexture);
  return std::shared_ptr<GLSurface>(surface);
}

std::shared_ptr<Surface> Surface::Make(Context* context, int width, int height, bool alphaOnly,
                                       int sampleCount) {
  auto pixelFormat = alphaOnly ? PixelFormat::ALPHA_8 : PixelFormat::RGBA_8888;
  std::shared_ptr<GLTexture> texture;
  if (alphaOnly) {
    if (GLCaps::Get(context)->textureRedSupport) {
      texture = std::static_pointer_cast<GLTexture>(Texture::MakeAlpha(context, width, height));
    }
  } else {
    texture = std::static_pointer_cast<GLTexture>(Texture::MakeRGBA(context, width, height));
  }
  if (texture == nullptr) {
    return nullptr;
  }
  auto caps = GLCaps::Get(context);
  sampleCount = caps->getSampleCount(sampleCount, pixelFormat);
  auto renderTarget = GLRenderTarget::MakeFrom(texture.get(), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto surface = new GLSurface(renderTarget, texture);
  // 对于内部创建的 RenderTarget 默认清屏。
  surface->getCanvas()->clear();
  return std::shared_ptr<Surface>(surface);
}

GLSurface::GLSurface(std::shared_ptr<GLRenderTarget> renderTarget,
                     std::shared_ptr<GLTexture> texture)
    : Surface(renderTarget->getContext()),
      renderTarget(std::move(renderTarget)),
      texture(std::move(texture)) {
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

bool GLSurface::wait(const Semaphore* semaphore) {
  auto glSync = static_cast<const GLSemaphore*>(semaphore)->glSync;
  if (glSync == nullptr) {
    return false;
  }
  auto caps = GLCaps::Get(context);
  if (!caps->semaphoreSupport) {
    return false;
  }
  auto gl = GLFunctions::Get(context);
  gl->waitSync(glSync, 0, GL_TIMEOUT_IGNORED);
  gl->deleteSync(glSync);
  return true;
}

bool GLSurface::flush(Semaphore* semaphore) {
  if (semaphore == nullptr) {
    if (canvas) {
      canvas->flush();
    }
    return false;
  }
  auto caps = GLCaps::Get(context);
  if (!caps->semaphoreSupport) {
    return false;
  }
  auto gl = GLFunctions::Get(context);
  auto* sync = gl->fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (sync) {
    static_cast<GLSemaphore*>(semaphore)->glSync = sync;
    // If we inserted semaphores during the flush, we need to call glFlush.
    gl->flush();
    return true;
  }
  return false;
}

std::shared_ptr<Texture> GLSurface::getTexture() const {
  if (canvas) {
    canvas->flush();
  }
  renderTarget->resolve();
  return texture;
}

bool GLSurface::onReadPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const {
  if (canvas) {
    canvas->flush();
  }
  renderTarget->resolve();
  return renderTarget->readPixels(dstInfo, dstPixels, srcX, srcY);
}
}  // namespace tgfx
