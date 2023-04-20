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
 * AndroidBitmap provides a utility to access an Android Bitmap object.
 */
class AndroidBitmap {
 public:
  /**
   * Returns an ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * specified Android Bitmap object. Returns an empty ImageInfo if the config of the Android Bitmap
   * is either 'ARGB_4444' or 'HARDWARE'.
   */
  static ImageInfo GetInfo(JNIEnv* env, jobject bitmap);

  /**
   * Wraps an hardware backed Android Bitmap into a tgfx::Bitmap object. The returned tgfx::Bitmap
   * shares the same pixel memory as the original Android Bitmap. Returns an empty tgfx::Bitmap if
   * the config of the Android Bitmap is not 'HARDWARE' or the API Level of the current system is
   * less than 30.
   */
  static Bitmap WrapHardware(JNIEnv* env, jobject bitmap);
};
}  // namespace tgfx
