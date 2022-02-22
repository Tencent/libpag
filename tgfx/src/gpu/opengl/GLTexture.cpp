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

#include "gpu/opengl/GLTexture.h"
#include "GLUtil.h"
#include "core/utils/UniqueID.h"

namespace tgfx {
class GLBackendTexture : public GLTexture {
 public:
  GLBackendTexture(GLSampler textureSampler, int width, int height, ImageOrigin origin,
                   bool adopted)
      : GLTexture(width, height, origin), adopted(adopted) {
    sampler = std::move(textureSampler);
  }

  size_t memoryUsage() const override {
    return 0;
  }

 protected:
  void onRelease(Context* context) override {
    if (adopted) {
      auto gl = GLContext::Unwrap(context);
      gl->deleteTextures(1, &sampler.id);
    }
  }

 private:
  bool adopted = false;
};

std::shared_ptr<GLTexture> GLTexture::MakeFrom(Context* context, const GLSampler& sampler,
                                               int width, int height, ImageOrigin origin) {
  if (context == nullptr || width <= 0 || height <= 0 || sampler.id == 0) {
    return nullptr;
  }
  auto texture = new GLBackendTexture(sampler, width, height, origin, false);
  return Resource::Wrap(context, texture);
}

std::shared_ptr<GLTexture> GLTexture::MakeAdopted(Context* context, const GLSampler& sampler,
                                                  int width, int height, ImageOrigin origin) {
  if (context == nullptr || width <= 0 || height <= 0 || sampler.id == 0) {
    return nullptr;
  }
  auto texture = new GLBackendTexture(sampler, width, height, origin, true);
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

  GLAlphaTexture(GLSampler textureSampler, int width, int height, ImageOrigin origin)
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
    gl->deleteTextures(1, &sampler.id);
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

  GLRGBATexture(GLSampler textureSampler, int width, int height,
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
    gl->deleteTextures(1, &sampler.id);
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
  PixelFormat pixelFormat = alphaOnly ? PixelFormat::ALPHA_8 : PixelFormat::RGBA_8888;
  const auto& format = gl->caps->getTextureFormat(pixelFormat);
  GLStateGuard stateGuard(context);
  BytesKey recycleKey = {};
  if (alphaOnly) {
    GLAlphaTexture::ComputeRecycleKey(&recycleKey, width, height);
  } else {
    GLRGBATexture::ComputeRecycleKey(&recycleKey, width, height);
  }

  auto texture =
      std::static_pointer_cast<GLTexture>(context->resourceCache()->getRecycled(recycleKey));
  GLSampler sampler = {};
  if (texture) {
    texture->_origin = origin;
    sampler = texture->glSampler();
  } else {
    sampler.target = GL::TEXTURE_2D;
    sampler.format = pixelFormat;
    gl->genTextures(1, &(sampler.id));
    if (sampler.id == 0) {
      return nullptr;
    }
    gl->bindTexture(sampler.target, sampler.id);
    gl->texParameteri(sampler.target, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    gl->texParameteri(sampler.target, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);
    gl->texParameteri(sampler.target, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    gl->texParameteri(sampler.target, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    if (pixels == nullptr) {
      gl->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage), width,
                     height, 0, format.externalFormat, GL::UNSIGNED_BYTE, nullptr);
    }
    if (!CheckGLError(gl)) {
      gl->deleteTextures(1, &sampler.id);
      return nullptr;
    }
    if (alphaOnly) {
      texture = Resource::Wrap(context, new GLAlphaTexture(sampler, width, height, origin));
    } else {
      texture = Resource::Wrap(context, new GLRGBATexture(sampler, width, height, origin));
    }
  }
  if (pixels != nullptr) {
    int bytesPerPixel = alphaOnly ? 1 : 4;
    SubmitGLTexture(gl, sampler, width, height, rowBytes, bytesPerPixel, pixels);
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

std::shared_ptr<Texture> Texture::MakeRGBA(Context* context, int width, int height, void* pixels,
                                           size_t rowBytes, ImageOrigin origin) {
  if (context == nullptr) {
    return nullptr;
  }
  return Make(context, width, height, pixels, rowBytes, origin, false);
}

GLTexture::GLTexture(int width, int height, ImageOrigin origin) : Texture(width, height, origin) {
}

Point GLTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}
}  // namespace tgfx
