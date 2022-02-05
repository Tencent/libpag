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
#include "GLContext.h"
#include "GLState.h"
#include "GLUtil.h"
#include "image/Bitmap.h"

namespace pag {
std::shared_ptr<GLRenderTarget> GLRenderTarget::MakeFrom(Context* context,
                                                         const BackendRenderTarget& renderTarget,
                                                         ImageOrigin origin) {
  GLFrameBufferInfo glInfo = {};
  if (context == nullptr || !renderTarget.getGLFramebufferInfo(&glInfo)) {
    return nullptr;
  }
  auto target = new GLRenderTarget(renderTarget.width(), renderTarget.height(), origin, glInfo);
  target->renderTargetFBInfo = glInfo;
  return Resource::Wrap(context, target);
}

static bool RenderbufferStorageMSAA(const GLInterface* gl, int sampleCount, unsigned format,
                                    int width, int height) {
  CheckGLError(gl);
  switch (gl->caps->msFBOType) {
    case MSFBOType::Standard:
      gl->renderbufferStorageMultisample(GL::RENDERBUFFER, sampleCount, format, width, height);
      break;
    case MSFBOType::ES_Apple:
      gl->renderbufferStorageMultisampleAPPLE(GL::RENDERBUFFER, sampleCount, format, width, height);
      break;
    case MSFBOType::ES_EXT_MsToTexture:
    case MSFBOType::ES_IMG_MsToTexture:
      gl->renderbufferStorageMultisampleEXT(GL::RENDERBUFFER, sampleCount, format, width, height);
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
    gl->framebufferTexture2DMultisample(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, textureTarget,
                                        textureID, 0, sampleCount);
  } else {
    gl->framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, textureTarget, textureID, 0);
  }
}

static void ReleaseResource(Context* context, GLFrameBufferInfo* textureFBInfo,
                            GLFrameBufferInfo* renderTargetFBInfo = nullptr,
                            unsigned* msRenderBufferID = nullptr) {
  auto gl = GLContext::Unwrap(context);
  if (textureFBInfo && textureFBInfo->id) {
    gl->deleteFramebuffers(1, &(textureFBInfo->id));
    if (renderTargetFBInfo && renderTargetFBInfo->id == textureFBInfo->id) {
      renderTargetFBInfo->id = 0;
    }
    textureFBInfo->id = 0;
  }
  if (renderTargetFBInfo && renderTargetFBInfo->id > 0) {
    {
      GLStateGuard stateGuard(context);
      gl->bindFramebuffer(GL::FRAMEBUFFER, renderTargetFBInfo->id);
      gl->framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, 0);
    }
    gl->deleteFramebuffers(1, &(renderTargetFBInfo->id));
    renderTargetFBInfo->id = 0;
  }
  if (msRenderBufferID && *msRenderBufferID > 0) {
    gl->deleteRenderbuffers(1, msRenderBufferID);
    *msRenderBufferID = 0;
  }
}

static bool CreateRenderBuffer(const GLInterface* gl, GLTexture* texture,
                               GLFrameBufferInfo* renderTargetFBInfo, unsigned* msRenderBufferID,
                               int sampleCount) {
  gl->genFramebuffers(1, &(renderTargetFBInfo->id));
  if (renderTargetFBInfo->id == 0) {
    return false;
  }
  gl->genRenderbuffers(1, msRenderBufferID);
  if (*msRenderBufferID == 0) {
    return false;
  }
  gl->bindRenderbuffer(GL::RENDERBUFFER, *msRenderBufferID);
  if (!RenderbufferStorageMSAA(gl, sampleCount, renderTargetFBInfo->format, texture->width(),
                               texture->height())) {
    return false;
  }
  gl->bindFramebuffer(GL::FRAMEBUFFER, renderTargetFBInfo->id);
  gl->framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER,
                              *msRenderBufferID);
  return gl->checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
}

std::shared_ptr<GLRenderTarget> GLRenderTarget::MakeFrom(Context* context, GLTexture* texture,
                                                         int sampleCount) {
  if (texture == nullptr || context == nullptr) {
    return nullptr;
  }
  auto gl = GLContext::Unwrap(context);
  GLFrameBufferInfo textureFBInfo = {};
  textureFBInfo.format = texture->getGLInfo().format;
  gl->genFramebuffers(1, &textureFBInfo.id);
  if (textureFBInfo.id == 0) {
    return nullptr;
  }
  GLStateGuard stateGuard(context);
  GLFrameBufferInfo renderTargetFBInfo = {};
  renderTargetFBInfo.format = texture->getGLInfo().format;
  unsigned msRenderBufferID = 0;
  if (sampleCount > 1 && gl->caps->usesMSAARenderBuffers()) {
    if (!CreateRenderBuffer(gl, texture, &renderTargetFBInfo, &msRenderBufferID, sampleCount)) {
      ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
      return nullptr;
    }
  } else {
    renderTargetFBInfo = textureFBInfo;
  }
  gl->bindFramebuffer(GL::FRAMEBUFFER, textureFBInfo.id);
  auto textureInfo = texture->getGLInfo();
  FrameBufferTexture2D(gl, textureInfo.target, textureInfo.id, sampleCount);
  std::shared_ptr<GLRenderTarget> renderTarget = nullptr;

  if (gl->checkFramebufferStatus(GL::FRAMEBUFFER) != GL::FRAMEBUFFER_COMPLETE) {
    ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
  } else {
    auto textureTarget = texture->getGLInfo().target;
    auto rt = new GLRenderTarget(texture->width(), texture->height(), texture->origin(),
                                 textureFBInfo, textureTarget);
    rt->_sampleCount = sampleCount;
    rt->renderTargetFBInfo = renderTargetFBInfo;
    rt->msRenderBufferID = msRenderBufferID;
    renderTarget = Resource::Wrap(context, rt);
  }
  return renderTarget;
}

GLRenderTarget::GLRenderTarget(int width, int height, ImageOrigin origin, GLFrameBufferInfo info,
                               unsigned int textureTarget)
    : _width(width),
      _height(height),
      _origin(origin),
      textureFBInfo(info),
      textureTarget(textureTarget) {
}

void GLRenderTarget::clear(const GLInterface* gl) const {
  int oldFb = 0;
  gl->getIntegerv(GL::FRAMEBUFFER_BINDING, &oldFb);
  gl->bindFramebuffer(GL::FRAMEBUFFER, renderTargetFBInfo.id);
  gl->viewport(0, 0, _width, _height);
  gl->scissor(0, 0, _width, _height);
  gl->clearColor(0.0f, 0.0f, 0.0f, 0.0f);
  gl->clear(GL::COLOR_BUFFER_BIT | GL::STENCIL_BUFFER_BIT | GL::DEPTH_BUFFER_BIT);
  gl->bindFramebuffer(GL::FRAMEBUFFER, oldFb);
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
  auto outInfo = dstInfo.makeIntersect(-srcX, -srcY, _width, _height);
  if (outInfo.isEmpty()) {
    return false;
  }
  auto alphaOnly = renderTargetFBInfo.format == GL::R8;
  auto pixelConfig = alphaOnly ? PixelConfig::ALPHA_8 : PixelConfig::RGBA_8888;
  GLStateGuard stateGuard(context);
  auto gl = GLContext::Unwrap(context);
  const auto& format = gl->caps->getTextureFormat(pixelConfig);
  gl->bindFramebuffer(GL::FRAMEBUFFER, renderTargetFBInfo.id);
  auto colorType = alphaOnly ? ColorType::ALPHA_8 : ColorType::RGBA_8888;
  auto srcInfo =
      ImageInfo::Make(outInfo.width(), outInfo.height(), colorType, AlphaType::Premultiplied);
  void* pixels = nullptr;
  uint8_t* tempPixels = nullptr;
  if (CanReadDirectly(gl, _origin, srcInfo, outInfo)) {
    pixels = dstPixels;
    if (outInfo.rowBytes() != outInfo.minRowBytes()) {
      gl->pixelStorei(GL::PACK_ROW_LENGTH,
                      static_cast<int>(outInfo.rowBytes() / outInfo.bytesPerPixel()));
    }
  } else {
    tempPixels = new uint8_t[srcInfo.byteSize()];
    pixels = tempPixels;
  }
  auto alignment = alphaOnly ? 1 : 4;
  gl->pixelStorei(GL::PACK_ALIGNMENT, alignment);
  auto flipY = _origin == ImageOrigin::BottomLeft;
  auto readX = std::max(0, srcX);
  auto readY = std::max(0, srcY);
  if (flipY) {
    readY = _height - readY - outInfo.height();
  }
  gl->readPixels(readX, readY, outInfo.width(), outInfo.height(), format.externalFormat,
                 GL::UNSIGNED_BYTE, pixels);
  if (tempPixels != nullptr) {
    CopyPixels(srcInfo, tempPixels, outInfo, dstPixels, flipY);
    delete[] tempPixels;
  }
  return true;
}

void GLRenderTarget::resolve(Context* context) const {
  if (_sampleCount <= 1) {
    return;
  }
  auto gl = GLContext::Unwrap(context);
  if (!gl->caps->usesMSAARenderBuffers()) {
    return;
  }
  GLStateGuard stateGuard(context);
  gl->bindFramebuffer(GL::READ_FRAMEBUFFER, renderTargetFBInfo.id);
  gl->bindFramebuffer(GL::DRAW_FRAMEBUFFER, textureFBInfo.id);
  if (gl->caps->msFBOType == MSFBOType::ES_Apple) {
    // Apple's extension uses the scissor as the blit bounds.
    gl->enable(GL::SCISSOR_TEST);
    gl->scissor(0, 0, _width, _height);
    gl->resolveMultisampleFramebuffer();
    gl->disable(GL::SCISSOR_TEST);
  } else {
    // BlitFrameBuffer respects the scissor, so disable it.
    gl->disable(GL::SCISSOR_TEST);
    gl->blitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL::COLOR_BUFFER_BIT,
                        GL::NEAREST);
  }
}

void GLRenderTarget::onRelease(Context* context) {
  if (textureTarget == 0) {
    return;
  }
  {
    // The currently bound fboID may be the same as textureFBInfo.id, we must restore and then
    // delete, otherwise GL::INVALID_OPERATION(1282) will be reported。
    GLStateGuard stateGuard(context);
    auto gl = GLContext::Unwrap(context);
    gl->bindFramebuffer(GL::FRAMEBUFFER, textureFBInfo.id);
    FrameBufferTexture2D(gl, textureTarget, 0, _sampleCount);
  }
  ReleaseResource(context, &textureFBInfo, &renderTargetFBInfo, &msRenderBufferID);
}
}  // namespace pag
