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

#include "JNIUtil.h"
#include "core/PixelBuffer.h"

namespace tgfx {
class NativePixelBuffer : public PixelBuffer {
 public:
  /**
   * Creates a PixelBuffer from the specified Android Bitmap object. Returns nullptr if the bitmap
   * is null, has an unpremultiply alpha type, or its color type is neither RGBA_8888 nor ALPHA_8.
   */
  static std::shared_ptr<PixelBuffer> MakeFrom(JNIEnv* env, jobject bitmap);

  bool isHardwareBacked() const override {
    return false;
  }

  void* lockPixels() override;

  void unlockPixels() override;

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override;

 private:
  Global<jobject> bitmap = {};

  explicit NativePixelBuffer(const ImageInfo& info) : PixelBuffer(info) {
  }
};

}  // namespace tgfx
