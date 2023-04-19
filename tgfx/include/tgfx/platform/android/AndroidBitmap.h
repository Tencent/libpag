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

#pragma once

#include <jni.h>
#include "tgfx/core/Bitmap.h"

namespace tgfx {
/**
 * AndroidBitmap defines method to wrap an android Bitmap into a tgfx::Bitmap.
 */
class AndroidBitmap {
 public:
  /**
   * Wraps an Android Bitmap into a tgfx::Bitmap object. The returned tgfx::Bitmap shares the same
   * pixel buffer as the Android Bitmap. Returns an empty tgfx::Bitmap if failed. The Android Bitmap
   * must have a premultiplied alpha type, and the pixel config must be either ARGB_8888, ALPHA_8,
   * or HARDWARE(API Level >= 30). Otherwise, the returned tgfx::Bitmap will be empty.
   */
  static Bitmap Wrap(JNIEnv* env, jobject bitmap);
};
}  // namespace tgfx
