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

#include "NativeImageBuffer.h"
#include "BitmapContextUtil.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> ImageBuffer::MakeFromNativeImage(NativeImageRef nativeImage) {
  if (nativeImage == nullptr) {
    return nullptr;
  }
  auto width = CGImageGetWidth(nativeImage);
  auto height = CGImageGetHeight(nativeImage);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<ImageBuffer>(new NativeImageBuffer(width, height, nativeImage));
}

NativeImageBuffer::NativeImageBuffer(int width, int height, CGImageRef nativeImage)
    : _width(width), _height(height), nativeImage(nativeImage) {
  CFRetain(nativeImage);
}

NativeImageBuffer::~NativeImageBuffer() {
  CFRelease(nativeImage);
}

std::shared_ptr<Texture> NativeImageBuffer::onMakeTexture(Context* context, bool mipMapped) const {
  auto pixelBuffer = PixelBuffer::Make(_width, _height, false, !mipMapped);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  auto dstPixels = pixelBuffer->lockPixels();
  auto cgContext = CreateBitmapContext(pixelBuffer->info(), dstPixels);
  if (cgContext == nullptr) {
    pixelBuffer->unlockPixels();
    return nullptr;
  }
  CGRect rect = CGRectMake(0, 0, _width, _height);
  CGContextSetBlendMode(cgContext, kCGBlendModeCopy);
  CGContextDrawImage(cgContext, rect, nativeImage);
  CGContextRelease(cgContext);
  pixelBuffer->unlockPixels();
  return pixelBuffer->makeTexture(context, mipMapped);
}

}  // namespace tgfx
