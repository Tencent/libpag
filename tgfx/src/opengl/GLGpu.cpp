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
#include "GLUtil.h"
#include "opengl/GLRenderTarget.h"
#include "opengl/GLSemaphore.h"
#include "utils/PixelFormatUtil.h"

namespace tgfx {
std::unique_ptr<Gpu> GLGpu::Make(Context* context) {
  return std::unique_ptr<GLGpu>(new GLGpu(context));
}

std::unique_ptr<TextureSampler> GLGpu::createSampler(int width, int height, PixelFormat format,
                                                     int mipLevelCount) {
  // Texture memory must be allocated first on the web platform then can write pixels.
  DEBUG_ASSERT(mipLevelCount > 0);
  // Clear the previously generated GLError, causing the subsequent CheckGLError to return an
  // incorrect result.
  CheckGLError(_context);
  auto gl = GLFunctions::Get(_context);
  auto sampler = std::make_unique<GLSampler>();
  gl->genTextures(1, &(sampler->id));
  if (sampler->id == 0) {
    return nullptr;
  }
  sampler->target = GL_TEXTURE_2D;
  sampler->format = format;
  sampler->maxMipMapLevel = mipLevelCount - 1;
  gl->bindTexture(sampler->target, sampler->id);
  gl->texParameteri(sampler->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(sampler->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(sampler->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(sampler->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  const auto& textureFormat = GLCaps::Get(_context)->getTextureFormat(format);
  bool success = true;
  for (int level = 0; level < mipLevelCount && success; level++) {
    const int twoToTheMipLevel = 1 << level;
    const int currentWidth = std::max(1, width / twoToTheMipLevel);
    const int currentHeight = std::max(1, height / twoToTheMipLevel);
    gl->texImage2D(sampler->target, level, static_cast<int>(textureFormat.internalFormatTexImage),
                   currentWidth, currentHeight, 0, textureFormat.externalFormat, GL_UNSIGNED_BYTE,
                   nullptr);
    success = CheckGLError(_context);
  }
  if (!success) {
    gl->deleteTextures(1, &(sampler->id));
    return nullptr;
  }
  return sampler;
}

void GLGpu::deleteSampler(TextureSampler* sampler) {
  auto glSampler = static_cast<GLSampler*>(sampler);
  if (glSampler == nullptr || glSampler->id == 0) {
    return;
  }
  GLFunctions::Get(_context)->deleteTextures(1, &glSampler->id);
  glSampler->id = 0;
}

void GLGpu::writePixels(const TextureSampler* sampler, Rect rect, const void* pixels,
                        size_t rowBytes) {
  if (sampler == nullptr) {
    return;
  }
  auto gl = GLFunctions::Get(_context);
  auto caps = GLCaps::Get(_context);
  auto glSampler = static_cast<const GLSampler*>(sampler);
  gl->bindTexture(glSampler->target, glSampler->id);
  const auto& format = caps->getTextureFormat(sampler->format);
  auto bytesPerPixel = PixelFormatBytesPerPixel(sampler->format);
  gl->pixelStorei(GL_UNPACK_ALIGNMENT, static_cast<int>(bytesPerPixel));
  int x = static_cast<int>(rect.x());
  int y = static_cast<int>(rect.y());
  int width = static_cast<int>(rect.width());
  int height = static_cast<int>(rect.height());
  if (caps->unpackRowLengthSupport) {
    // the number of pixels, not bytes
    gl->pixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<int>(rowBytes / bytesPerPixel));
    gl->texSubImage2D(glSampler->target, 0, x, y, width, height, format.externalFormat,
                      GL_UNSIGNED_BYTE, pixels);
    gl->pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  } else {
    if (static_cast<size_t>(width) * bytesPerPixel == rowBytes) {
      gl->texSubImage2D(glSampler->target, 0, x, y, width, height, format.externalFormat,
                        GL_UNSIGNED_BYTE, pixels);
    } else {
      auto data = reinterpret_cast<const uint8_t*>(pixels);
      for (int row = 0; row < height; ++row) {
        gl->texSubImage2D(glSampler->target, 0, x, y + row, width, 1, format.externalFormat,
                          GL_UNSIGNED_BYTE, data + (row * rowBytes));
      }
    }
  }
  if (sampler->hasMipmaps()) {
    onRegenerateMipMapLevels(sampler);
  }
}

static int FilterToGLMagFilter(FilterMode filterMode) {
  switch (filterMode) {
    case FilterMode::Nearest:
      return GL_NEAREST;
    case FilterMode::Linear:
      return GL_LINEAR;
  }
}

static int FilterToGLMinFilter(FilterMode filterMode, MipMapMode mipMapMode) {
  switch (mipMapMode) {
    case MipMapMode::None:
      return FilterToGLMagFilter(filterMode);
    case MipMapMode::Nearest:
      switch (filterMode) {
        case FilterMode::Nearest:
          return GL_NEAREST_MIPMAP_NEAREST;
        case FilterMode::Linear:
          return GL_LINEAR_MIPMAP_NEAREST;
      }
    case MipMapMode::Linear:
      switch (filterMode) {
        case FilterMode::Nearest:
          return GL_NEAREST_MIPMAP_LINEAR;
        case FilterMode::Linear:
          return GL_LINEAR_MIPMAP_LINEAR;
      }
  }
}

static int GetGLWrap(unsigned target, SamplerState::WrapMode wrapMode) {
  if (target == GL_TEXTURE_RECTANGLE) {
    if (wrapMode == SamplerState::WrapMode::ClampToBorder) {
      return GL_CLAMP_TO_BORDER;
    } else {
      return GL_CLAMP_TO_EDGE;
    }
  }
  switch (wrapMode) {
    case SamplerState::WrapMode::Clamp:
      return GL_CLAMP_TO_EDGE;
    case SamplerState::WrapMode::Repeat:
      return GL_REPEAT;
    case SamplerState::WrapMode::MirrorRepeat:
      return GL_MIRRORED_REPEAT;
    case SamplerState::WrapMode::ClampToBorder:
      return GL_CLAMP_TO_BORDER;
  }
}

void GLGpu::bindTexture(int unitIndex, const TextureSampler* sampler, SamplerState samplerState) {
  if (sampler == nullptr) {
    return;
  }
  auto glSampler = static_cast<const GLSampler*>(sampler);
  auto gl = GLFunctions::Get(_context);
  gl->activeTexture(GL_TEXTURE0 + unitIndex);
  gl->bindTexture(glSampler->target, glSampler->id);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_S,
                    GetGLWrap(glSampler->target, samplerState.wrapModeX));
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_T,
                    GetGLWrap(glSampler->target, samplerState.wrapModeY));
  if (samplerState.mipMapped() && (!_context->caps()->mipMapSupport || !glSampler->hasMipmaps())) {
    samplerState.mipMapMode = MipMapMode::None;
  }
  gl->texParameteri(glSampler->target, GL_TEXTURE_MIN_FILTER,
                    FilterToGLMinFilter(samplerState.filterMode, samplerState.mipMapMode));
  gl->texParameteri(glSampler->target, GL_TEXTURE_MAG_FILTER,
                    FilterToGLMagFilter(samplerState.filterMode));
}

void GLGpu::copyRenderTargetToTexture(const RenderTarget* renderTarget, Texture* texture,
                                      const Rect& srcRect, const Point& dstPoint) {
  auto gl = GLFunctions::Get(_context);
  auto glRenderTarget = static_cast<const GLRenderTarget*>(renderTarget);
  gl->bindFramebuffer(GL_FRAMEBUFFER, glRenderTarget->getFrameBufferID(false));
  auto glSampler = static_cast<const GLSampler*>(texture->getSampler());
  gl->bindTexture(glSampler->target, glSampler->id);
  // format != BGRA && !srcHasMSAARenderBuffer && !dstHasMSAARenderBuffer && dstIsTextureable &&
  // dstOrigin == srcOrigin && canConfigBeFBOColorAttachment(srcConfig) && (!srcIsTextureable ||
  // srcIsGLTexture2D)
  gl->copyTexSubImage2D(glSampler->target, 0, static_cast<int>(dstPoint.x),
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

bool GLGpu::submitToGpu(bool syncCpu) {
  auto gl = GLFunctions::Get(_context);
  if (syncCpu) {
    gl->finish();
  } else {
    gl->flush();
  }
  return true;
}

void GLGpu::submit(OpsRenderPass*) {
  opsRenderPass->reset();
}

void GLGpu::onRegenerateMipMapLevels(const TextureSampler* sampler) {
  auto gl = GLFunctions::Get(_context);
  auto glSampler = static_cast<const GLSampler*>(sampler);
  if (glSampler->target != GL_TEXTURE_2D) {
    return;
  }
  gl->bindTexture(glSampler->target, glSampler->id);
  gl->generateMipmap(glSampler->target);
}
}  // namespace tgfx
