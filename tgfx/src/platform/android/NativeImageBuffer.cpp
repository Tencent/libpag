/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <android/bitmap.h>
#include "NativeImageInfo.h"
#include "tgfx/core/PixelBuffer.h"
#include "tgfx/core/Pixmap.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> ImageBuffer::MakeFrom(NativeImageRef nativeImage) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto info = NativeImageInfo::GetInfo(env, nativeImage);
  if (info.isEmpty()) {
    env->ExceptionClear();
    return nullptr;
  }
  auto image =
      std::shared_ptr<NativeImageBuffer>(new NativeImageBuffer(info.width(), info.height()));
  image->nativeImage = nativeImage;
  return image;
}

NativeImageBuffer::NativeImageBuffer(int width, int height) : _width(width), _height(height) {
}

std::shared_ptr<Texture> NativeImageBuffer::onMakeTexture(Context* context, bool mipMapped) const {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto info = NativeImageInfo::GetInfo(env, nativeImage.get());
  if (info.isEmpty()) {
    env->ExceptionClear();
    return nullptr;
  }
  Bitmap bitmap(_width, _height, false, !mipMapped);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, nativeImage.get(), &pixels) != 0) {
    env->ExceptionClear();
    return nullptr;
  }
  Pixmap pixmap(bitmap);
  auto result = pixmap.writePixels(info, pixels);
  pixmap.reset();  // unlock the pixelBuffer.
  AndroidBitmap_unlockPixels(env, nativeImage.get());
  if (!result) {
    return nullptr;
  }
  auto imageBuffer = bitmap.makeBuffer();
  return imageBuffer->makeTexture(context);
}

}  // namespace tgfx
