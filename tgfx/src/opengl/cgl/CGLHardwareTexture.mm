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
#include "opengl/GLSampler.h"
#include "utils/UniqueID.h"

namespace tgfx {
std::shared_ptr<CGLHardwareTexture> CGLHardwareTexture::MakeFrom(
    Context* context, CVPixelBufferRef pixelBuffer, CVOpenGLTextureCacheRef textureCache) {
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, pixelBuffer);
  auto glTexture = std::static_pointer_cast<CGLHardwareTexture>(
      context->resourceCache()->getRecycled(recycleKey));
  if (glTexture) {
    return glTexture;
  }
  if (textureCache == nil) {
    return nullptr;
  }
  CVOpenGLTextureRef texture = nil;
  CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, pixelBuffer,
                                             nullptr, &texture);
  if (texture == nil) {
    return nullptr;
  }
  auto glSampler = std::make_unique<GLSampler>();
  glSampler->target = CVOpenGLTextureGetTarget(texture);
  glSampler->id = CVOpenGLTextureGetName(texture);
  glSampler->format =
      CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_OneComponent8
          ? PixelFormat::ALPHA_8
          : PixelFormat::RGBA_8888;
  glTexture = Resource::Wrap(context, new CGLHardwareTexture(pixelBuffer));
  glTexture->sampler = std::move(glSampler);
  glTexture->texture = texture;
  glTexture->textureCache = textureCache;
  CFRetain(textureCache);

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
    : Texture(static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)),
              static_cast<int>(CVPixelBufferGetHeight(pixelBuffer)), ImageOrigin::TopLeft),
      pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
}

CGLHardwareTexture::~CGLHardwareTexture() {
  CFRelease(pixelBuffer);
  if (texture != nil) {
    CFRelease(texture);
    CFRelease(textureCache);
  }
}

size_t CGLHardwareTexture::memoryUsage() const {
  return CVPixelBufferGetDataSize(pixelBuffer);
}

void CGLHardwareTexture::computeRecycleKey(BytesKey* recycleKey) const {
  ComputeRecycleKey(recycleKey, pixelBuffer);
}

void CGLHardwareTexture::onReleaseGPU() {
  if (texture == nil) {
    return;
  }
  CFRelease(texture);
  texture = nil;
  CVOpenGLTextureCacheFlush(textureCache, 0);
  CFRelease(textureCache);
  textureCache = nil;
}
}  // namespace tgfx
