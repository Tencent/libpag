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

#include "tgfx/platform/android/AndroidBitmap.h"
#include <android/bitmap.h>
#include "AHardwareBufferFunctions.h"

namespace tgfx {
static constexpr int BITMAP_FLAGS_ALPHA_UNPREMUL = 2;
static constexpr int BITMAP_FORMAT_RGBA_F16 = 9;
static constexpr int BITMAP_FORMAT_RGBA_1010102 = 10;

ImageInfo AndroidBitmap::GetInfo(JNIEnv* env, jobject bitmap) {
  if (env == nullptr || bitmap == nullptr) {
    return {};
  }
  AndroidBitmapInfo bitmapInfo = {};
  if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) != 0) {
    env->ExceptionClear();
    return {};
  }
  AlphaType alphaType = (bitmapInfo.flags & BITMAP_FLAGS_ALPHA_UNPREMUL)
                            ? AlphaType::Unpremultiplied
                            : AlphaType::Premultiplied;
  ColorType colorType;
  switch (bitmapInfo.format) {
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
      colorType = ColorType::RGBA_8888;
      break;
    case ANDROID_BITMAP_FORMAT_A_8:
      colorType = ColorType::ALPHA_8;
      break;
    case ANDROID_BITMAP_FORMAT_RGB_565:
      colorType = ColorType::RGB_565;
      break;
    case BITMAP_FORMAT_RGBA_F16:
      colorType = ColorType::RGBA_F16;
      break;
    case BITMAP_FORMAT_RGBA_1010102:
      colorType = ColorType::RGBA_1010102;
      break;
    default:
      colorType = ColorType::Unknown;
      break;
  }
  return ImageInfo::Make(static_cast<int>(bitmapInfo.width), static_cast<int>(bitmapInfo.height),
                         colorType, alphaType, bitmapInfo.stride);
}

HardwareBufferRef AndroidBitmap::GetHardwareBuffer(JNIEnv* env, jobject bitmap) {
  static const auto fromBitmap = AHardwareBufferFunctions::Get()->fromBitmap;
  static const auto release = AHardwareBufferFunctions::Get()->release;
  if (fromBitmap == nullptr || release == nullptr || bitmap == nullptr) {
    return nullptr;
  }
  AHardwareBuffer* hardwareBuffer = nullptr;
  fromBitmap(env, bitmap, &hardwareBuffer);
  if (hardwareBuffer != nullptr) {
    // The hardware buffer returned by AndroidBitmap_getHardwareBuffer() has a reference count of 1.
    // We decrease it to keep the behaviour the same as AHardwareBuffer_fromHardwareBuffer().
    release(hardwareBuffer);
  }
  return hardwareBuffer;
}
}  // namespace tgfx