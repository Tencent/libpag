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

#include "GLTexture.h"
#include "GLUtil.h"
#include "base/utils/UniqueID.h"
#include "core/Bitmap.h"
#include "gpu/Surface.h"

namespace pag {
class GLBackendTexture : public GLTexture {
 public:
  GLBackendTexture(GLTextureSampler textureSampler, int width, int height, ImageOrigin origin)
      : GLTexture(width, height, origin) {
    sampler = std::move(textureSampler);
  }

  size_t memoryUsage() const override {
    return 0;
  }

 protected:
  void onRelease(Context*) override {
  }
};

std::shared_ptr<Texture> Texture::MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           ImageOrigin origin) {
  GLTextureInfo glTextureInfo = {};
  if (context == nullptr || !backendTexture.getGLTextureInfo(&glTextureInfo)) {
    return nullptr;
  }
  auto sampler = GLTextureSampler(PixelConfig::RGBA_8888, glTextureInfo);
  auto texture =
      new GLBackendTexture(sampler, backendTexture.width(), backendTexture.height(), origin);
  return Resource::Wrap(context, texture);
}

class GLAlphaTexture : public GLTexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height) {
    static const uint32_t AlphaType = UniqueID::Next();
    recycleKey->write(AlphaType);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
  }

  GLAlphaTexture(GLTextureSampler textureSampler, int width, int height, ImageOrigin origin)
      : GLTexture(width, height, origin) {
    sampler = std::move(textureSampler);
  }

  size_t memoryUsage() const override {
    return static_cast<size_t>(width() * height());
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height());
  }

  void onRelease(Context* context) override {
    auto gl = GLContext::Unwrap(context);
    gl->deleteTextures(1, &sampler.glInfo.id);
  }
};

class GLRGBATexture : public GLTexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height) {
    static const uint32_t RGBAType = UniqueID::Next();
    recycleKey->write(RGBAType);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
  }

  GLRGBATexture(GLTextureSampler textureSampler, int width, int height,
                ImageOrigin origin = ImageOrigin::TopLeft)
      : GLTexture(width, height, origin) {
    sampler = std::move(textureSampler);
  }

  size_t memoryUsage() const override {
    return width() * height() * 4;
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height());
  }

  void onRelease(Context* context) override {
    auto gl = GLContext::Unwrap(context);
    gl->deleteTextures(1, &sampler.glInfo.id);
  }
};

static bool CheckMaxTextureSize(const GLInterface* gl, int width, int height) {
  auto maxTextureSize = gl->caps->maxTextureSize;
  return width <= maxTextureSize && height <= maxTextureSize;
}

std::shared_ptr<Texture> Texture::Make(Context* context, int width, int height, void* pixels,
                                       size_t rowBytes, ImageOrigin origin, bool alphaOnly) {
  auto gl = GLContext::Unwrap(context);
  // Clear the previously generated GLError, causing the subsequent CheckGLError to return an
  // incorrect result.
  CheckGLError(gl);

  if (!CheckMaxTextureSize(gl, width, height)) {
    return nullptr;
  }
  PixelConfig pixelConfig = alphaOnly ? PixelConfig::ALPHA_8 : PixelConfig::RGBA_8888;
  const auto& format = gl->caps->getTextureFormat(pixelConfig);
  GLStateGuard stateGuard(context);
  BytesKey recycleKey = {};
  if (alphaOnly) {
    GLAlphaTexture::ComputeRecycleKey(&recycleKey, width, height);
  } else {
    GLRGBATexture::ComputeRecycleKey(&recycleKey, width, height);
  }

  auto texture =
      std::static_pointer_cast<GLTexture>(context->resourceCache()->getRecycled(recycleKey));
  GLTextureInfo glInfo = {};
  if (texture) {
    texture->_origin = origin;
    glInfo = texture->getGLInfo();
  } else {
    glInfo.target = GL::TEXTURE_2D;
    glInfo.format = format.sizedFormat;
    gl->genTextures(1, &(glInfo.id));
    if (glInfo.id == 0) {
      return nullptr;
    }
    gl->bindTexture(glInfo.target, glInfo.id);
    gl->texParameteri(glInfo.target, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    gl->texParameteri(glInfo.target, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);
    gl->texParameteri(glInfo.target, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    gl->texParameteri(glInfo.target, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    if (pixels == nullptr) {
      gl->texImage2D(glInfo.target, 0, static_cast<int>(format.internalFormatTexImage), width,
                     height, 0, format.externalFormat, GL::UNSIGNED_BYTE, nullptr);
    }
    if (!CheckGLError(gl)) {
      gl->deleteTextures(1, &glInfo.id);
      return nullptr;
    }
    auto sampler = GLTextureSampler(pixelConfig, glInfo);
    if (alphaOnly) {
      texture = Resource::Wrap(context, new GLAlphaTexture(sampler, width, height, origin));
    } else {
      texture = Resource::Wrap(context, new GLRGBATexture(sampler, width, height, origin));
    }
  }
  if (pixels != nullptr) {
    int bytesPerPixel = alphaOnly ? 1 : 4;
    SubmitTexture(gl, glInfo, format, width, height, rowBytes, bytesPerPixel, pixels);
  }
  return texture;
}

std::shared_ptr<Texture> Texture::MakeAlpha(Context* context, int width, int height, void* pixels,
                                            size_t rowBytes, ImageOrigin origin) {
  if (context == nullptr) {
    return nullptr;
  }
  return Make(context, width, height, pixels, rowBytes, origin, true);
}

std::shared_ptr<GLTexture> GLTexture::MakeAlpha(Context* context, int width, int height,
                                                ImageOrigin origin) {
  auto texture = Texture::MakeAlpha(context, width, height, nullptr, 0, origin);
  return std::static_pointer_cast<GLTexture>(texture);
}

std::shared_ptr<Texture> Texture::MakeRGBA(Context* context, int width, int height, void* pixels,
                                           size_t rowBytes, ImageOrigin origin) {
  if (context == nullptr) {
    return nullptr;
  }
  return Make(context, width, height, pixels, rowBytes, origin, false);
}

std::shared_ptr<GLTexture> GLTexture::MakeRGBA(Context* context, int width, int height,
                                               ImageOrigin origin) {
  auto texture = Texture::MakeRGBA(context, width, height, nullptr, 0, origin);
  return std::static_pointer_cast<GLTexture>(texture);
}

GLTextureInfo GLTexture::Unwrap(const Texture* texture) {
  if (texture == nullptr || texture->isYUV()) {
    return {};
  }
  return static_cast<const GLTexture*>(texture)->sampler.glInfo;
}

GLTexture::GLTexture(int width, int height, ImageOrigin origin) : Texture(width, height, origin) {
}

Point GLTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}

void Trace(const Texture* texture, const std::string& path) {
  if (texture == nullptr) {
    return;
  }
  auto surface = Surface::Make(texture->context, texture->width(), texture->height());
  if (surface == nullptr) {
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->drawTexture(texture);
  auto pixelBuffer = PixelBuffer::Make(texture->width(), texture->height());
  Bitmap bitmap(pixelBuffer);
  if (bitmap.isEmpty()) {
    return;
  }
  surface->readPixels(bitmap.info(), bitmap.writablePixels());
  Trace(bitmap, path);
}
}  // namespace pag
