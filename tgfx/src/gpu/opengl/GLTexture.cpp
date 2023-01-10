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

#include "tgfx/gpu/opengl/GLTexture.h"
#include "GLUtil.h"
#include "core/utils/UniqueID.h"
#include "gpu/Gpu.h"

namespace tgfx {
static size_t ComputeSize(int width, int height, int sizePerPixel, bool mipMapped) {
  auto colorSize = width * height * sizePerPixel;
  return mipMapped ? colorSize * 4 / 3 : colorSize;
}

class GLBackendTexture : public GLTexture {
 public:
  GLBackendTexture(GLSampler textureSampler, int width, int height, ImageOrigin origin,
                   bool adopted)
      : GLTexture(width, height, origin), adopted(adopted) {
    sampler = std::move(textureSampler);
  }

 private:
  bool adopted = false;

  size_t memoryUsage() const override {
    return 0;
  }

  void onReleaseGPU() override {
    if (adopted) {
      context->gpu()->deleteTexture(&sampler);
    }
  }
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
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height, bool mipMap) {
    static const uint32_t AlphaType = UniqueID::Next();
    recycleKey->write(AlphaType);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
    recycleKey->write(static_cast<uint32_t>(mipMap ? 1 : 0));
  }

  GLAlphaTexture(GLSampler textureSampler, int width, int height, ImageOrigin origin)
      : GLTexture(width, height, origin) {
    sampler = std::move(textureSampler);
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height(), sampler.maxMipMapLevel > 0);
  }

 private:
  size_t memoryUsage() const override {
    return ComputeSize(width(), height(), 1, sampler.maxMipMapLevel > 0);
  }

  void onReleaseGPU() override {
    context->gpu()->deleteTexture(&sampler);
  }
};

class GLRGBATexture : public GLTexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height, bool mipMap) {
    static const uint32_t RGBAType = UniqueID::Next();
    recycleKey->write(RGBAType);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
    recycleKey->write(static_cast<uint32_t>(mipMap ? 1 : 0));
  }

  GLRGBATexture(GLSampler textureSampler, int width, int height,
                ImageOrigin origin = ImageOrigin::TopLeft)
      : GLTexture(width, height, origin) {
    sampler = std::move(textureSampler);
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height(), sampler.maxMipMapLevel > 0);
  }

 private:
  size_t memoryUsage() const override {
    return ComputeSize(width(), height(), 4, sampler.maxMipMapLevel > 0);
  }

  void onReleaseGPU() override {
    context->gpu()->deleteTexture(&sampler);
  }
};

class GLBGRATexture : public GLRGBATexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height, bool mipMap) {
    static const uint32_t BGRAType = UniqueID::Next();
    recycleKey->write(BGRAType);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
    recycleKey->write(static_cast<uint32_t>(mipMap ? 1 : 0));
  }

  GLBGRATexture(GLSampler textureSampler, int width, int height,
                ImageOrigin origin = ImageOrigin::TopLeft)
      : GLRGBATexture(std::move(textureSampler), width, height, origin) {
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height(), sampler.maxMipMapLevel > 0);
  }
};

static bool CheckTextureSize(const GLCaps* caps, int width, int height) {
  if (width < 1 || height < 1) {
    return false;
  }
  auto maxTextureSize = caps->maxTextureSize;
  return width <= maxTextureSize && height <= maxTextureSize;
}

std::shared_ptr<Texture> Texture::Make(Context* context, int width, int height, void* pixels,
                                       size_t rowBytes, ImageOrigin origin, PixelFormat pixelFormat,
                                       bool mipMapped) {
  // Clear the previously generated GLError, causing the subsequent CheckGLError to return an
  // incorrect result.
  CheckGLError(context);
  auto caps = GLCaps::Get(context);
  if (!CheckTextureSize(caps, width, height)) {
    return nullptr;
  }
  bool enableMipMap = mipMapped && caps->mipMapSupport;
  BytesKey recycleKey = {};
  if (pixelFormat == PixelFormat::ALPHA_8) {
    GLAlphaTexture::ComputeRecycleKey(&recycleKey, width, height, enableMipMap);
  } else if (pixelFormat == PixelFormat::BGRA_8888) {
    GLBGRATexture::ComputeRecycleKey(&recycleKey, width, height, enableMipMap);
  } else {
    GLRGBATexture::ComputeRecycleKey(&recycleKey, width, height, enableMipMap);
  }

  auto texture =
      std::static_pointer_cast<GLTexture>(context->resourceCache()->getRecycled(recycleKey));
  if (texture) {
    texture->_origin = origin;
  } else {
    int maxMipmapLevel = 0;
    if (enableMipMap) {
      maxMipmapLevel = static_cast<int>(std::floor(std::log2(std::max(width, height))));
    }
    auto sampler = context->gpu()->createTexture(width, height, pixelFormat, maxMipmapLevel + 1);
    if (sampler == nullptr) {
      return nullptr;
    }
    auto glSampler = static_cast<GLSampler*>(sampler.get());
    if (pixelFormat == PixelFormat::ALPHA_8) {
      texture = Resource::Wrap(context, new GLAlphaTexture(*glSampler, width, height, origin));
    } else if (pixelFormat == PixelFormat::BGRA_8888) {
      texture = Resource::Wrap(context, new GLBGRATexture(*glSampler, width, height, origin));
    } else {
      texture = Resource::Wrap(context, new GLRGBATexture(*glSampler, width, height, origin));
    }
  }
  if (pixels != nullptr) {
    context->gpu()->writePixels(texture->getSampler(),
                                Rect::MakeWH(static_cast<float>(width), static_cast<float>(height)),
                                pixels, rowBytes, pixelFormat);
    context->gpu()->regenerateMipMapLevels(texture->getSampler());
  }
  return texture;
}

std::shared_ptr<Texture> Texture::MakeAlpha(Context* context, int width, int height, void* pixels,
                                            size_t rowBytes, ImageOrigin origin, bool mipMapped) {
  if (context == nullptr) {
    return nullptr;
  }
  return Make(context, width, height, pixels, rowBytes, origin, PixelFormat::ALPHA_8, mipMapped);
}

std::shared_ptr<Texture> Texture::MakeRGBA(Context* context, int width, int height, void* pixels,
                                           size_t rowBytes, ImageOrigin origin, bool mipMapped) {
  if (context == nullptr) {
    return nullptr;
  }
  return Make(context, width, height, pixels, rowBytes, origin, PixelFormat::RGBA_8888, mipMapped);
}

GLTexture::GLTexture(int width, int height, ImageOrigin origin) : Texture(width, height, origin) {
}

Point GLTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}

/**
 * Overrides this method if the texture is backed by HardwareBuffer.
 */
bool GLTexture::readPixels(const ImageInfo&, void*, int, int) const {
  return false;
}

}  // namespace tgfx
