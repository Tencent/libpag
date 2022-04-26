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
#include <OpenGL/gl3.h>
#include "core/utils/UniqueID.h"
#include "tgfx/gpu/opengl/cgl/CGLDevice.h"

namespace tgfx {
std::shared_ptr<CGLHardwareTexture> CGLHardwareTexture::MakeFrom(Context* context,
                                                                 CVPixelBufferRef pixelBuffer) {
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, pixelBuffer);
  auto glTexture = std::static_pointer_cast<CGLHardwareTexture>(
      context->resourceCache()->getRecycled(recycleKey));
  if (glTexture) {
    return glTexture;
  }
  auto cglDevice = static_cast<CGLDevice*>(context->device());
  if (cglDevice == nullptr) {
    return nullptr;
  }
  CVOpenGLTextureCacheRef textureCache = cglDevice->getTextureCache();
  if (textureCache == nil) {
    return nullptr;
  }
  CVOpenGLTextureRef texture = nil;
  CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, pixelBuffer,
                                             nullptr, &texture);
  if (texture == nil) {
    return nullptr;
  }
  GLSampler glSampler = {};
  glSampler.target = CVOpenGLTextureGetTarget(texture);
  glSampler.id = CVOpenGLTextureGetName(texture);
  glSampler.format =
      CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_OneComponent8
          ? PixelFormat::ALPHA_8
          : PixelFormat::RGBA_8888;
  glTexture = Resource::Wrap(context, new CGLHardwareTexture(pixelBuffer));
  glTexture->sampler = glSampler;
  glTexture->texture = texture;
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
}

Point CGLHardwareTexture::getTextureCoord(float x, float y) const {
  if (sampler.target == GL_TEXTURE_RECTANGLE) {
    return {x, y};
  }
  return GLTexture::getTextureCoord(x, y);
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
  auto cglDevice = static_cast<CGLDevice*>(context->device());
  auto textureCache = cglDevice->getTextureCache();
  CVOpenGLTextureCacheFlush(textureCache, 0);
}
}
