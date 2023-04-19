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
#include "HardwareBuffer.h"
#include "NativePixelBuffer.h"
#include "core/PixelRef.h"

namespace tgfx {
Bitmap AndroidBitmap::Wrap(JNIEnv* env, jobject bitmap) {
  auto buffer = HardwareBuffer::MakeFrom(env, bitmap);
  if (buffer == nullptr) {
    buffer = NativePixelBuffer::MakeFrom(env, bitmap);
  }
  if (buffer == nullptr) {
    return {};
  }
  auto pixelRef = PixelRef::Wrap(std::move(buffer));
  return Bitmap(std::move(pixelRef));
}
}  // namespace tgfx