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

#include "GLRenderTarget.h"
#include "gpu/TextureSampler.h"
#include "opengl/GLContext.h"
#include "opengl/GLSampler.h"
#include "opengl/GLUtil.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Pixmap.h"
#include "utils/PixelFormatUtil.h"

namespace tgfx {
std::shared_ptr<RenderTarget> RenderTarget::MakeFrom(Context* context,
                                                     const BackendRenderTarget& renderTarget,
                                                     ImageOrigin origin) {
  if (context == nullptr || !renderTarget.isValid()) {
    return nullptr;
  }
  GLFrameBufferInfo frameBufferInfo = {};
  if (!renderTarget.getGLFramebufferInfo(&frameBufferInfo)) {
    return nullptr;
  }
  GLFrameBuffer frameBuffer = {};
  frameBuffer.id = frameBufferInfo.id;
  frameBuffer.format = GLSizeFormatToPixelFormat(frameBufferInfo.format);
  auto target =
      new GLRenderTarget(renderTarget.width(), renderTarget.height(), origin, 1, frameBuffer);
  target->frameBufferForDraw = frameBuffer;
  target->externalResource = true;
  return Resource::Wrap(context, target);
}

static bool RenderbufferStorageMSAA(Context* context, int sampleCount, PixelFormat pixelFormat,
                                    int width, int height) {
  CheckGLError(context);
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  auto format = caps->getTextureFormat(pixelFormat).sizedFormat;
  switch (caps->msFBOType) {
    case MSFBOType::Standard:
      gl->renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, format, width, height);
      break;
    case MSFBOType::ES_Apple:
      gl->renderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, sampleCount, format, width, height);
      break;
    case MSFBOType::ES_EXT_MsToTexture:
    case MSFBOType::ES_IMG_MsToTexture:
      gl->renderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sampleCount, format, width, height);
      break;
    case MSFBOType::None:
      LOGE("Shouldn't be here if we don't support multisampled renderbuffers.");
      break;
  }
  return CheckGLError(context);
}

static void FrameBufferTexture2D(Context* context, unsigned textureTarget, unsigned textureID,
                                 int sampleCount) {
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  // 解绑的时候framebufferTexture2DMultisample在华为手机上会出现crash，统一走framebufferTexture2D解绑
  if (textureID != 0 && sampleCount > 1 && caps->usesImplicitMSAAResolve()) {
    gl->framebufferTexture2DMultisample(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget,
                                        textureID, 0, sampleCount);
  } else {
    gl->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, textureID, 0);
  }
}

static void ReleaseResource(Context* context, GLFrameBuffer* textureFBInfo,
                            GLFrameBuffer* renderTargetFBInfo = nullptr,
                            unsigned* msRenderBufferID = nullptr) {
  auto gl = GLFunctions::Get(context);
  if (textureFBInfo && textureFBInfo->id) {
    gl->deleteFramebuffers(1, &(textureFBInfo->id));
    if (renderTargetFBInfo && renderTargetFBInfo->id == textureFBInfo->id) {
      renderTargetFBInfo->id = 0;
    }
    textureFBInfo->id = 0;
  }
  if (renderTargetFBInfo && renderTargetFBInfo->id > 0) {
    gl->bindFramebuffer(GL_FRAMEBUFFER, renderTargetFBInfo->id);
    gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    gl->bindFramebuffer(GL_FRAMEBUFFER, 0);
    gl->deleteFramebuffers(1, &(renderTargetFBInfo->id));
    renderTargetFBInfo->id = 0;
  }
  if (msRenderBufferID && *msRenderBufferID > 0) {
    gl->deleteRenderbuffers(1, msRenderBufferID);
    *msRenderBufferID = 0;
  }
}

static bool CreateRenderBuffer(const Texture* texture, GLFrameBuffer* renderTargetFBInfo,
                               unsigned* msRenderBufferID, int sampleCount) {
  auto gl = GLFunctions::Get(texture->getContext());
  gl->genFramebuffers(1, &(renderTargetFBInfo->id));
  if (renderTargetFBInfo->id == 0) {
    return false;
  }
  gl->genRenderbuffers(1, msRenderBufferID);
  if (*msRenderBufferID == 0) {
    return false;
  }
  gl->bindRenderbuffer(GL_RENDERBUFFER, *msRenderBufferID);
  if (!RenderbufferStorageMSAA(texture->getContext(), sampleCount, renderTargetFBInfo->format,
                               texture->width(), texture->height())) {
    return false;
  }
  gl->bindFramebuffer(GL_FRAMEBUFFER, renderTargetFBInfo->id);
  gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              *msRenderBufferID);
#ifdef TGFX_BUILD_FOR_WEB
  return true;
#else
  return gl->checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
#endif
}

std::shared_ptr<RenderTarget> RenderTarget::MakeFrom(const Texture* texture, int sampleCount) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto context = texture->getContext();
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  auto glSampler = static_cast<const GLSampler*>(texture->getSampler());
  GLFrameBuffer textureFBInfo = {};
  textureFBInfo.format = glSampler->format;
  gl->genFramebuffers(1, &textureFBInfo.id);
  if (textureFBInfo.id == 0) {
    return nullptr;
  }
  GLFrameBuffer renderTargetFBInfo = {};
  renderTargetFBInfo.format = glSampler->format;
  unsigned msRenderBufferID = 0;
  if (sampleCount > 1 && caps->usesMSAARenderBuffers()) {
    if (!CreateRenderBuffer(texture, &renderTargetFBInfo, &msRenderBufferID, sampleCount)) {
      ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
      return nullptr;
    }
  } else {
    renderTargetFBInfo = textureFBInfo;
  }
  gl->bindFramebuffer(GL_FRAMEBUFFER, textureFBInfo.id);
  FrameBufferTexture2D(context, glSampler->target, glSampler->id, sampleCount);
#ifndef TGFX_BUILD_FOR_WEB
  if (gl->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
    return nullptr;
  }
#endif
  auto rt = new GLRenderTarget(texture->width(), texture->height(), texture->origin(), sampleCount,
                               textureFBInfo, glSampler->target);
  rt->frameBufferForDraw = renderTargetFBInfo;
  rt->msRenderBufferID = msRenderBufferID;
  return Resource::Wrap(context, rt);
}

GLRenderTarget::GLRenderTarget(int width, int height, ImageOrigin origin, int sampleCount,
                               GLFrameBuffer frameBuffer, unsigned textureTarget)
    : RenderTarget(width, height, origin, sampleCount),
      frameBufferForRead(frameBuffer),
      textureTarget(textureTarget) {
}

static bool CanReadDirectly(Context* context, ImageOrigin origin, const ImageInfo& srcInfo,
                            const ImageInfo& dstInfo) {
  if (origin != ImageOrigin::TopLeft || dstInfo.alphaType() != srcInfo.alphaType() ||
      dstInfo.colorType() != srcInfo.colorType()) {
    return false;
  }
  auto caps = GLCaps::Get(context);
  if (dstInfo.rowBytes() != dstInfo.minRowBytes() && !caps->packRowLengthSupport) {
    return false;
  }
  return true;
}

static void CopyPixels(const ImageInfo& srcInfo, const void* srcPixels, const ImageInfo& dstInfo,
                       void* dstPixels, bool flipY) {
  auto pixels = srcPixels;
  Buffer tempBuffer = {};
  if (flipY) {
    tempBuffer.alloc(srcInfo.byteSize());
    auto rowCount = srcInfo.height();
    auto rowBytes = srcInfo.rowBytes();
    auto dst = tempBuffer.bytes();
    for (int i = 0; i < rowCount; i++) {
      auto src = reinterpret_cast<const uint8_t*>(srcPixels) + (rowCount - i - 1) * rowBytes;
      memcpy(dst, src, rowBytes);
      dst += rowBytes;
    }
    pixels = tempBuffer.data();
  }
  Pixmap pixmap(srcInfo, pixels);
  pixmap.readPixels(dstInfo, dstPixels);
}

BackendRenderTarget GLRenderTarget::getBackendRenderTarget() const {
  GLFrameBufferInfo glInfo = {};
  glInfo.id = frameBufferForDraw.id;
  glInfo.format = PixelFormatToGLSizeFormat(frameBufferForDraw.format);
  return {glInfo, width(), height()};
}

bool GLRenderTarget::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX,
                                int srcY) const {
  dstPixels = dstInfo.computeOffset(dstPixels, -srcX, -srcY);
  auto outInfo = dstInfo.makeIntersect(-srcX, -srcY, width(), height());
  if (outInfo.isEmpty()) {
    return false;
  }
  auto pixelFormat = frameBufferForRead.format;
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  const auto& format = caps->getTextureFormat(pixelFormat);
  gl->bindFramebuffer(GL_FRAMEBUFFER, frameBufferForRead.id);

  auto colorType = PixelFormatToColorType(pixelFormat);
  auto srcInfo =
      ImageInfo::Make(outInfo.width(), outInfo.height(), colorType, AlphaType::Premultiplied);
  void* pixels = nullptr;
  Buffer tempBuffer = {};
  auto restoreGLRowLength = false;
  if (CanReadDirectly(context, origin(), srcInfo, outInfo)) {
    pixels = dstPixels;
    if (outInfo.rowBytes() != outInfo.minRowBytes()) {
      gl->pixelStorei(GL_PACK_ROW_LENGTH,
                      static_cast<int>(outInfo.rowBytes() / outInfo.bytesPerPixel()));
      restoreGLRowLength = true;
    }
  } else {
    tempBuffer.alloc(srcInfo.byteSize());
    pixels = tempBuffer.data();
  }
  auto alignment = pixelFormat == PixelFormat::ALPHA_8 ? 1 : 4;
  gl->pixelStorei(GL_PACK_ALIGNMENT, alignment);
  auto flipY = origin() == ImageOrigin::BottomLeft;
  auto readX = std::max(0, srcX);
  auto readY = std::max(0, srcY);
  if (flipY) {
    readY = height() - readY - outInfo.height();
  }
  gl->readPixels(readX, readY, outInfo.width(), outInfo.height(), format.externalFormat,
                 GL_UNSIGNED_BYTE, pixels);
  if (restoreGLRowLength) {
    gl->pixelStorei(GL_PACK_ROW_LENGTH, 0);
  }
  if (!tempBuffer.isEmpty()) {
    CopyPixels(srcInfo, tempBuffer.data(), outInfo, dstPixels, flipY);
  }
  return true;
}

unsigned GLRenderTarget::getFrameBufferID(bool forDraw) const {
  return forDraw ? frameBufferForDraw.id : frameBufferForRead.id;
}

void GLRenderTarget::resolve() const {
  if (sampleCount() <= 1) {
    return;
  }
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  if (!caps->usesMSAARenderBuffers()) {
    return;
  }
  gl->bindFramebuffer(GL_READ_FRAMEBUFFER, frameBufferForDraw.id);
  gl->bindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBufferForRead.id);
  if (caps->msFBOType == MSFBOType::ES_Apple) {
    // Apple's extension uses the scissor as the blit bounds.
    gl->enable(GL_SCISSOR_TEST);
    gl->scissor(0, 0, width(), height());
    gl->resolveMultisampleFramebuffer();
    gl->disable(GL_SCISSOR_TEST);
  } else {
    // BlitFrameBuffer respects the scissor, so disable it.
    gl->disable(GL_SCISSOR_TEST);
    gl->blitFramebuffer(0, 0, width(), height(), 0, 0, width(), height(), GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);
  }
}

bool GLRenderTarget::replaceTexture(const Texture* texture) {
  if (textureTarget == 0 || texture == nullptr || texture->width() != width() ||
      texture->height() != height() || texture->origin() != origin() ||
      texture->getSampler()->format != format()) {
    return false;
  }
  auto sampler = static_cast<const GLSampler*>(texture->getSampler());
  auto gl = GLFunctions::Get(context);
  gl->bindFramebuffer(GL_FRAMEBUFFER, frameBufferForRead.id);
  FrameBufferTexture2D(context, textureTarget, 0, sampleCount());
  textureTarget = sampler->target;
  FrameBufferTexture2D(context, textureTarget, sampler->id, sampleCount());
  gl->bindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}

void GLRenderTarget::onReleaseGPU() {
  if (externalResource) {
    return;
  }
  if (textureTarget != 0) {
    auto gl = GLFunctions::Get(context);
    gl->bindFramebuffer(GL_FRAMEBUFFER, frameBufferForRead.id);
    FrameBufferTexture2D(context, textureTarget, 0, sampleCount());
    gl->bindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  ReleaseResource(context, &frameBufferForRead, &frameBufferForDraw, &msRenderBufferID);
}

const Swizzle& GLRenderTarget::writeSwizzle() const {
  return context->caps()->getWriteSwizzle(frameBufferForDraw.format);
}
}  // namespace tgfx
