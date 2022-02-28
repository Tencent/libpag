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

#include "gpu/opengl/GLRenderTarget.h"
#include "core/Bitmap.h"
#include "gpu/opengl/GLContext.h"
#include "gpu/opengl/GLState.h"
#include "gpu/opengl/GLUtil.h"

namespace tgfx {
std::shared_ptr<GLRenderTarget> GLRenderTarget::MakeFrom(Context* context,
                                                         const GLFrameBuffer& frameBuffer,
                                                         int width, int height, ImageOrigin origin,
                                                         int sampleCount) {
  if (context == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto target = new GLRenderTarget(width, height, origin, sampleCount, frameBuffer);
  target->renderTargetFBInfo = frameBuffer;
  target->externalTexture = true;
  return Resource::Wrap(context, target);
}

std::shared_ptr<GLRenderTarget> GLRenderTarget::MakeAdopted(Context* context,
                                                            const GLFrameBuffer& frameBuffer,
                                                            int width, int height,
                                                            ImageOrigin origin, int sampleCount) {
  if (context == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto target = new GLRenderTarget(width, height, origin, sampleCount, frameBuffer);
  target->renderTargetFBInfo = frameBuffer;
  return Resource::Wrap(context, target);
}

static bool RenderbufferStorageMSAA(const GLInterface* gl, int sampleCount, PixelFormat pixelFormat,
                                    int width, int height) {
  CheckGLError(gl);
  auto format = gl->caps->getTextureFormat(pixelFormat).sizedFormat;
  switch (gl->caps->msFBOType) {
    case MSFBOType::Standard:
      gl->functions->renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, format, width,
                                                    height);
      break;
    case MSFBOType::ES_Apple:
      gl->functions->renderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, sampleCount, format,
                                                         width, height);
      break;
    case MSFBOType::ES_EXT_MsToTexture:
    case MSFBOType::ES_IMG_MsToTexture:
      gl->functions->renderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sampleCount, format, width,
                                                       height);
      break;
    case MSFBOType::None:
      LOGE("Shouldn't be here if we don't support multisampled renderbuffers.");
      break;
  }
  return CheckGLError(gl);
}

static void FrameBufferTexture2D(const GLInterface* gl, unsigned textureTarget, unsigned textureID,
                                 int sampleCount) {
  // 解绑的时候framebufferTexture2DMultisample在华为手机上会出现crash，统一走framebufferTexture2D解绑
  if (textureID != 0 && sampleCount > 1 && gl->caps->usesImplicitMSAAResolve()) {
    gl->functions->framebufferTexture2DMultisample(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                   textureTarget, textureID, 0, sampleCount);
  } else {
    gl->functions->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget,
                                        textureID, 0);
  }
}

static void ReleaseResource(Context* context, GLFrameBuffer* textureFBInfo,
                            GLFrameBuffer* renderTargetFBInfo = nullptr,
                            unsigned* msRenderBufferID = nullptr) {
  auto gl = GLInterface::Get(context);
  if (textureFBInfo && textureFBInfo->id) {
    gl->functions->deleteFramebuffers(1, &(textureFBInfo->id));
    if (renderTargetFBInfo && renderTargetFBInfo->id == textureFBInfo->id) {
      renderTargetFBInfo->id = 0;
    }
    textureFBInfo->id = 0;
  }
  if (renderTargetFBInfo && renderTargetFBInfo->id > 0) {
    gl->functions->bindFramebuffer(GL_FRAMEBUFFER, renderTargetFBInfo->id);
    gl->functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                           0);
    gl->functions->bindFramebuffer(GL_FRAMEBUFFER, 0);
    gl->functions->deleteFramebuffers(1, &(renderTargetFBInfo->id));
    renderTargetFBInfo->id = 0;
  }
  if (msRenderBufferID && *msRenderBufferID > 0) {
    gl->functions->deleteRenderbuffers(1, msRenderBufferID);
    *msRenderBufferID = 0;
  }
}

static bool CreateRenderBuffer(const GLInterface* gl, const GLTexture* texture,
                               GLFrameBuffer* renderTargetFBInfo, unsigned* msRenderBufferID,
                               int sampleCount) {
  gl->functions->genFramebuffers(1, &(renderTargetFBInfo->id));
  if (renderTargetFBInfo->id == 0) {
    return false;
  }
  gl->functions->genRenderbuffers(1, msRenderBufferID);
  if (*msRenderBufferID == 0) {
    return false;
  }
  gl->functions->bindRenderbuffer(GL_RENDERBUFFER, *msRenderBufferID);
  if (!RenderbufferStorageMSAA(gl, sampleCount, renderTargetFBInfo->format, texture->width(),
                               texture->height())) {
    return false;
  }
  gl->functions->bindFramebuffer(GL_FRAMEBUFFER, renderTargetFBInfo->id);
  gl->functions->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                         *msRenderBufferID);
#ifdef TGFX_BUILD_FOR_WEB
  return true;
#else
  return gl->functions->checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
#endif
}

std::shared_ptr<GLRenderTarget> GLRenderTarget::MakeFrom(Context* context, const GLTexture* texture,
                                                         int sampleCount) {
  if (texture == nullptr || context == nullptr) {
    return nullptr;
  }
  auto gl = GLInterface::Get(context);
  GLFrameBuffer textureFBInfo = {};
  textureFBInfo.format = texture->glSampler().format;
  gl->functions->genFramebuffers(1, &textureFBInfo.id);
  if (textureFBInfo.id == 0) {
    return nullptr;
  }
  GLFrameBuffer renderTargetFBInfo = {};
  renderTargetFBInfo.format = texture->glSampler().format;
  unsigned msRenderBufferID = 0;
  if (sampleCount > 1 && gl->caps->usesMSAARenderBuffers()) {
    if (!CreateRenderBuffer(gl, texture, &renderTargetFBInfo, &msRenderBufferID, sampleCount)) {
      ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
      return nullptr;
    }
  } else {
    renderTargetFBInfo = textureFBInfo;
  }
  gl->functions->bindFramebuffer(GL_FRAMEBUFFER, textureFBInfo.id);
  auto textureInfo = texture->glSampler();
  FrameBufferTexture2D(gl, textureInfo.target, textureInfo.id, sampleCount);
#ifndef TGFX_BUILD_FOR_WEB
  if (gl->functions->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
    return nullptr;
  }
#endif
  auto textureTarget = texture->glSampler().target;
  auto rt = new GLRenderTarget(texture->width(), texture->height(), texture->origin(), sampleCount,
                               textureFBInfo, textureTarget);
  rt->renderTargetFBInfo = renderTargetFBInfo;
  rt->msRenderBufferID = msRenderBufferID;
  return Resource::Wrap(context, rt);
}

GLRenderTarget::GLRenderTarget(int width, int height, ImageOrigin origin, int sampleCount,
                               GLFrameBuffer frameBuffer, unsigned textureTarget)
    : RenderTarget(width, height, origin, sampleCount),
      textureFBInfo(frameBuffer),
      textureTarget(textureTarget) {
}

void GLRenderTarget::clear(const GLInterface* gl) const {
  int oldFb = 0;
  gl->functions->getIntegerv(GL_FRAMEBUFFER_BINDING, &oldFb);
  gl->functions->bindFramebuffer(GL_FRAMEBUFFER, renderTargetFBInfo.id);
  gl->functions->viewport(0, 0, width(), height());
  gl->functions->scissor(0, 0, width(), height());
  gl->functions->clearColor(0.0f, 0.0f, 0.0f, 0.0f);
  gl->functions->clear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl->functions->bindFramebuffer(GL_FRAMEBUFFER, oldFb);
}

static bool CanReadDirectly(const GLInterface* gl, ImageOrigin origin, const ImageInfo& srcInfo,
                            const ImageInfo& dstInfo) {
  if (origin != ImageOrigin::TopLeft || dstInfo.alphaType() != srcInfo.alphaType() ||
      dstInfo.colorType() != srcInfo.colorType()) {
    return false;
  }
  if (dstInfo.rowBytes() != dstInfo.minRowBytes() && !gl->caps->packRowLengthSupport) {
    return false;
  }
  return true;
}

static void CopyPixels(const ImageInfo& srcInfo, const void* srcPixels, const ImageInfo& dstInfo,
                       void* dstPixels, bool flipY) {
  auto pixels = srcPixels;
  uint8_t* tempPixels = nullptr;
  if (flipY) {
    tempPixels = new uint8_t[srcInfo.byteSize()];
    auto rowCount = srcInfo.height();
    auto rowBytes = srcInfo.rowBytes();
    auto dst = tempPixels;
    for (int i = 0; i < rowCount; i++) {
      auto src = reinterpret_cast<const uint8_t*>(srcPixels) + (rowCount - i - 1) * rowBytes;
      memcpy(dst, src, rowBytes);
      dst += rowBytes;
    }
    pixels = tempPixels;
  }
  Bitmap bitmap(srcInfo, pixels);
  bitmap.readPixels(dstInfo, dstPixels);
  delete[] tempPixels;
}

bool GLRenderTarget::readPixels(Context* context, const ImageInfo& dstInfo, void* dstPixels,
                                int srcX, int srcY) const {
  dstPixels = dstInfo.computeOffset(dstPixels, -srcX, -srcY);
  auto outInfo = dstInfo.makeIntersect(-srcX, -srcY, width(), height());
  if (outInfo.isEmpty()) {
    return false;
  }
  auto pixelFormat = renderTargetFBInfo.format;
  auto gl = GLInterface::Get(context);
  const auto& format = gl->caps->getTextureFormat(pixelFormat);
  gl->functions->bindFramebuffer(GL_FRAMEBUFFER, renderTargetFBInfo.id);
  auto colorType = pixelFormat == PixelFormat::ALPHA_8 ? ColorType::ALPHA_8 : ColorType::RGBA_8888;
  auto srcInfo =
      ImageInfo::Make(outInfo.width(), outInfo.height(), colorType, AlphaType::Premultiplied);
  void* pixels = nullptr;
  uint8_t* tempPixels = nullptr;
  auto restoreGLRowLength = false;
  if (CanReadDirectly(gl, origin(), srcInfo, outInfo)) {
    pixels = dstPixels;
    if (outInfo.rowBytes() != outInfo.minRowBytes()) {
      gl->functions->pixelStorei(GL_PACK_ROW_LENGTH,
                                 static_cast<int>(outInfo.rowBytes() / outInfo.bytesPerPixel()));
      restoreGLRowLength = true;
    }
  } else {
    tempPixels = new uint8_t[srcInfo.byteSize()];
    pixels = tempPixels;
  }
  auto alignment = pixelFormat == PixelFormat::ALPHA_8 ? 1 : 4;
  gl->functions->pixelStorei(GL_PACK_ALIGNMENT, alignment);
  auto flipY = origin() == ImageOrigin::BottomLeft;
  auto readX = std::max(0, srcX);
  auto readY = std::max(0, srcY);
  if (flipY) {
    readY = height() - readY - outInfo.height();
  }
  gl->functions->readPixels(readX, readY, outInfo.width(), outInfo.height(), format.externalFormat,
                            GL_UNSIGNED_BYTE, pixels);
  if (restoreGLRowLength) {
    gl->functions->pixelStorei(GL_PACK_ROW_LENGTH, 0);
  }
  if (tempPixels != nullptr) {
    CopyPixels(srcInfo, tempPixels, outInfo, dstPixels, flipY);
    delete[] tempPixels;
  }
  return true;
}

void GLRenderTarget::resolve(Context* context) const {
  if (sampleCount() <= 1) {
    return;
  }
  auto gl = GLInterface::Get(context);
  if (!gl->caps->usesMSAARenderBuffers()) {
    return;
  }
  gl->functions->bindFramebuffer(GL_READ_FRAMEBUFFER, renderTargetFBInfo.id);
  gl->functions->bindFramebuffer(GL_DRAW_FRAMEBUFFER, textureFBInfo.id);
  if (gl->caps->msFBOType == MSFBOType::ES_Apple) {
    // Apple's extension uses the scissor as the blit bounds.
    gl->functions->enable(GL_SCISSOR_TEST);
    gl->functions->scissor(0, 0, width(), height());
    gl->functions->resolveMultisampleFramebuffer();
    gl->functions->disable(GL_SCISSOR_TEST);
  } else {
    // BlitFrameBuffer respects the scissor, so disable it.
    gl->functions->disable(GL_SCISSOR_TEST);
    gl->functions->blitFramebuffer(0, 0, width(), height(), 0, 0, width(), height(),
                                   GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
}

void GLRenderTarget::onRelease(Context* context) {
  if (externalTexture) {
    return;
  }
  if (textureTarget != 0) {
    auto gl = GLInterface::Get(context);
    gl->functions->bindFramebuffer(GL_FRAMEBUFFER, textureFBInfo.id);
    FrameBufferTexture2D(gl, textureTarget, 0, sampleCount());
    gl->functions->bindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
}
}  // namespace tgfx
