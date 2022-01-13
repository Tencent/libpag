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

#include "CGLHardwareTexture.h"
#include "gpu/opengl/cgl/CGLDevice.h"

namespace pag {
std::shared_ptr<CGLHardwareTexture> CGLHardwareTexture::MakeFrom(Context* context,
                                                                 CVPixelBufferRef pixelBuffer,
                                                                 bool adopted) {
  std::shared_ptr<CGLHardwareTexture> glTexture = nullptr;
  if (!adopted) {
    BytesKey recycleKey = {};
    ComputeRecycleKey(&recycleKey, pixelBuffer);
    auto glTexture =
        std::static_pointer_cast<CGLHardwareTexture>(context->getRecycledResource(recycleKey));
    if (glTexture) {
      return glTexture;
    }
  }
  auto cglDevice = static_cast<CGLDevice*>(context->getDevice());
  if (cglDevice == nullptr) {
    return nullptr;
  }
  CVOpenGLTextureCacheRef textureCache = nil;
  if (adopted) {
    // use independent texture cache here, to prevent memory leaking when binding the same
    // CVPixelBuffer to two different texture caches.
    CGLPixelFormatObj cglPixelFormat = CGLGetPixelFormat(cglDevice->cglContext());
    CVOpenGLTextureCacheCreate(kCFAllocatorDefault, NULL, cglDevice->cglContext(), cglPixelFormat,
                               NULL, &textureCache);
  } else {
    textureCache = cglDevice->getTextureCache();
  }
  if (textureCache == nil) {
    return nullptr;
  }
  CVOpenGLTextureRef texture = nil;
  CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, pixelBuffer,
                                             nullptr, &texture);
  if (texture == nil) {
    if (adopted) {
      CFRelease(textureCache);
    }
    return nullptr;
  }
  GLTextureInfo glInfo = {};
  glInfo.target = CVOpenGLTextureGetTarget(texture);
  glInfo.id = CVOpenGLTextureGetName(texture);
  auto oneComponent8 =
      CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_OneComponent8;
  // mac 端创建的单通道纹理都是 GL::RED。
  glInfo.format = oneComponent8 ? GL::R8 : GL::RGBA8;
  glTexture = Resource::Wrap(context, new CGLHardwareTexture(pixelBuffer));
  glTexture->sampler.glInfo = glInfo;
  glTexture->sampler.config = oneComponent8 ? PixelConfig::ALPHA_8 : PixelConfig::RGBA_8888;
  glTexture->texture = texture;
  glTexture->textureCache = adopted ? textureCache : nil;
  return glTexture;
}

void CGLHardwareTexture::ComputeRecycleKey(BytesKey* recycleKey, CVPixelBufferRef pixelBuffer) {
  static const uint32_t BGRAType = UniqueID::Next();
  recycleKey->write(BGRAType);
  // 这里可以直接用指针做为 key 是因为缓存的 holder 会持有 CVPixelBuffer，只要 holder
  // 缓存存在，对应的 CVPixelBuffer
  // 指针就是有效的，不会出现指针地址被其他新创建对象占用的情况。其他情况下应该避免使用指针做 key。
  recycleKey->write(pixelBuffer);
}

CGLHardwareTexture::CGLHardwareTexture(CVPixelBufferRef pixelBuffer)
    : GLTexture(static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)),
                static_cast<int>(CVPixelBufferGetHeight(pixelBuffer)), ImageOrigin::TopLeft),
      pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
}

CGLHardwareTexture::~CGLHardwareTexture() {
  CFRelease(pixelBuffer);
  if (texture) {
    CFRelease(texture);
  }
  if (textureCache) {
    CFRelease(textureCache);
  }
}

Point CGLHardwareTexture::getTextureCoord(float x, float y) const {
  if (sampler.glInfo.target == GL::TEXTURE_RECTANGLE) {
    return {x, y};
  }
  return GLTexture::getTextureCoord(x, y);
}

size_t CGLHardwareTexture::memoryUsage() const {
  // 显存来自 CVPixelBuffer，这里不做重复统计。
  return 0;
}

void CGLHardwareTexture::computeRecycleKey(BytesKey* recycleKey) const {
  if (textureCache == nil) {
    // not adopted
    ComputeRecycleKey(recycleKey, pixelBuffer);
  }
}

void CGLHardwareTexture::onRelease(Context* context) {
  if (texture == nil) {
    return;
  }
  auto cache = textureCache;
  if (!cache) {
    auto cglDevice = static_cast<CGLDevice*>(context->getDevice());
    cache = cglDevice->getTextureCache();
  }
  CFRelease(texture);
  texture = nil;
  CVOpenGLTextureCacheFlush(cache, 0);
  if (textureCache != nil) {
    CFRelease(textureCache);
    textureCache = nil;
  }
}
}
