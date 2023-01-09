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
#include "core/utils/UniqueID.h"
#include "gpu/opengl/GLContext.h"
#include "platform/apple/CVPixelBufferUtil.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/gpu/opengl/eagl/EAGLDevice.h"

namespace tgfx {
static CVOpenGLESTextureRef GetTextureRef(Context* context, CVPixelBufferRef pixelBuffer,
                                          PixelFormat pixelFormat,
                                          CVOpenGLESTextureCacheRef textureCache) {
  if (textureCache == nil) {
    return nil;
  }
  auto width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
  auto height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
  CVOpenGLESTextureRef texture = nil;
  auto caps = GLCaps::Get(context);
  const auto& format = caps->getTextureFormat(pixelFormat);
  // 返回的 texture 对象是一个强引用计数为 1 的对象。
  CVReturn result = CVOpenGLESTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, textureCache, pixelBuffer, NULL, /* texture attributes */
      GL_TEXTURE_2D, format.internalFormatTexImage,         /* opengl format */
      width, height, format.externalFormat,                 /* native iOS format */
      GL_UNSIGNED_BYTE, 0, &texture);
  if (result != kCVReturnSuccess && texture != nil) {
    CFRelease(texture);
    texture = nil;
  }
  return texture;
}

std::shared_ptr<EAGLHardwareTexture> EAGLHardwareTexture::MakeFrom(Context* context,
                                                                   CVPixelBufferRef pixelBuffer) {
  std::shared_ptr<EAGLHardwareTexture> glTexture = nullptr;
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, pixelBuffer);
  glTexture = std::static_pointer_cast<EAGLHardwareTexture>(
      context->resourceCache()->getRecycled(recycleKey));
  if (glTexture) {
    return glTexture;
  }
  auto eaglDevice = static_cast<EAGLDevice*>(context->device());
  if (eaglDevice == nullptr) {
    return nullptr;
  }

  auto format = CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_OneComponent8
                    ? PixelFormat::ALPHA_8
                    : PixelFormat::BGRA_8888;
  auto texture = GetTextureRef(context, pixelBuffer, format, eaglDevice->getTextureCache());
  if (texture == nil) {
    return nullptr;
  }
  GLSampler glInfo = {};
  glInfo.format = format;
  glInfo.target = CVOpenGLESTextureGetTarget(texture);
  glInfo.id = CVOpenGLESTextureGetName(texture);
  glTexture = Resource::Wrap(context, new EAGLHardwareTexture(pixelBuffer));
  glTexture->sampler = glInfo;
  glTexture->texture = texture;
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
}

void EAGLHardwareTexture::computeRecycleKey(BytesKey* recycleKey) const {
  ComputeRecycleKey(recycleKey, pixelBuffer);
}

size_t EAGLHardwareTexture::memoryUsage() const {
  return CVPixelBufferGetDataSize(pixelBuffer);
}

void EAGLHardwareTexture::onReleaseGPU() {
  if (texture == nil) {
    return;
  }
  static_cast<EAGLDevice*>(context->device())->releaseTexture(texture);
  texture = nil;
}

bool EAGLHardwareTexture::readPixels(const ImageInfo &dstInfo, void *dstPixels, int srcX, int srcY) const {
    if (dstPixels == nullptr) {
        return false;
    }
    dstPixels = dstInfo.computeOffset(dstPixels, -srcX, -srcY);
    auto outInfo = dstInfo.makeIntersect(-srcX, -srcY, width(), height());
    if (outInfo.isEmpty()) {
      return false;
    }
    auto srcInfo = GetImageInfo(pixelBuffer);
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void* baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    Bitmap bitmap(srcInfo, baseAddress);
    bool result = bitmap.readPixels(dstInfo, dstPixels);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    return result;
}
}  // namespace tgfx
