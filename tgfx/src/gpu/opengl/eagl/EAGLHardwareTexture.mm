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

#include "EAGLHardwareTexture.h"
#include "EAGLDevice.h"

namespace pag {
static CVOpenGLESTextureRef GetTextureRef(Context* context, CVPixelBufferRef pixelBuffer,
                                          CVOpenGLESTextureCacheRef textureCache,
                                          unsigned* sizedFormat) {
  auto width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
  auto height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
  auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  CVOpenGLESTextureRef texture = nil;
  CVReturn result;
  if (pixelFormat == kCVPixelFormatType_OneComponent8) {
    auto gl = GLContext::Unwrap(context);
    const auto& format = gl->caps->getTextureFormat(PixelConfig::ALPHA_8);
    *sizedFormat = format.sizedFormat;
    // 返回的 texture 对象是一个强引用计数为 1 的对象。
    result = CVOpenGLESTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault, textureCache, pixelBuffer, NULL, /* texture attributes */
        GL::TEXTURE_2D, format.internalFormatTexImage,         /* opengl format */
        width, height, format.externalFormat,                 /* native iOS format */
        GL::UNSIGNED_BYTE, 0, &texture);
  } else {
    *sizedFormat = GL::RGBA8;
    // 返回的 texture 对象是一个强引用计数为 1 的对象。
    result = CVOpenGLESTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault, textureCache, pixelBuffer, NULL, /* texture attributes */
        GL::TEXTURE_2D, GL::RGBA,                               /* opengl format */
        width, height, GL::BGRA,                               /* native iOS format */
        GL::UNSIGNED_BYTE, 0, &texture);
  }
  if (result != kCVReturnSuccess && texture != nil) {
    CFRelease(texture);
    texture = nil;
  }
  return texture;
}

std::shared_ptr<EAGLHardwareTexture> EAGLHardwareTexture::MakeFrom(Context* context,
                                                                   CVPixelBufferRef pixelBuffer,
                                                                   bool adopted) {
  std::shared_ptr<EAGLHardwareTexture> glTexture = nullptr;
  if (!adopted) {
    BytesKey recycleKey = {};
    ComputeRecycleKey(&recycleKey, pixelBuffer);
    glTexture =
        std::static_pointer_cast<EAGLHardwareTexture>(context->getRecycledResource(recycleKey));
    if (glTexture) {
      return glTexture;
    }
  }
  auto eaglDevice = static_cast<EAGLDevice*>(context->getDevice());
  if (eaglDevice == nullptr) {
    return nullptr;
  }
  CVOpenGLESTextureCacheRef textureCache = nil;
  if (adopted) {
    // use independent texture cache here, to prevent memory leaking when binding the same
    // CVPixelBuffer to two different texture caches.
    CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, eaglDevice->eaglContext(), NULL,
                                 &textureCache);
  } else {
    textureCache = eaglDevice->getTextureCache();
  }
  if (textureCache == nil) {
    return nullptr;
  }
  unsigned sizedFormat = 0;
  auto texture = GetTextureRef(context, pixelBuffer, textureCache, &sizedFormat);
  if (texture == nil) {
    if (adopted) {
      CFRelease(textureCache);
    }
    return nullptr;
  }
  GLTextureInfo glInfo = {};
  glInfo.target = CVOpenGLESTextureGetTarget(texture);
  glInfo.id = CVOpenGLESTextureGetName(texture);
  glInfo.format = sizedFormat;
  auto oneComponent8 =
      CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_OneComponent8;
  glTexture = Resource::Wrap(context, new EAGLHardwareTexture(pixelBuffer));
  glTexture->sampler.glInfo = glInfo;
  glTexture->sampler.config = oneComponent8 ? PixelConfig::ALPHA_8 : PixelConfig::RGBA_8888;
  glTexture->texture = texture;
  glTexture->textureCache = adopted ? textureCache : nil;
  return glTexture;
}

void EAGLHardwareTexture::ComputeRecycleKey(BytesKey* recycleKey, CVPixelBufferRef pixelBuffer) {
  static const uint32_t HardwareType = UniqueID::Next();
  recycleKey->write(HardwareType);
  // 这里可以直接用指针做为 key 是因为缓存的 holder 会持有 CVPixelBuffer，只要 holder
  // 缓存存在，对应的 CVPixelBuffer
  // 指针就是有效的，不会出现指针地址被其他新创建对象占用的情况。其他情况下应该避免使用指针做 key。
  recycleKey->write(pixelBuffer);
}

EAGLHardwareTexture::EAGLHardwareTexture(CVPixelBufferRef pixelBuffer)
    : GLTexture(static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)),
                static_cast<int>(CVPixelBufferGetHeight(pixelBuffer)), ImageOrigin::TopLeft),
      pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
}

EAGLHardwareTexture::~EAGLHardwareTexture() {
  CFRelease(pixelBuffer);
  if (texture) {
    CFRelease(texture);
  }
  if (textureCache) {
    CFRelease(textureCache);
  }
}

size_t EAGLHardwareTexture::memoryUsage() const {
  // 显存来自 CVPixelBuffer，这里不做重复统计。
  return 0;
}

void EAGLHardwareTexture::computeRecycleKey(BytesKey* recycleKey) const {
  if (textureCache == nil) {
    // not adopted
    ComputeRecycleKey(recycleKey, pixelBuffer);
  }
}

void EAGLHardwareTexture::onRelease(Context* context) {
  if (texture == nil) {
    return;
  }
  auto cache = textureCache;
  if (!cache) {
    auto eaglDevice = static_cast<EAGLDevice*>(context->getDevice());
    cache = eaglDevice->getTextureCache();
  }
  CFRelease(texture);
  texture = nil;
  CVOpenGLESTextureCacheFlush(cache, 0);
  if (textureCache != nil) {
    CFRelease(textureCache);
    textureCache = nil;
  }
}
}
