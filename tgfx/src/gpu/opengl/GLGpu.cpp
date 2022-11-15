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

#include "GLGpu.h"
#include "gpu/PixelFormat.h"
#include "tgfx/gpu/opengl/GLSemaphore.h"

namespace tgfx {
std::unique_ptr<Gpu> GLGpu::Make(Context* context) {
  return std::unique_ptr<GLGpu>(new GLGpu(context));
}

void GLGpu::writePixels(const Texture* texture, Rect rect, const void* pixels, size_t rowBytes) {
  auto gl = GLFunctions::Get(_context);
  auto caps = GLCaps::Get(_context);
  auto glSampler = static_cast<const GLTexture*>(texture)->glSampler();
  gl->bindTexture(glSampler.target, glSampler.id);
  const auto& format = caps->getTextureFormat(glSampler.format);
  auto bytesPerPixel = PixelFormatBytesPerPixel(glSampler.format);
  gl->pixelStorei(GL_UNPACK_ALIGNMENT, static_cast<int>(bytesPerPixel));
  int x = static_cast<int>(rect.x());
  int y = static_cast<int>(rect.y());
  int width = static_cast<int>(rect.width());
  int height = static_cast<int>(rect.height());
  if (caps->unpackRowLengthSupport) {
    // the number of pixels, not bytes
    gl->pixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<int>(rowBytes / bytesPerPixel));
    gl->texSubImage2D(glSampler.target, 0, x, y, width, height, format.externalFormat,
                      GL_UNSIGNED_BYTE, pixels);
    gl->pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  } else {
    if (static_cast<size_t>(width) * bytesPerPixel == rowBytes) {
      gl->texSubImage2D(glSampler.target, 0, x, y, width, height, format.externalFormat,
                        GL_UNSIGNED_BYTE, pixels);
    } else {
      auto data = reinterpret_cast<const uint8_t*>(pixels);
      for (int row = 0; row < height; ++row) {
        gl->texSubImage2D(glSampler.target, 0, x, y + row, width, 1, format.externalFormat,
                          GL_UNSIGNED_BYTE, data + (row * rowBytes));
      }
    }
  }
}

void GLGpu::copyRenderTargetToTexture(RenderTarget* renderTarget, Texture* texture,
                                      const Rect& srcRect, const Point& dstPoint) {
  auto gl = GLFunctions::Get(_context);
  auto glRenderTarget = static_cast<GLRenderTarget*>(renderTarget);
  gl->bindFramebuffer(GL_FRAMEBUFFER, glRenderTarget->glFrameBuffer().id);
  auto glSampler = static_cast<GLTexture*>(texture)->glSampler();
  gl->bindTexture(glSampler.target, glSampler.id);
  // format != BGRA && !srcHasMSAARenderBuffer && !dstHasMSAARenderBuffer && dstIsTextureable &&
  // dstOrigin == srcOrigin && canConfigBeFBOColorAttachment(srcConfig) && (!srcIsTextureable ||
  // srcIsGLTexture2D)
  gl->copyTexSubImage2D(glSampler.target, 0, static_cast<int>(dstPoint.x),
                        static_cast<int>(dstPoint.y), static_cast<int>(srcRect.x()),
                        static_cast<int>(srcRect.y()), static_cast<int>(srcRect.width()),
                        static_cast<int>(srcRect.height()));
}

void GLGpu::resolveRenderTarget(RenderTarget* renderTarget) {
  static_cast<GLRenderTarget*>(renderTarget)->resolve();
}

bool GLGpu::insertSemaphore(Semaphore* semaphore) {
  if (semaphore == nullptr) {
    return false;
  }
  auto gl = GLFunctions::Get(_context);
  auto* sync = gl->fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (sync) {
    static_cast<GLSemaphore*>(semaphore)->glSync = sync;
    // If we inserted semaphores during the flush, we need to call glFlush.
    gl->flush();
    return true;
  }
  return false;
}

bool GLGpu::waitSemaphore(const Semaphore* semaphore) {
  auto glSync = static_cast<const GLSemaphore*>(semaphore)->glSync;
  if (glSync == nullptr) {
    return false;
  }
  auto gl = GLFunctions::Get(_context);
  gl->waitSync(glSync, 0, GL_TIMEOUT_IGNORED);
  gl->deleteSync(glSync);
  return true;
}

OpsRenderPass* GLGpu::getOpsRenderPass(std::shared_ptr<RenderTarget> renderTarget,
                                       std::shared_ptr<Texture> renderTargetTexture) {
  if (opsRenderPass == nullptr) {
    opsRenderPass = GLOpsRenderPass::Make(_context);
  }
  if (opsRenderPass) {
    opsRenderPass->set(renderTarget, renderTargetTexture);
  }
  return opsRenderPass.get();
}

void GLGpu::submit(OpsRenderPass*) {
  opsRenderPass->reset();
}
}  // namespace tgfx
